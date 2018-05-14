// Copyright 1986-2017 Xilinx, Inc. All Rights Reserved.
// --------------------------------------------------------------------------------
// Tool Version: Vivado v.2017.1 (win64) Build 1846317 Fri Apr 14 18:55:03 MDT 2017
// Date        : Mon May  7 19:05:17 2018
// Host        : U106032 running 64-bit Service Pack 1  (build 7601)
// Command     : write_verilog -force -mode synth_stub
//               c:/Users/Docencia/Desktop/Zynq_C/Zynq_C.srcs/sources_1/bd/system/ip/system_xlconstant_0_0/system_xlconstant_0_0_stub.v
// Design      : system_xlconstant_0_0
// Purpose     : Stub declaration of top-level module interface
// Device      : xc7z020clg484-1
// --------------------------------------------------------------------------------

// This empty module with port declaration file causes synthesis tools to infer a black box for IP.
// The synthesis directives are for Synopsys Synplify support to prevent IO buffer insertion.
// Please paste the declaration into a Verilog source file or add the file as an additional source.
(* X_CORE_INFO = "xlconstant_v1_1_3_xlconstant,Vivado 2017.1" *)
module system_xlconstant_0_0(dout)
/* synthesis syn_black_box black_box_pad_pin="dout[1:0]" */;
  output [1:0]dout;
endmodule
