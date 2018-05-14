// Copyright 1986-2017 Xilinx, Inc. All Rights Reserved.
// --------------------------------------------------------------------------------
// Tool Version: Vivado v.2017.1 (win64) Build 1846317 Fri Apr 14 18:55:03 MDT 2017
// Date        : Mon May  7 19:03:11 2018
// Host        : U106032 running 64-bit Service Pack 1  (build 7601)
// Command     : write_verilog -force -mode synth_stub
//               c:/Users/Docencia/Desktop/Zynq_C/Zynq_C.srcs/sources_1/bd/system/ip/system_nco_0_0/system_nco_0_0_stub.v
// Design      : system_nco_0_0
// Purpose     : Stub declaration of top-level module interface
// Device      : xc7z020clg484-1
// --------------------------------------------------------------------------------

// This empty module with port declaration file causes synthesis tools to infer a black box for IP.
// The synthesis directives are for Synopsys Synplify support to prevent IO buffer insertion.
// Please paste the declaration into a Verilog source file or add the file as an additional source.
(* X_CORE_INFO = "nco_top,Vivado 2017.1" *)
module system_nco_0_0(s_axi_AXI4LiteS_AWADDR, 
  s_axi_AXI4LiteS_AWVALID, s_axi_AXI4LiteS_AWREADY, s_axi_AXI4LiteS_WDATA, 
  s_axi_AXI4LiteS_WSTRB, s_axi_AXI4LiteS_WVALID, s_axi_AXI4LiteS_WREADY, 
  s_axi_AXI4LiteS_BRESP, s_axi_AXI4LiteS_BVALID, s_axi_AXI4LiteS_BREADY, 
  s_axi_AXI4LiteS_ARADDR, s_axi_AXI4LiteS_ARVALID, s_axi_AXI4LiteS_ARREADY, 
  s_axi_AXI4LiteS_RDATA, s_axi_AXI4LiteS_RRESP, s_axi_AXI4LiteS_RVALID, 
  s_axi_AXI4LiteS_RREADY, aclk, aresetn)
/* synthesis syn_black_box black_box_pad_pin="s_axi_AXI4LiteS_AWADDR[4:0],s_axi_AXI4LiteS_AWVALID,s_axi_AXI4LiteS_AWREADY,s_axi_AXI4LiteS_WDATA[31:0],s_axi_AXI4LiteS_WSTRB[3:0],s_axi_AXI4LiteS_WVALID,s_axi_AXI4LiteS_WREADY,s_axi_AXI4LiteS_BRESP[1:0],s_axi_AXI4LiteS_BVALID,s_axi_AXI4LiteS_BREADY,s_axi_AXI4LiteS_ARADDR[4:0],s_axi_AXI4LiteS_ARVALID,s_axi_AXI4LiteS_ARREADY,s_axi_AXI4LiteS_RDATA[31:0],s_axi_AXI4LiteS_RRESP[1:0],s_axi_AXI4LiteS_RVALID,s_axi_AXI4LiteS_RREADY,aclk,aresetn" */;
  input [4:0]s_axi_AXI4LiteS_AWADDR;
  input s_axi_AXI4LiteS_AWVALID;
  output s_axi_AXI4LiteS_AWREADY;
  input [31:0]s_axi_AXI4LiteS_WDATA;
  input [3:0]s_axi_AXI4LiteS_WSTRB;
  input s_axi_AXI4LiteS_WVALID;
  output s_axi_AXI4LiteS_WREADY;
  output [1:0]s_axi_AXI4LiteS_BRESP;
  output s_axi_AXI4LiteS_BVALID;
  input s_axi_AXI4LiteS_BREADY;
  input [4:0]s_axi_AXI4LiteS_ARADDR;
  input s_axi_AXI4LiteS_ARVALID;
  output s_axi_AXI4LiteS_ARREADY;
  output [31:0]s_axi_AXI4LiteS_RDATA;
  output [1:0]s_axi_AXI4LiteS_RRESP;
  output s_axi_AXI4LiteS_RVALID;
  input s_axi_AXI4LiteS_RREADY;
  input aclk;
  input aresetn;
endmodule
