#include <stdio.h>
#include <xil_io.h>
#include <sleep.h>
#include "xiicps.h"
#include <xil_printf.h>
#include <xparameters.h>
#include "xgpio.h"
#include "xuartps.h"
#include "stdlib.h"
#include "audio.h"
#include "xnco.h"
#include "xscutimer.h"

/* Parameter definitions */
#define UART_BASEADDR XPAR_PS7_UART_1_BASEADDR
#define GPIO_BASE     XPAR_GPIO_0_BASEADDR
#define GPIO_0_ID     XPAR_GPIO_0_DEVICE_ID
#define GPIO_1_ID     XPAR_GPIO_1_DEVICE_ID
#define NCO_ID        XPAR_NCO_0_DEVICE_ID

/* Define GPIO 0 Channels */
#define LED_CHANNEL 1
#define SWITCH_CHANNEL 2

/* Define GPIO 1 Channels */
#define BUTTON_CHANNEL 1

/* Button constants */
// %000URLDC
#define BTN_U 0b00010000
#define BTN_R 0b00001000
#define BTN_L 0b00000100
#define BTN_D 0b00000010
#define BTN_C 0b00000001

/* Switch constants */
#define S0 (1 << 0)
#define S1 (1 << 1)
#define S2 (1 << 2)
#define S3 (1 << 3)
#define S4 (1 << 4)
#define S5 (1 << 5)
#define S6 (1 << 6)
#define S7 (1 << 7)

/* 48 kHz Timer */
#define TIMER 6771

/* DDR */
#define DDR_START (XPAR_PS7_DDR_0_S_AXI_BASEADDR + 0x00100000)
#define DDR_END (XPAR_PS7_DDR_0_S_AXI_BASEADDR)
#define DDR_SIZE (DDR_END - DDR_START)
#define MAX_RECORD_SAMPLES (DDR_SIZE / 4)

/* Other */
#define INITIAL_VOLUME 0xFE
#define MODE_PLAY_AUDIO S0
#define MODE_PLAY_RECORDED S1
#define MODE_RECORD_AUDIO S7

/* Prototype Functions */
unsigned char gpio_init();
void nco_init(void*);
void timer_init();
void read_play();
unsigned char IicConfig(unsigned int DeviceIdPS);
void AudioPllConfig();
void AudioWriteToReg(unsigned char, unsigned char);
void AudioReadFromReg(unsigned char, unsigned char);
void AudioConfigureJacks();
void LineinLineoutConfig();

/* Global variables */
XIicPs Iic;
XGpio Gpio0; /* Gpio instance for led and switches */
XGpio Gpio1; /* Gpio instance for buttons */
XNco Nco;
XScuTimer Timer; /* Cortex A9 SCU Private Timer Instance */
XScuTimer *TimerInstancePtr;
unsigned char volume = INITIAL_VOLUME;


int main(void)
{
	xil_printf("Lab8-Zynq Soc Training-ADC/DAC and Digital Audio Processing\r\n");
	xil_printf("EmbeddedCentric.com\r\n");

	/* Configure the IIC data structure */
	IicConfig(XPAR_XIICPS_0_DEVICE_ID);

	/* Configure the Audio Codec's PLL */
	AudioPllConfig();

	/* Configure the Line in and Line out ports */
	// Call LineInLineOutConfig() for a configuration that enables the HP jack too.
	AudioConfigureJacks();
	xil_printf("ADAU1761 configured\n\r");

	/* Initialize GPIO and NCO controller */
	nco_init(&Nco);
	gpio_init();
	xil_printf("GPIO and NCO peripheral configured\r\n");

	/* Initialize 48 kHz timer */
	timer_init();

	/* Infinite loop of: Read Audio from the ADC, and send it to the DAC */
	read_play();

	// Never reached on normal execution
    return 1;
}


/* ---------------------------------------------------------------------------- *
 *                               gpio_init()                                    *
 * ---------------------------------------------------------------------------- *
 * initializes the GPIO driver for the led buttons and switches.
 * ---------------------------------------------------------------------------- */
unsigned char gpio_init()
{
	int Status;

	Status = XGpio_Initialize(&Gpio0, GPIO_0_ID);
	if(Status != XST_SUCCESS) return XST_FAILURE;

	Status = XGpio_Initialize(&Gpio1, GPIO_1_ID);
	if(Status != XST_SUCCESS) return XST_FAILURE;

	XGpio_SetDataDirection(&Gpio0, SWITCH_CHANNEL, 0xFF);
	XGpio_SetDataDirection(&Gpio0, LED_CHANNEL,    0x00);
	XGpio_SetDataDirection(&Gpio1, BUTTON_CHANNEL, 0xFF);

	return XST_SUCCESS;
}

/* ---------------------------------------------------------------------------- *
 *                               nco_init()                                     *
 * ---------------------------------------------------------------------------- *
 * initializes the Numerically Controlled Oscillator driver so that it's ready
 * to use.
 * ---------------------------------------------------------------------------- */
void nco_init(void *InstancePtr)
{
	XNco_Config *cfgPtr;
	int status;

	// Look up the configuration in the config table
	cfgPtr = XNco_LookupConfig(NCO_ID);
	if (!cfgPtr) {
		print("ERROR: Lookup of NCO configuration failed.\n\r");
	}

	// Initialise the NCO driver configuration
	status = XNco_CfgInitialize(InstancePtr, cfgPtr);
	if (status != XST_SUCCESS) {
		print("ERROR: Could not initialise NCO.\n\r");
	}
}

/* ---------------------------------------------------------------------------- *
 *                                   timer_init()                               *
 * ---------------------------------------------------------------------------- *
 * initializes 48 kHz timer
 * ---------------------------------------------------------------------------- */
void timer_init()
{
	XScuTimer_Config *ConfigPtr;
	TimerInstancePtr = &Timer;
	int status;

	// Initialize the timer
	ConfigPtr = XScuTimer_LookupConfig (XPAR_PS7_SCUTIMER_0_DEVICE_ID);
	status = XScuTimer_CfgInitialize (TimerInstancePtr, ConfigPtr, ConfigPtr->BaseAddr);
	if (status != XST_SUCCESS) {
		xil_printf("Timer init() failed. \r\n");
	}

	// Set AutoLoad mode
	XScuTimer_EnableAutoReload(TimerInstancePtr);
	// Start the timer
	XScuTimer_Start(TimerInstancePtr);
}

/* ---------------------------------------------------------------------------- *
 *                                    read_play()                               *
 * ---------------------------------------------------------------------------- *
 * - Si S0 -> Reproducir audio que entra [Se enciende L0].
 * - Si S1 -> Reproducir audio grabado hasta que acabe o hasta que se cierre S1
 * [Se enciende L1 mientras reproduce].
 * - Si S7 -> Grabar audio de entrada (cerrar S7 para que pare de grabar,
 * sino sigue grabando hasta que se llene el buffer) [Se enciende L7 mientras graba].
 * - Si ningún switch o mas de un switch -> No hacer nada [No se enciende ningun led].
 * ---------------------------------------------------------------------------- */
void read_play()
{
	u32 mode;
	u32 sws, btns, cur_btns;
	u32 in_left, in_right, out_left, out_right;
	u32* buffer;
	int bufr_pos;
	int bufw_pos;
	int num_recorded_samples_lr;

	/* Init record buffer */
	buffer = (u32*) DDR_START;
	bufr_pos = 0;
	bufw_pos = 0;
	num_recorded_samples_lr = 0;

	while (1) {

		/* Read value from DIP switches */
		sws = XGpio_DiscreteRead(&Gpio0, SWITCH_CHANNEL);

		if (sws == MODE_PLAY_AUDIO) {
			mode = MODE_PLAY_AUDIO;
			bufr_pos = 0;
			bufw_pos = 0;
		} else if (sws == MODE_RECORD_AUDIO) {
			mode = MODE_RECORD_AUDIO;
			bufw_pos = 0;
		} else if (sws == MODE_PLAY_RECORDED) {
			mode = MODE_PLAY_RECORDED;
			bufr_pos = 0;
		} else {
			mode = 0;
			bufr_pos = 0;
			bufw_pos = 0;
		}

		/* Write value to the LEDs */
		XGpio_DiscreteWrite(&Gpio0, LED_CHANNEL, mode);

		/* Read buttons */
		btns = XGpio_DiscreteRead(&Gpio1, BUTTON_CHANNEL);

		// AudioReadFromReg(R3X_, volume);
		// BTN_U --> volume up
		if ((btns & BTN_U) != 0) {
			if ((volume & 0b11111100) != 0b11111100) {
				volume += (1 << 2);
				AudioWriteToReg(R31_PLAYBACK_LINE_OUTPUT_LEFT_VOLUME_CONTROL, volume);
				AudioWriteToReg(R32_PLAYBACK_LINE_OUTPUT_RIGHT_VOLUME_CONTROL, volume);
			}
		}

		// BTN_D --> volume down
		if ((btns & BTN_D) != 0) {
			if ((volume & 0b11111100) != 0) {
				volume -= (1 << 2);
				AudioWriteToReg(R31_PLAYBACK_LINE_OUTPUT_LEFT_VOLUME_CONTROL, volume);
				AudioWriteToReg(R32_PLAYBACK_LINE_OUTPUT_RIGHT_VOLUME_CONTROL, volume);
			}
		}

		while (!XUartPs_IsReceiveData(UART_BASEADDR)) {

			/* Wait for next audio sample */
			while (1) {
				if (XScuTimer_IsExpired(TimerInstancePtr)) {
					XScuTimer_ClearInterruptStatus(TimerInstancePtr);
					break;
				}
			}

			/* Sample L+R audio from the codec */
			in_left = Xil_In32(I2S_DATA_RX_L_REG);
			in_right = Xil_In32(I2S_DATA_RX_R_REG);

			/* Process current audio sample */
			switch (mode) {

				case MODE_PLAY_AUDIO:
					out_left = in_left;
					out_right = in_right;
					break;

				case MODE_RECORD_AUDIO:
					if (bufr_pos == 0) {
						num_recorded_samples_lr = 0;
						memset(buffer, 0, DDR_SIZE);
					}
					if (bufr_pos >= MAX_RECORD_SAMPLES) {
						mode = 0;
					} else {
						buffer[bufr_pos] = in_left;
						bufr_pos ++;
						buffer[bufr_pos] = in_right;
						bufr_pos ++;
						num_recorded_samples_lr ++;
					}
					out_left = 0;
					out_right = 0;
					break;

				case MODE_PLAY_RECORDED:
					if (bufw_pos >= (num_recorded_samples_lr * 2)) {
						mode = 0;
						out_left = 0;
						out_right = 0;
					} else {
						out_left = buffer[bufw_pos];
						bufw_pos ++;
						out_right = buffer[bufw_pos];
						bufw_pos ++;
					}
					break;

				default:
					out_left = 0;
					out_right = 0;
					break;
			}

			/* Output audio to the codec */
			Xil_Out32(I2S_DATA_TX_L_REG, out_left);
			Xil_Out32(I2S_DATA_TX_R_REG, out_right);

			/* Break if switches changed */
			if (sws != XGpio_DiscreteRead(&Gpio0, SWITCH_CHANNEL)) {
				break;
			}

			/* Break if buttons pressed changed */
			cur_btns = XGpio_DiscreteRead(&Gpio1, BUTTON_CHANNEL);
			if (((btns & BTN_C) == 0) && ((cur_btns & BTN_C) != 0))
				break;
			if (((btns & BTN_D) == 0) && ((cur_btns & BTN_D) != 0))
				break;
			if (((btns & BTN_U) == 0) && ((cur_btns & BTN_U) != 0))
				break;

			/* Update released buttons */
			btns = cur_btns;
		}

	}
	read_play();
}

/* ---------------------------------------------------------------------------- *
 *                               IicConfig()                                    *
 * ---------------------------------------------------------------------------- *
 * Initialises the IIC driver by looking up the configuration in the config
 * table and then initialising it. Also sets the IIC serial clock rate.
 * ---------------------------------------------------------------------------- */
unsigned char IicConfig(unsigned int DeviceIdPS)
{
	XIicPs_Config *Config;
	int Status;

	/* Initialise the IIC driver so that it's ready to use */

	// Look up the configuration in the config table
	Config = XIicPs_LookupConfig(DeviceIdPS);
	if(NULL == Config) {
		return XST_FAILURE;
	}

	// Initialise the IIC driver configuration
	Status = XIicPs_CfgInitialize(&Iic, Config, Config->BaseAddress);
	if(Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	//Set the IIC serial clock rate.
	XIicPs_SetSClk(&Iic, IIC_SCLK_RATE);

	return XST_SUCCESS;
}

/* ---------------------------------------------------------------------------- *
 *                              AudioPllConfig()                                *
 * ---------------------------------------------------------------------------- *
 * Configures audio codes's internal PLL. With MCLK = 10 MHz it configures the
 * PLL for a VCO frequency = 49.152 MHz, and an audio sample rate of 48 KHz.
 *
 * El FCLK_CLK1 (mismo valor que MCLK) que le entra al ADAU de la PS está configurado a 10 MHz
 * Con el bit CLKSRC (bit 3) de R0 se configura que se la señal de reloj (Core Clock) de genera de la PLL.
 *
 * Para tener una fs en los ADC de 48 kHz:
 * El clock rate de la PLL es de 1024*fs = 49152 MHz.
 * Con el valor de 49152 MHz, hay que calcular M, N, y R y llevarlos a R1 (PLL Control Register).
 * PLL/MCLK = 4.9152 = R + (N/M).
 *
 * Primero se deshabilita el Core Clock (R0.COREN a 0), luego se configura el registro R2 de la PLL,
 * se espera ha que se enganche el PLL a la frecuencia (R2 @ PLL Lock bit), y luego se pone COREN a 1.
 * ---------------------------------------------------------------------------- */
void AudioPllConfig()
{
	unsigned char u8TxData[8], u8RxData[6];
	int Status;

	Status = IicConfig(XPAR_XIICPS_0_DEVICE_ID);
	if(Status != XST_SUCCESS) {
		xil_printf("\nError initializing IIC");

	}

	// Disable Core Clock
	AudioWriteToReg(R0_CLOCK_CONTROL, 0x0E);

	/* 	MCLK = 10 MHz
		R = 0100 = 4, N = 0x02 0x3C = 572, M = 0x02 0x71 = 625

		PLL required output = 1024x48 KHz
		(PLLout)			= 49.152 MHz

		PLLout/MCLK			= 49.152 MHz/10 MHz
							= 4.9152
							= R + (N/M)
							= 4 + (572/625) */

	// Write 6 bytes to R1 @ register address 0x4002
	u8TxData[0] = 0x40; // Register write address [15:8]
	u8TxData[1] = 0x02; // Register write address [7:0]
	u8TxData[2] = 0x02; // byte 6 - M[15:8]
	u8TxData[3] = 0x71; // byte 5 - M[7:0]
	u8TxData[4] = 0x02; // byte 4 - N[15:8]
	u8TxData[5] = 0x3C; // byte 3 - N[7:0]
	u8TxData[6] = 0x21; // byte 2 - 7 = reserved, bits 6:3 = R[3:0], 2:1 = X[1:0], 0 = PLL operation mode
	u8TxData[7] = 0x01; // byte 1 - 7:2 = reserved, 1 = PLL Lock, 0 = Core clock enable

	// Write bytes to PLL Control register R1 @ 0x4002
	XIicPs_MasterSendPolled(&Iic, u8TxData, 8, (IIC_SLAVE_ADDR >> 1));
	while(XIicPs_BusIsBusy(&Iic));

	// Register address set: 0x4002
	u8TxData[0] = 0x40;
	u8TxData[1] = 0x02;

	// Poll PLL Lock bit
	do {
		XIicPs_MasterSendPolled(&Iic, u8TxData, 2, (IIC_SLAVE_ADDR >> 1));
		while(XIicPs_BusIsBusy(&Iic));
		XIicPs_MasterRecvPolled(&Iic, u8RxData, 6, (IIC_SLAVE_ADDR >> 1));
		while(XIicPs_BusIsBusy(&Iic));
	}
	while((u8RxData[5] & 0x02) == 0); // while not locked

	AudioWriteToReg(R0_CLOCK_CONTROL, 0x0F);	// 1111
												// bit 3:		CLKSRC = PLL Clock input
												// bits 2:1:	INFREQ = 1024 x fs
												// bit 0:		COREN = Core Clock enabled
}

/* ---------------------------------------------------------------------------- *
 *                            AudioWriteToReg                                   *
 * ---------------------------------------------------------------------------- *
 * Function to write one byte (8-bits) to one of the registers from the audio
 * controller.
 * ---------------------------------------------------------------------------- */
void AudioWriteToReg(unsigned char u8RegAddr, unsigned char u8Data)
{
	unsigned char u8TxData[3];

	u8TxData[0] = 0x40;
	u8TxData[1] = u8RegAddr;
	u8TxData[2] = u8Data;

	XIicPs_MasterSendPolled(&Iic, u8TxData, 3, (IIC_SLAVE_ADDR >> 1));
	while(XIicPs_BusIsBusy(&Iic));
}

/* ---------------------------------------------------------------------------- *
 *                           AudioReadFromReg                                   *
 * ---------------------------------------------------------------------------- *
 * Function to read one byte (8-bits) from one of the registers from the audio
 * controller.
 * ---------------------------------------------------------------------------- */
void AudioReadFromReg(unsigned char u8RegAddr, unsigned char u8Data)
{
	unsigned char u8RxData[3];

	u8RxData[0] = 0x40;
	u8RxData[1] = u8RegAddr;
	u8RxData[2] = u8Data;

	XIicPs_MasterRecvPolled(&Iic, u8RxData, 3, (IIC_SLAVE_ADDR >> 1));
	while(XIicPs_BusIsBusy(&Iic));
}

/* ---------------------------------------------------------------------------- *
 *                             AudioConfigureJacks()                            *
 * ---------------------------------------------------------------------------- *
 * Configures audio codes's various mixers, ADC's, DAC's, and amplifiers to
 * accept stereo input from line in and push stereo output to line out.
 * ---------------------------------------------------------------------------- */
void AudioConfigureJacks()
{
	AudioWriteToReg(R4_RECORD_MIXER_LEFT_CONTROL_0, 0x01); //enable mixer 1
	AudioWriteToReg(R5_RECORD_MIXER_LEFT_CONTROL_1, 0x07); //unmute Left channel of line in into mxr 1 and set gain to 6 db
	AudioWriteToReg(R6_RECORD_MIXER_RIGHT_CONTROL_0, 0x01); //enable mixer 2
	AudioWriteToReg(R7_RECORD_MIXER_RIGHT_CONTROL_1, 0x07); //unmute Right channel of line in into mxr 2 and set gain to 6 db
	AudioWriteToReg(R19_ADC_CONTROL, 0x13); //enable ADCs

	AudioWriteToReg(R22_PLAYBACK_MIXER_LEFT_CONTROL_0, 0x21); //unmute Left DAC into Mxr 3; enable mxr 3
	AudioWriteToReg(R24_PLAYBACK_MIXER_RIGHT_CONTROL_0, 0x41); //unmute Right DAC into Mxr4; enable mxr 4
	AudioWriteToReg(R26_PLAYBACK_LR_MIXER_LEFT_LINE_OUTPUT_CONTROL, 0x05); //unmute Mxr3 into Mxr5 and set gain to 6db; enable mxr 5
	AudioWriteToReg(R27_PLAYBACK_LR_MIXER_RIGHT_LINE_OUTPUT_CONTROL, 0x11); //unmute Mxr4 into Mxr6 and set gain to 6db; enable mxr 6
	AudioWriteToReg(R29_PLAYBACK_HEADPHONE_LEFT_VOLUME_CONTROL, 0xFF);//Mute Left channel of HP port (LHP)
	AudioWriteToReg(R30_PLAYBACK_HEADPHONE_RIGHT_VOLUME_CONTROL, 0xFF); //Mute Right channel of HP port (LHP)
	AudioWriteToReg(R31_PLAYBACK_LINE_OUTPUT_LEFT_VOLUME_CONTROL, INITIAL_VOLUME); //set LOUT volume (0db); unmute left channel of Line out port; set Line out port to line out mode
	AudioWriteToReg(R32_PLAYBACK_LINE_OUTPUT_RIGHT_VOLUME_CONTROL, INITIAL_VOLUME); // set ROUT volume (0db); unmute right channel of Line out port; set Line out port to line out mode
	AudioWriteToReg(R35_PLAYBACK_POWER_MANAGEMENT, 0x03); //enable left and right channel playback (not sure exactly what this does...)
	AudioWriteToReg(R36_DAC_CONTROL_0, 0x03); //enable both DACs

	AudioWriteToReg(R58_SERIAL_INPUT_ROUTE_CONTROL, 0x01); //Connect I2S serial port output (SDATA_O) to DACs
	AudioWriteToReg(R59_SERIAL_OUTPUT_ROUTE_CONTROL, 0x01); //connect I2S serial port input (SDATA_I) to ADCs

	AudioWriteToReg(R65_CLOCK_ENABLE_0, 0x7F); //Enable clocks
	AudioWriteToReg(R66_CLOCK_ENABLE_1, 0x03); //Enable rest of clocks
}

/* ---------------------------------------------------------------------------- *
 * 	                           LineinLineoutConfig()                            *
 * ---------------------------------------------------------------------------- *
 * Configures Line-In input, ADC's, DAC's, Line-Out and HP-Out.
 * ---------------------------------------------------------------------------- */
void LineinLineoutConfig()
{
	AudioWriteToReg(R17_CONVERTER_CONTROL_0, 0x05);//48 KHz
	AudioWriteToReg(R64_SERIAL_PORT_SAMPLING_RATE, 0x05);//48 KHz
	AudioWriteToReg(R19_ADC_CONTROL, 0x13);
	AudioWriteToReg(R36_DAC_CONTROL_0, 0x03);
	AudioWriteToReg(R35_PLAYBACK_POWER_MANAGEMENT, 0x03);
	AudioWriteToReg(R58_SERIAL_INPUT_ROUTE_CONTROL, 0x01);
	AudioWriteToReg(R59_SERIAL_OUTPUT_ROUTE_CONTROL, 0x01);
	AudioWriteToReg(R65_CLOCK_ENABLE_0, 0x7F);
	AudioWriteToReg(R66_CLOCK_ENABLE_1, 0x03);

	AudioWriteToReg(R4_RECORD_MIXER_LEFT_CONTROL_0, 0x01);
	AudioWriteToReg(R5_RECORD_MIXER_LEFT_CONTROL_1, 0x05);//0 dB gain
	AudioWriteToReg(R6_RECORD_MIXER_RIGHT_CONTROL_0, 0x01);
	AudioWriteToReg(R7_RECORD_MIXER_RIGHT_CONTROL_1, 0x05);//0 dB gain

	AudioWriteToReg(R22_PLAYBACK_MIXER_LEFT_CONTROL_0, 0x21);
	AudioWriteToReg(R24_PLAYBACK_MIXER_RIGHT_CONTROL_0, 0x41);
	AudioWriteToReg(R26_PLAYBACK_LR_MIXER_LEFT_LINE_OUTPUT_CONTROL, 0x03);//0 dB
	AudioWriteToReg(R27_PLAYBACK_LR_MIXER_RIGHT_LINE_OUTPUT_CONTROL, 0x09);//0 dB
	AudioWriteToReg(R29_PLAYBACK_HEADPHONE_LEFT_VOLUME_CONTROL, 0xE7);//0 dB
	AudioWriteToReg(R30_PLAYBACK_HEADPHONE_RIGHT_VOLUME_CONTROL, 0xE7);//0 dB
	AudioWriteToReg(R31_PLAYBACK_LINE_OUTPUT_LEFT_VOLUME_CONTROL, 0xE6);//0 dB
	AudioWriteToReg(R32_PLAYBACK_LINE_OUTPUT_RIGHT_VOLUME_CONTROL, 0xE6);//0 dB
}



