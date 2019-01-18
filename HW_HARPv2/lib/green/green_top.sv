// ***************************************************************************
//
//        Copyright (C) 2008-2016 Intel Corporation All Rights Reserved.
//
// Module Name :	green top
// Project :            BDW + FPGA 
// Description :        This module instantiates CCI-P compliant AFU and 
//                      Debug modules remote signal Tap feature.
//                      User AFUs should be instantiated in ccip_std_afu.sv
//                      ----- DO NOT MODIFY THIS FILE -----
// ***************************************************************************
import ccip_if_pkg::*;
parameter CCIP_TXPORT_WIDTH = $bits(t_if_ccip_Tx);
parameter CCIP_RXPORT_WIDTH = $bits(t_if_ccip_Rx);
module green_top(
  // CCI-P Clocks and Resets
  input           logic             pClk,              // 400MHz - CCI-P clock domain. Primary interface clock
  input           logic             pClkDiv2,          // 200MHz - CCI-P clock domain.
  input           logic             pClkDiv4,          // 100MHz - CCI-P clock domain.
  input           logic             uClk_usr,          // User clock domain. Refer to clock programming guide  ** Currently provides fixed 272.78MHz clock **
  input           logic             uClk_usrDiv2,      // User clock domain. Half the programmed frequency  ** Currently provides fixed 136.37MHz clock **
  input           logic             pck_cp2af_softReset,      // CCI-P ACTIVE HIGH Soft Reset
  input           logic [1:0]       pck_cp2af_pwrState,       // CCI-P AFU Power State
  input           logic             pck_cp2af_error,          // CCI-P Protocol Error Detected

  // Interface structures
  output          logic [CCIP_TXPORT_WIDTH-1:0] bus_ccip_Tx,         // CCI-P TX port
  input           logic [CCIP_RXPORT_WIDTH-1:0] bus_ccip_Rx,         // CCI-P RX port
  
  // JTAG interface for PR region debug
  input           logic             sr2pr_tms,
  input           logic             sr2pr_tdi,             
  output          logic             pr2sr_tdo,             
  input           logic             sr2pr_tck             
);

t_if_ccip_Tx pck_af2cp_sTx;
t_if_ccip_Rx pck_cp2af_sRx;

always_comb
begin
  bus_ccip_Tx      = pck_af2cp_sTx;
  pck_cp2af_sRx    = bus_ccip_Rx;
end


// ===========================================
// AFU - Remote Debug JTAG IP instantiation
// --- DO NOT MODIFY ----
// ===========================================

`ifdef SIMULATION_MODE
assign pr2sr_tdo = 0;

`else
wire loopback;
sld_virtual_jtag  (.tdi(loopback), .tdo(loopback));
SCJIO 
inst_SCJIO (
		.tms         (sr2pr_tms),         //        jtag.tms
		.tdi         (sr2pr_tdi),         //            .tdi
		.tdo         (pr2sr_tdo),         //            .tdo
		.tck         (sr2pr_tck)          //         tck.clk
); 
`endif 

// ===========================================
// CCI-P AFU Instantiation     
// --- DO NOT MODIFY ----
// ===========================================

ccip_std_afu 
inst_ccip_std_afu ( 
.pClk                   ( pClk),                  // 16ui link/protocol clock domain. Interface Clock
.pClkDiv2               ( pClkDiv2),              // 32ui link/protocol clock domain. Synchronous to interface clock
.pClkDiv4               ( pClkDiv4),              // 64ui link/protocol clock domain. Synchronous to interface clock
.uClk_usr               ( uClk_usr),
.uClk_usrDiv2           ( uClk_usrDiv2),
.pck_cp2af_softReset    ( pck_cp2af_softReset),
.pck_cp2af_pwrState     ( pck_cp2af_pwrState),
.pck_cp2af_error        ( pck_cp2af_error),
                        
.pck_af2cp_sTx          ( pck_af2cp_sTx),         // CCI-P Tx Port
.pck_cp2af_sRx          ( pck_cp2af_sRx)          // CCI-P Rx Port
);

endmodule
