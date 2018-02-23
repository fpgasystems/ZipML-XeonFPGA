// ***************************************************************************
//
// Copyright (c) 2013-2015, Intel Corporation
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
// * Neither the name of Intel Corporation nor the names of its contributors
// may be used to endorse or promote products derived from this software
// without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// ***************************************************************************
//
// Engineer :           Pratik Marolia
// Create Date:         Thu Jul 28 20:31:17 PDT 2011
// Last Modified :  Fri 03 Oct 2014 10:38:55 AM PDT
// Module Name:         requestor.v
// Project:             NLB AFU v1.1
//                      Compliant with CCI v2.1
// Description:         accepts requests from arbiter and formats it per cci
//                      spec. It also implements the flow control.
// ***************************************************************************
//
// The requestor accepts the address index from the arbiter, appends that to the source/destination base address and 
// sends out the request to the CCI module. It arbitrates between the read and the write requests, peforms the flow control,
// implements all the CSRs for source address, destination address, status address, wrthru enable, start and stop the test.
//
//
//
// CSR Address Map -- Change v1.1
//------------------------------------------------------------------------------------------
//      Address[15:0] Attribute         Name                    Comments
//     'h1A00          WO                CSR_AFU_DSM_BASEL       Lower 32-bits of AFU DSM base address. The lower 6-bbits are 4x00 since the address is cache aligned.
//     'h1A04          WO                CSR_AFU_DSM_BASEH       Upper 32-bits of AFU DSM base address.
//     'h1A20:         WO                CSR_SRC_ADDR            Start physical address for source buffer. All read requests are targetted to this region.
//     'h1A24:         WO                CSR_DST_ADDR            Start physical address for destination buffer. All write requests are targetted to this region.
//     'h1A28:         WO                CSR_NUM_LINES           Number of cache lines
//     'h1A2c:         WO                CSR_CTL                 Controls test flow, start, stop, force completion
//     'h1A34:         WO                CSR_CFG                 Configures test parameters
//     'h1A38:         WO                CSR_INACT_THRESH        inactivity threshold limit
//     'h1A3c          WO                CSR_INTERRUPT0          SW allocates Interrupt APIC ID & Vector to device
//     
//
// DSM Offeset Map -- Change v1.1
//------------------------------------------------------------------------------------------
//      Byte Offset   Attribute         Name                  Comments
//      0x00          RO                DSM_AFU_ID            non-zero value to uniquely identify the AFU
//      0x40          RO                DSM_STATUS            test status and error register
//

module requestor #(parameter PEND_THRESH=1, ADDR_LMT=20, TXHDR_WIDTH=61, RXHDR_WIDTH=18, DATA_WIDTH=512)
(

//      ---------------------------global signals-------------------------------------------------
    Clk_32UI               , // in    std_logic;  -- Core clock
    Resetb                 , // in    std_logic;  -- Use SPARINGLY only for control
       // ---------------------------CCI IF signals between CCI and requestor  ---------------------
    cf2ci_C0TxHdr,           // [TXHDR_WIDTH-1:0]   cci_top:         Tx hdr
    cf2ci_C0TxRdValid,       //                     cci_top:         Tx hdr is valid
    cf2ci_C1TxHdr,           // [TXHDR_WIDTH-1:0]   cci_top:         Tx hdr
    cf2ci_C1TxData,          //                     cci_top:         Tx data
    cf2ci_C1TxWrValid,       //                     cci_top:         Tx hdr is valid
    cf2ci_C1TxIntrValid,     //                     cci_top:         Tx Interrupt valid

    rb2cf_C0RxHdr,           // [TXHDR_WIDTH-1:0]   cci_rb:          Rx hdr
    rb2cf_C0RxData,          // [DATA_WIDTH-1:0]    cci_rb:          Rx data
    rb2cf_C0RxWrValid,       //                     cci_rb:          Rx hdr is valid
    rb2cf_C0RxRdValid,       //                     cci_rb:          Rx hdr is valid
    rb2cf_C0RxCfgValid,      //                     cci_rb:          Rx hdr is valid
    rb2cf_C0RxUMsgValid,     //                     cci_intf:        Rx UMsg valid
    rb2cf_C0RxIntrValid,     //                     cci_intf:        Rx interrupt valid
    rb2cf_C1RxHdr,           // [TXHDR_WIDTH-1:0]   cci_rb:          Rx hdr
    rb2cf_C1RxWrValid,       //                     cci_rb:          Rx hdr is valid
    rb2cf_C1RxIntrValid,     //                     cci_intf:        Rx interrupt valid

    ci2cf_C0TxAlmFull,       //                     cci_top:         Tx channel is almost full
    ci2cf_C1TxAlmFull,       //                     cci_top:         Tx channel is almost full
    ci2cf_InitDn,            //                     Link initialization is complete

    ab2re_WrAddr,            // [ADDR_LMT-1:0]      arbiter:        Writes are guaranteed to be accepted
    ab2re_WrTID,             // [13:0]              arbiter:        meta data
    ab2re_WrDin,             // [511:0]             arbiter:        Cache line data
    ab2re_WrFence,           //                     arbiter:        write fence.
    ab2re_WrEn,              //                     arbiter:        write enable
    re2ab_WrSent,            //                     arbiter:        can accept writes. Qualify with write enable
    re2ab_WrAlmFull,         //                     arbiter:        write fifo almost full

    ab2re_RdAddr,            // [ADDR_LMT-1:0]      arbiter:        Reads may yield to writes
    ab2re_RdTID,             // [13:0]              arbiter:        meta data
    ab2re_RdEn,              //                     arbiter:        read enable
    re2ab_RdSent,            //                     arbiter:        read issued

    re2ab_RdRspValid,        //                     arbiter:        read response valid
    re2ab_UMsgValid,         //                     arbiter:        UMsg valid
    re2ab_CfgValid,          //                     arbiter:        Cfg Valid
    re2ab_RdRsp,             // [ADDR_LMT-1:0]      arbiter:        read response header
    re2ab_RdData,            // [511:0]             arbiter:        read data
    re2ab_stallRd,           //                     arbiter:        stall read requests FOR LPBK1

    re2ab_WrRspValid,        //                     arbiter:        write response valid
    re2ab_WrRsp,             // [ADDR_LMT-1:0]      arbiter:        write response header
    re2xy_go,                //                     requestor:      start the test
    re2xy_NumLines,          // [31:0]              requestor:      number of cache lines
    re2xy_addr_reset,
    re2xy_read_offset,
    re2xy_write_offset,
    re2xy_my_config1,
    re2xy_my_config2,
    re2xy_my_config3,
    re2xy_my_config4,
    re2xy_my_config5,
    re2xy_Cont,              //                     requestor:      continuous mode
    re2xy_src_addr,          // [31:0]              requestor:      src address
    re2xy_dst_addr,          // [31:0]              requestor:      destination address
    re2xy_test_cfg,          // [7:0]               requestor:      8-bit test cfg register.
    re2ab_Mode,              // [2:0]               requestor:      test mode

    ab2re_TestCmp,           //                     arbiter:        Test completion flag
    ab2re_ErrorInfo,         // [255:0]             arbiter:        error information
    ab2re_ErrorValid,        //                     arbiter:        test has detected an error
    test_Resetb              //                     requestor:      rest the app
);
    //--------------------------------------------------------------------------------------------------------------
    
    input[RXHDR_WIDTH-1:0]  rb2cf_C0RxHdr;          //  [RXHDR_WIDTH-1:0]   cci_rb:         Rx hdr
    input[DATA_WIDTH-1:0]   rb2cf_C0RxData;         //  [DATA_WIDTH-1:0]    cci_rb:         Rx data
    input                   rb2cf_C0RxWrValid;      //                      cci_rb:         Rx hdr carries a write response
    input                   rb2cf_C0RxRdValid;      //                      cci_rb:         Rx hdr carries a read response
    input                   rb2cf_C0RxCfgValid;     //                      cci_rb:         Rx hdr carries a cfg write
    input                   rb2cf_C0RxUMsgValid;    //                      cci_intf:       Rx UMsg valid
    input                   rb2cf_C0RxIntrValid;    //                      cci_intf:       interrupt response enable
    input[RXHDR_WIDTH-1:0]  rb2cf_C1RxHdr;          //  [RXHDR_WIDTH-1:0]   cci_rb:         Rx hdr
    input                   rb2cf_C1RxWrValid;      //                      cci_rb:         Rx hdr carries a write response
    input                   rb2cf_C1RxIntrValid;    //                      cci_intf:       interrupt response enable
    input                   ci2cf_C0TxAlmFull;      //                      cci_top:        Tx channel is almost full
    input                   ci2cf_C1TxAlmFull;      //                      cci_top:        Tx channel is almost full
    input                   ci2cf_InitDn;
    input                   Clk_32UI;               //                      csi_top:        Clk_32UI
    input                   Resetb;                 //                      csi_top:        system Resetb
    
    output[TXHDR_WIDTH-1:0] cf2ci_C0TxHdr;          //   [TXHDR_WIDTH-1:0]  cci_top:        Tx hdr
    output                  cf2ci_C0TxRdValid;      //                      cci_top:        Tx hdr is valid
    output[TXHDR_WIDTH-1:0] cf2ci_C1TxHdr;          //   [TXHDR_WIDTH-1:0]  cci_top:        Tx hdr
    output[DATA_WIDTH-1:0]  cf2ci_C1TxData;         //                      cci_top:        Tx data
    output                  cf2ci_C1TxWrValid;      //                      cci_top:        Tx hdr is valid
    output                  cf2ci_C1TxIntrValid;    //                      cci_top:        Tx Interrupt valid
    
    input  [ADDR_LMT-1:0]   ab2re_WrAddr;           // [ADDR_LMT-1:0]        arbiter:       Writes are guaranteed to be accepted
    input  [13:0]           ab2re_WrTID;            // [13:0]                arbiter:       meta data
    input  [DATA_WIDTH-1:0] ab2re_WrDin;            // [511:0]               arbiter:       Cache line data
    input                   ab2re_WrFence;          //                       arbiter:       write fence 
    input                   ab2re_WrEn;             //                       arbiter:       write enable
    output                  re2ab_WrSent;           //                       arbiter:       write issued
    output                  re2ab_WrAlmFull;        //                       arbiter:       write fifo almost full
    
    input  [ADDR_LMT-1:0]   ab2re_RdAddr;           // [ADDR_LMT-1:0]        arbiter:       Reads may yield to writes
    input  [13:0]           ab2re_RdTID;            // [13:0]                arbiter:       meta data
    input                   ab2re_RdEn;             //                       arbiter:       read enable
    output                  re2ab_RdSent;           //                       arbiter:       read issued
    
    output                  re2ab_RdRspValid;       //                       arbiter:       read response valid
    output                  re2ab_UMsgValid;        //                       arbiter:       UMsg valid
    output                  re2ab_CfgValid;         //                       arbiter:       Cfg valid
    output [13:0]           re2ab_RdRsp;            // [13:0]                arbiter:       read response header
    output [DATA_WIDTH-1:0] re2ab_RdData;           // [511:0]               arbiter:       read data
    output                  re2ab_stallRd;          //                       arbiter:       stall read requests FOR LPBK1
    
    output                  re2ab_WrRspValid;       //                       arbiter:       write response valid
    output [13:0]           re2ab_WrRsp;            // [13:0]                arbiter:       write response header
    
    output                  re2xy_go;               //                       requestor:     start of frame recvd
    output [31:0]           re2xy_NumLines;         // [31:0]                requestor:     number of cache lines
    output [31:0]           re2xy_addr_reset;
    output [31:0]           re2xy_read_offset;
    output [31:0]           re2xy_write_offset;
    output [31:0]           re2xy_my_config1;
    output [31:0]           re2xy_my_config2;
    output [31:0]           re2xy_my_config3;
    output [31:0]           re2xy_my_config4;
    output [31:0]           re2xy_my_config5;
    output                  re2xy_Cont;             //                       requestor:     continuous mode
    output [31:0]           re2xy_src_addr;         // [31:0]                requestor:     src address
    output [31:0]           re2xy_dst_addr;         // [31:0]                requestor:     destination address
    output [7:0]            re2xy_test_cfg;         // [7:0]                 requestor:     8-bit test cfg register.
    output [2:0]            re2ab_Mode;             // [2:0]                 requestor:     test mode
    input                   ab2re_TestCmp;          //                       arbiter:       Test completion flag
    input  [255:0]          ab2re_ErrorInfo;        // [255:0]               arbiter:       error information
    input                   ab2re_ErrorValid;       //                       arbiter:       test has detected an error
    
    output                  test_Resetb;
    //----------------------------------------------------------------------------------------------------------------------
    // NLB v1.1 AFU ID
    localparam       NLB_V1_1            = 128'hC000_C966_0D82_4272_9AEF_FE5F_8457_0612;
    localparam       VERSION             = 16'h0002;
    
    //---------------------------------------------------------
    // CCI-S Request Encodings  ***** DO NOT MODIFY ******
    //---------------------------------------------------------
    localparam       WrThru              = 4'h1;
    localparam       WrLine              = 4'h2;
    localparam       RdLine_S            = 4'h4;
    localparam       WrFence             = 4'h5;
    localparam       RdLine_I            = 4'h6;
    localparam       RdLine_O            = 4'h7;
    localparam       Intr                = 4'h8;    // FPGA to CPU interrupt
    
    //--------------------------------------------------------
    // CCI-S Response Encodings  ***** DO NOT MODIFY ******
    //--------------------------------------------------------
    localparam      RSP_READ             = 4'h0;
    localparam      RSP_CSR              = 4'h1;
    localparam      RSP_WRITE            = 4'h2;
    
    //---------------------------------------------------------
    // Default Values ****** May be MODIFIED ******* 
    //---------------------------------------------------------
    localparam      DEF_SRC_ADDR         = 32'h0400_0000;           // Read data starting from here. Cache aligned Address
    localparam      DEF_DST_ADDR         = 32'h0500_0000;           // Copy data to here. Cache aligned Address
    localparam      DEF_DSM_BASE         = 32'h04ff_ffff;           // default status address
    
    //---------------------------------------------------------
    // CSR Address Map ***** DO NOT MODIFY *****
    //---------------------------------------------------------
    localparam      CSR_AFU_DSM_BASEL    = 16'h1a00;                 // WO - Lower 32-bits of AFU DSM base address. The lower 6-bbits are 4x00 since the address is cache aligned.
    localparam      CSR_AFU_DSM_BASEH    = 16'h1a04;                 // WO - Upper 32-bits of AFU DSM base address.
    localparam      CSR_SRC_ADDR         = 16'h1a20;                 // WO   Reads are targetted to this region 
    localparam      CSR_DST_ADDR         = 16'h1a24;                 // WO   Writes are targetted to this region
    localparam      CSR_CTL              = 16'h1a2c;                 // WO   Control CSR to start n stop the test
    localparam      CSR_CFG              = 16'h1a34;                 // WO   Configures test mode, wrthru, cont and delay mode
    localparam      CSR_NUM_LINES        = 16'h1a28;                 // WO   Numbers of cache lines to be read/write
    localparam      CSR_INACT_THRESH     = 16'h1a38;                 // WO   set the threshold limit for inactivity trigger
    localparam      CSR_INTERRUPT0       = 16'h1a3c;                 // WO   SW allocates Interrupt APIC ID & Vector
    localparam      CSR_ADDR_RESET       = 16'h1a80;
    localparam      CSR_READ_OFFSET      = 16'h1a84;
    localparam      CSR_WRITE_OFFSET     = 16'h1a88;
    localparam      CSR_MY_CONFIG1       = 16'h1a94;
    localparam      CSR_MY_CONFIG2       = 16'h1a8c;
    localparam      CSR_MY_CONFIG3       = 16'h1a90;
    localparam      CSR_MY_CONFIG4       = 16'h1a98;
    localparam      CSR_MY_CONFIG5       = 16'h1a9c;
    
    //----------------------------------------------------------------------------------
    // Device Status Memory (DSM) Address Map ***** DO NOT MODIFY *****
    // Physical address = value at CSR_AFU_DSM_BASE + Byte offset
    //----------------------------------------------------------------------------------
    //                                     Byte Offset                 Attribute    Width   Comments
    localparam      DSM_AFU_ID           = 32'h0;                   // RO           32b     non-zero value to uniquely identify the AFU
    localparam      DSM_STATUS           = 32'h40;                  // RO           512b    test status and error info
    
    //----------------------------------------------------------------------------------------------------------------------
    
    reg  [DATA_WIDTH-1:0]   cf2ci_C1TxData;
    reg  [TXHDR_WIDTH-1:0]  cf2ci_C1TxHdr;
    reg                     cf2ci_C1TxWrValid;
    reg  [TXHDR_WIDTH-1:0]  cf2ci_C0TxHdr;
    reg                     cf2ci_C0TxRdValid;
    reg                     cf2ci_C1TxIntrValid;
    
    reg  [31:0]             ErrorVector;
    reg  [31:0]             Num_Reads;                              // Number of reads performed
    reg  [31:0]             Num_Writes;                             // Number of writes performed
    reg  [31:0]             Num_ticks_low, Num_ticks_high;
    reg  [PEND_THRESH-1:0]  Num_Pend;                               // Number of pending requests
    reg  [PEND_THRESH-1:0]  Num_WrPend;                             // Number of pending writes
    reg  [31:0]             Num_C0stall;                            // Number of clocks for which Channel0 was throttled
    reg  [31:0]             Num_C1stall;                            // Number of clocks for which channel1 was throttled
    reg  signed [31:0]      Num_RdCredits;                          // For LPBK1: number of read credits
    reg                     re2ab_stallRd;
    reg                     tx_stallWr;
    reg                     RdHdr_valid;
    wire                    WrHdr_valid;
    reg  [31:0]             wrfifo_addr;
    reg  [DATA_WIDTH-1:0]   wrfifo_data;
    reg                     txFifo_RdAck;
    reg  [DATA_WIDTH-1:0]   rb2cf_C0RxData_q;
    reg  [RXHDR_WIDTH-1:0]  rb2cf_C0RxHdr_q, rb2cf_C0RxHdr_qq;
    reg                     rb2cf_C0RxWrValid_q, rb2cf_C0RxWrValid_qq;
    reg                     rb2cf_C0RxRdValid_q;
    reg                     rb2cf_C0RxUMsgValid_q;
    reg                     re2ab_CfgValid_d;
    reg                     re2ab_RdSent;
    reg                     status_write;
    reg                     interrupt_sent;
    reg                     send_interrupt;
    
    reg   [31:0]            inact_cnt;
    reg                     inact_timeout;
    reg   [5:0]             delay_lfsr;
    reg   [31:0]            cr_inact_thresh;
    reg                     penalty_start_f;
    reg   [7:0]             penalty_start;
    reg   [7:0]             penalty_end;
    reg                     dsm_base_valid;
    reg                     dsm_base_valid_q;
    reg                     afuid_updtd;
    reg   [3:0]             rdreq_type;
    reg   [3:0]             rnd_rdreq_type;
    reg   [1:0]             rnd_rdreq_sel;
    
    integer                 i;
    reg   [63:0]            cr_dsm_base;                            // a00h, a04h - DSM base address
    reg   [31:0]            cr_src_address;                         // a20h - source buffer address
    reg   [31:0]            cr_dst_address;                         // a24h - destn buffer address
    reg   [31:0]            cr_num_lines;                           // a28h - Number of cache lines
    reg   [31:0]            cr_addr_reset;
    reg   [31:0]            cr_read_offset;
    reg   [31:0]            cr_write_offset;
    reg   [31:0]            cr_my_config1;
    reg   [31:0]            cr_my_config2;
    reg   [31:0]            cr_my_config3;
    reg   [31:0]            cr_my_config4;
    reg   [31:0]            cr_my_config5;
    reg   [31:0]            cr_ctl = 0;                             // a2ch - control register to start and stop the test
    reg                     cr_wrthru_en;                           // a34h - [0]    : test configuration- wrthru_en
    reg                     cr_cont;                                // a34h - [1]    : repeats the test sequence, NO end condition
    reg   [2:0]             cr_mode;                                // a34h - [4:2]  : selects test mode
    reg                     cr_delay_en;                            // a34h - [8]    : use start delay
    reg   [1:0]             cr_rdsel, cr_rdsel_q;                   // a34h - [10:9] : read request type
    reg   [7:0]             cr_test_cfg;                            // a34h - [27:0] : configuration within a selected test mode
    reg   [31:0]            cr_interrupt0;                          // a3ch - SW allocates apic id & interrupt vector
    reg                     cr_interrupt_testmode;
    reg                     cr_interrupt_on_error;
    reg   [31:0]            ds_stat_address;                        // 040h - test status is written to this address
    reg   [31:0]            ds_afuid_address;                        // 040h - test status is written to this address
    wire  [31:0]            re2xy_src_addr  = cr_src_address;
    wire  [31:0]            re2xy_dst_addr  = cr_dst_address;
    
    
    wire                    txFifo_Full;
    wire                    txFifo_AlmFull;
    wire [13:0]             rxfifo_Din      = rb2cf_C1RxHdr[13:0];
    wire                    rxfifo_WrEn     = rb2cf_C1RxWrValid;
    wire                    rxfifo_Full;
    
    wire [13:0]             rxfifo_Dout;
    wire                    rxfifo_Dout_v;
    wire                    test_Resetb     = cr_ctl[0];                // Clears all the states. Either is one then test is out of Reset.
    wire                    test_go         = cr_ctl[1];                // When 0, it allows reconfiguration of test parameters.
    wire [2:0]              re2ab_Mode      = cr_mode;
    wire                    re2ab_WrSent    = !txFifo_Full;             // stop accepting new requests, after status write=1 
    wire                    txFifo_WrEn     = (ab2re_WrEn| ab2re_WrFence) && ~txFifo_Full;
    wire [13:0]             txFifo_WrReqId;
    wire [ADDR_LMT-1:0]     txFifo_WrAddr;
    wire                    txFifo_WrFence;
    
        // Format Read Header
    wire [31:0]             RdAddr  = ab2re_RdAddr;
    wire [13:0]             RdReqId = 14'h0000 | ab2re_RdTID;
    wire [TXHDR_WIDTH-1:0]  RdHdr   = {
                                        5'h00,                          // [60:56]      Byte Enable
                                        rdreq_type,                     // [55:52]      Request Type
                                        6'h00,                          // [51:46]      Rsvd
                                        RdAddr,                         // [45:14]      Address
                                        RdReqId                         // [13:0]       Meta data to track the SPL requests
                                      };
    
        // Format Write Header
    wire [31:0]             WrAddr      = txFifo_WrAddr;
    wire [13:0]             WrReqId     = 14'h0000 | txFifo_WrReqId;
    wire [3:0]              wrreq_type  = txFifo_WrFence ? WrFence 
                                          :cr_wrthru_en  ? WrThru
                                                         : WrLine;
    wire [DATA_WIDTH-1:0]   WrData;
    wire [TXHDR_WIDTH-1:0]  WrHdr   = {
                                        5'h00,                          // [60:56]      Byte Enable
                                        wrreq_type,                     // [55:52]      Request Type
                                        6'h00,                          // [51:46]      Rsvd
                                        WrAddr,                         // [45:14]      Address
                                        WrReqId                         // [13:0]       Meta data to track the SPL requests
                                        };


    wire                    re2ab_RdRspValid = rb2cf_C0RxRdValid_q;
    wire                    re2ab_UMsgValid  = rb2cf_C0RxUMsgValid_q;
    wire                    re2ab_CfgValid   = re2ab_CfgValid_d;
    wire   [13:0]           re2ab_RdRsp      = rb2cf_C0RxHdr_q[13:0];
    wire   [DATA_WIDTH-1:0] re2ab_RdData     = rb2cf_C0RxData_q;
    wire                    re2ab_WrRspValid = rxfifo_Dout_v | rb2cf_C0RxWrValid_qq;
    wire   [13:0]           re2ab_WrRsp      = rb2cf_C0RxWrValid_qq ? rb2cf_C0RxHdr_qq[13:0] : rxfifo_Dout;
    reg                     re2xy_go;
    wire   [31:0]           re2xy_NumLines   = cr_num_lines;
    wire   [31:0]           re2xy_addr_reset    = cr_addr_reset;
    wire   [31:0]           re2xy_read_offset   = cr_read_offset;
    wire   [31:0]           re2xy_write_offset  = cr_write_offset;
    wire   [31:0]           re2xy_my_config1 = cr_my_config1;
    wire   [31:0]           re2xy_my_config2 = cr_my_config2;
    wire   [31:0]           re2xy_my_config3 = cr_my_config3;
    wire   [31:0]           re2xy_my_config4 = cr_my_config4;
    wire   [31:0]           re2xy_my_config5 = cr_my_config5;
    wire                    re2xy_Cont       = cr_cont;
    wire                    re2ab_WrAlmFull  = txFifo_AlmFull;
    wire                    rnd_delay        = ~cr_delay_en || (delay_lfsr[0] || delay_lfsr[2] || delay_lfsr[3]);
    wire   [7:0]            re2xy_test_cfg   = cr_test_cfg;
    wire                    tx_errorValid    = ErrorVector!=0;
    reg      [3:0]          cr_rate_limit;
    reg      [1:0]          cr_rate_read;
    reg      [1:0]          cr_rate_write;
    reg      [1:0]          rate_limit_read =0;
    reg                     rate_mask_read_n;
    reg      [1:0]          rate_limit_write=0;
    reg                     rate_mask_write_n;



    always @(posedge Clk_32UI)                                              // - Update Test Configuration
    begin                                                                   //-----------------------------
        if(!Resetb)
        begin
            cr_src_address  <= 0;
            cr_dst_address  <= 0;
            cr_ctl          <= 0;
            cr_wrthru_en    <= 0;
            cr_mode         <= 0;
            cr_cont         <= 0;
            cr_num_lines    <= 0;
            cr_addr_reset   <= 0;
            cr_read_offset  <= 0;
            cr_write_offset <= 0;
            cr_my_config1   <= 0;
            cr_my_config2   <= 0;
            cr_my_config3   <= 0;
            cr_my_config4   <= 0;
            cr_my_config5   <= 0;
            cr_delay_en     <= 0;
            cr_test_cfg     <= 0;
            cr_inact_thresh <= 32'hffff_ffff;
            cr_dsm_base     <= DEF_DSM_BASE;
            cr_interrupt0   <= 0;
            cr_interrupt_on_error <= 0;
            cr_interrupt_testmode <= 0;
            dsm_base_valid  <= 0;
            cr_rate_limit   <= 0;
        end
        else
        begin                  
            if(rb2cf_C0RxCfgValid)
                case({rb2cf_C0RxHdr[13:0],2'b00})         /* synthesis parallel_case */
                    CSR_CTL          :   cr_ctl             <= rb2cf_C0RxData[31:0];
                    CSR_AFU_DSM_BASEH:   cr_dsm_base[63:32] <= rb2cf_C0RxData[31:0];
                    CSR_AFU_DSM_BASEL:begin
                                         cr_dsm_base[31:0]  <= rb2cf_C0RxData[31:0];
                                         dsm_base_valid     <= 1;
                                      end
                    CSR_SRC_ADDR:        cr_src_address     <= rb2cf_C0RxData[31:0];
                    CSR_DST_ADDR:        cr_dst_address     <= rb2cf_C0RxData[31:0];
                    CSR_NUM_LINES:       cr_num_lines       <= rb2cf_C0RxData[31:0];            
                    CSR_ADDR_RESET:      cr_addr_reset      <= rb2cf_C0RxData[31:0];
                    CSR_READ_OFFSET:     cr_read_offset     <= rb2cf_C0RxData[31:0];
                    CSR_WRITE_OFFSET:    cr_write_offset    <= rb2cf_C0RxData[31:0];
                    CSR_MY_CONFIG1:      cr_my_config1      <= rb2cf_C0RxData[31:0];    
                    CSR_MY_CONFIG2:      cr_my_config2      <= rb2cf_C0RxData[31:0];
                    CSR_MY_CONFIG3:      cr_my_config3      <= rb2cf_C0RxData[31:0];
                    CSR_MY_CONFIG4:      cr_my_config4      <= rb2cf_C0RxData[31:0];
                    CSR_MY_CONFIG5:      cr_my_config5      <= rb2cf_C0RxData[31:0];
                endcase

            if(test_Resetb && ~test_go) // Configuration Mode, following CSRs can only be updated in this mode
            begin
                if(rb2cf_C0RxCfgValid)
                case({rb2cf_C0RxHdr[13:0],2'b00})         /* synthesis parallel_case */
                    CSR_INACT_THRESH:    cr_inact_thresh    <= rb2cf_C0RxData[31:0];
                    CSR_INTERRUPT0:      cr_interrupt0      <= rb2cf_C0RxData[31:0];
                    CSR_CFG:        begin
                                         cr_wrthru_en       <= rb2cf_C0RxData[0];
                                         cr_cont            <= rb2cf_C0RxData[1];
                                         cr_mode            <= rb2cf_C0RxData[4:2];
                                         cr_delay_en        <= rb2cf_C0RxData[8];
                                         cr_rdsel           <= rb2cf_C0RxData[10:9];
                                         cr_rate_limit      <= rb2cf_C0RxData[18:16];
                                         cr_test_cfg        <= rb2cf_C0RxData[27:20];
                                         cr_interrupt_on_error <= rb2cf_C0RxData[28];
                                         cr_interrupt_testmode <= rb2cf_C0RxData[29];
                                     end
                endcase
            end
        end
    end

    always @(posedge Clk_32UI)
    begin
        ds_stat_address  <= dsm_offset2addr(DSM_STATUS,cr_dsm_base);
        ds_afuid_address <= dsm_offset2addr(DSM_AFU_ID,cr_dsm_base);
        dsm_base_valid_q <= dsm_base_valid;
        cr_rdsel_q       <= cr_rdsel;
        delay_lfsr <= {delay_lfsr[4:0], (delay_lfsr[5] ^ delay_lfsr[4]) };

        case(cr_rdsel_q)
            2'h0:   rdreq_type <= RdLine_S;
            2'h1:   rdreq_type <= RdLine_I;
            2'h2:   rdreq_type <= RdLine_O;
            2'h3:   rdreq_type <= rnd_rdreq_type;
        endcase
        rnd_rdreq_sel  <= delay_lfsr%3;
        case(rnd_rdreq_sel)
            2'h1:   rnd_rdreq_type <= RdLine_I;
            2'h2:   rnd_rdreq_type <= RdLine_O;
            default:rnd_rdreq_type <= RdLine_S;
        endcase

        if(test_go & ci2cf_InitDn  & afuid_updtd)                                             
            re2xy_go    <= 1;
        if(status_write)
            re2xy_go    <= 0;
        
        send_interrupt <= (cr_interrupt_on_error & tx_errorValid) | cr_interrupt_testmode;

        // Either Read or Write Rate must be enabled
        // cr_rate_limit[2]   - 1: select write, 0: select read
        // cr_rate_limit[1:0] - rate limit ratio; min- 4:1, max- 1:1 (disable)
        // 
        cr_rate_read <= cr_rate_limit[1:0];
        cr_rate_write<= cr_rate_limit[1:0];
       if(cr_rate_limit[2]==1'b0 && cr_rate_limit[1:0]!=0)
       begin
            case(rate_mask_read_n)
                1'b0:begin
                    if(txFifo_RdAck || ~WrHdr_valid)
                    begin
                        rate_limit_read <= rate_limit_read + 1'b1;
                        if(rate_limit_read==cr_rate_read-1'b1)
                            rate_mask_read_n <= 1'b1;
                    end
                end
                1'b1:begin
                    rate_limit_read <= 0;
                    if(re2ab_RdSent)
                        rate_mask_read_n <= 1'b0;
                end
            endcase
       end
       else // cr_rate_read==0, i.e rate limiting disabled
       begin
            rate_mask_read_n <= 1'b1;
       end
       if(cr_rate_limit[2]==1'b1 && cr_rate_limit[1:0]!=0)
       begin
            case(rate_mask_write_n)
                1'b0:begin
                    if(re2ab_RdSent || ~ab2re_RdEn)
                    begin
                        rate_limit_write <= rate_limit_write + 1'b1;
                        if(rate_limit_write==cr_rate_write-1'b1)
                            rate_mask_write_n <= 1'b1;
                    end
                end
                1'b1:begin
                    rate_limit_write <= 1'b0;
                    if(txFifo_RdAck)
                        rate_mask_write_n <= 1'b0;
                end
            endcase
       end
       else // cr_rate_write==0, i.e rate limiting disabled
       begin
            rate_mask_write_n <= 1'b1;
       end

        //Tx Path
        //--------------------------------------------------------------------------
        cf2ci_C1TxHdr           <= 0;
        cf2ci_C1TxWrValid       <= 0;
        cf2ci_C1TxIntrValid     <= 0;
        cf2ci_C0TxHdr           <= 0;
        cf2ci_C0TxRdValid       <= 0;

        // Channel 1
        if(ci2cf_C1TxAlmFull==0)
        begin
            if( ci2cf_InitDn && dsm_base_valid_q && !afuid_updtd )
            begin
                afuid_updtd             <= 1;
                cf2ci_C1TxHdr           <= {
                                                5'h0,                      // [60:56]      Byte Enable
                                                WrLine,                    // [55:52]      Request Type
                                                6'h00,                     // [51:46]      Rsvd
                                                ds_afuid_address,          // [44:14]      Address
                                                14'h3ffe                   // [13:0]       Meta data to track the SPL requests
                                           };                
                cf2ci_C1TxWrValid       <= 1;
                cf2ci_C1TxData          <= {    368'h0,                    // [512:144]    Zeros
                                                VERSION ,                  // [143:128]    Version #2
                                                NLB_V1_1                   // [127:0]      AFU ID
                                           };
            end
            else if ( status_write
                    & send_interrupt
                    & !interrupt_sent
                    )
            begin
                interrupt_sent          <= 1;
                cf2ci_C1TxHdr           <= {
                                                5'h0,                      // [60:56]      Byte Enable
                                                Intr,                      // [55:52]      Request Type
                                                6'h00,                     // [51:46]      Rsvd
                                                cr_interrupt0,             // [44:14]      Address
                                                14'h3ffc                   // [13:0]       Meta data to track the SPL requests
                                            };                
                cf2ci_C1TxIntrValid     <= 1;
            end
            else if (re2xy_go & rnd_delay)
            begin
                if( ab2re_TestCmp                                               // Update Status upon test completion
                    ||tx_errorValid                                             // Error detected 
                    ||cr_ctl[2]                                                 // SW forced test termination
                  )                       
                begin                                                           //-----------------------------------
                    status_write       <= 1'b1;
                    if(status_write==0)
                        cf2ci_C1TxWrValid  <= 1'b1;
                    cf2ci_C1TxHdr      <= {
                                                5'h0,                           // [60:56]      Byte Enable
                                                cr_wrthru_en? WrThru            // [55:52]      Req Type
                                                            : WrLine,           //
                                                6'h00,                          // [51:46]      Rsvd
                                                ds_stat_address,                // [44:14]      Address
                                                14'h3fff                        // [13:0]       Meta data to track the SPL requests
                                            };
                    cf2ci_C1TxData     <= {     ab2re_ErrorInfo,               // [511:256] upper half cache line
                                                24'h00_0000,penalty_end,       // [255:224] test end overhead in # clks
                                                24'h00_0000,penalty_start,     // [223:192] test start overhead in # clks
                                                Num_Writes,                    // [191:160] Total number of Writes sent
                                                Num_Reads,                     // [159:128] Total number of Reads sent
                                                Num_ticks_high, Num_ticks_low, // [127:64]  number of clks
                                                ErrorVector,                   // [63:32]   errors detected            
                                                32'h0000_0001                  // [31:0]    test completion flag
                                            };
                end
                else if( WrHdr_valid  & rate_mask_write_n )                    // Write to Destination Workspace
                begin                                                          //-------------------------------------
                    cf2ci_C1TxHdr     <= WrHdr;
                    cf2ci_C1TxWrValid <= 1'b1;
                    cf2ci_C1TxData    <= WrData;
                    Num_Writes        <= Num_Writes + 1'b1;
                end
            end // re2xy_go
        end // C1_TxAmlFull

        // Channel 0
        if(  re2xy_go && rnd_delay 
          && RdHdr_valid && !ci2cf_C0TxAlmFull )                                // Read from Source Workspace
        begin                                                                   //----------------------------------
            cf2ci_C0TxHdr      <= RdHdr;
            cf2ci_C0TxRdValid  <= 1;
            Num_Reads          <= Num_Reads + 1'b1;
        end

        //--------------------------------------------------------------------------
        //Rx Response Path
        //--------------------------------------------------------------------------
        rb2cf_C0RxData_q       <= rb2cf_C0RxData;
        rb2cf_C0RxHdr_q        <= rb2cf_C0RxHdr;
        rb2cf_C0RxHdr_qq       <= rb2cf_C0RxHdr_q;
        rb2cf_C0RxWrValid_q    <= rb2cf_C0RxWrValid;
        rb2cf_C0RxWrValid_qq   <= rb2cf_C0RxWrValid_q;
        rb2cf_C0RxRdValid_q    <= rb2cf_C0RxRdValid;
        rb2cf_C0RxUMsgValid_q  <= rb2cf_C0RxUMsgValid;
        re2ab_CfgValid_d       <= rb2cf_C0RxCfgValid;

        // Counters
        //--------------------------------------------------------------------------

        if(re2xy_go)                                                // Count #clks after test start
        begin
            Num_ticks_low   <= Num_ticks_low + 1'b1;
            if(&Num_ticks_low)
                Num_ticks_high  <= Num_ticks_high + 1'b1;
        end

        if(re2xy_go & ci2cf_C0TxAlmFull)
            Num_C0stall     <= Num_C0stall + 1'b1;

        if(re2xy_go & ci2cf_C1TxAlmFull)
            Num_C1stall     <= Num_C1stall + 1'b1;

        case({cf2ci_C1TxWrValid, cf2ci_C0TxRdValid, rb2cf_C1RxWrValid,(rb2cf_C0RxRdValid|rb2cf_C0RxWrValid)})
            4'b0001:    Num_Pend    <= Num_Pend - 2'h1;
            4'b0010:    Num_Pend    <= Num_Pend - 2'h1;
            4'b0011:    Num_Pend    <= Num_Pend - 2'h2;
            4'b0100:    Num_Pend    <= Num_Pend + 2'h1;
            //4'b0101:    
            //4'b0110:
            4'b0111:    Num_Pend    <= Num_Pend - 2'h1;
            4'b1000:    Num_Pend    <= Num_Pend + 2'h1;
            //4'b1001:    
            //4'b1010:
            4'b1011:    Num_Pend    <= Num_Pend - 2'h1;
            4'b1100:    Num_Pend    <= Num_Pend + 2'h2;
            4'b1101:    Num_Pend    <= Num_Pend + 2'h1;
            4'b1110:    Num_Pend    <= Num_Pend + 2'h1;
            //4'b1111:
        endcase                

        case({cf2ci_C1TxWrValid, rb2cf_C1RxWrValid, rb2cf_C0RxWrValid})
            3'b001:     Num_WrPend  <= Num_WrPend - 1'b1;
            3'b010:     Num_WrPend  <= Num_WrPend - 1'b1;
            3'b011:     Num_WrPend  <= Num_WrPend - 2'h2;
            3'b100:     Num_WrPend  <= Num_WrPend + 1'b1;
            3'b111:     Num_WrPend  <= Num_WrPend - 1'b1;
        endcase
        tx_stallWr <= Num_WrPend>=(2**PEND_THRESH-4);

        // For LPBK1 (memory copy): stall reads  if Num_RdCredits less than 0. Read credits are limited by the depth of Write fifo
        // Wr fifo depth in requestor is 128. Therefore max num write pending should be less than 128.
        if(cf2ci_C0TxRdValid && !cf2ci_C1TxWrValid)
            Num_RdCredits <= Num_RdCredits - 1'b1;

        if(!cf2ci_C0TxRdValid && cf2ci_C1TxWrValid )
            Num_RdCredits <= Num_RdCredits + 1'b1;

        re2ab_stallRd     <= ($signed(Num_RdCredits)<=0);
        
        // Error Detection Logic
        //--------------------------
        if(Num_Pend<0)
        begin
            ErrorVector[0]  <= 1;
            /*synthesis translate_off */
            $display("nlb_lpbk: Error: unexpected Rx response");
            /*synthesis translate_on */
        end

        if(rxfifo_Full & rxfifo_WrEn)
        begin
            ErrorVector[1]  <= 1;
            /*synthesis translate_off */
            $display("nlb_lpbk: Error: WrRx fifo overflow");
            /*synthesis translate_on */
        end

        if(txFifo_Full & txFifo_WrEn)
        begin
            ErrorVector[2]  <= 1;
            /*synthesis translate_off */
            $display("nlb_lpbk: Error: wr fifo overflow");
            /*synthesis translate_on */
        end

        if(ErrorVector[3]==0)
            ErrorVector[3]  <= ab2re_ErrorValid;

        /* synthesis translate_off */
        if(cf2ci_C1TxWrValid)
            $display("*Req Type: %x \t Addr: %x \n Data: %x", cf2ci_C1TxHdr[55:52], cf2ci_C1TxHdr[45:14], cf2ci_C1TxData);

        if(cf2ci_C0TxRdValid)
            $display("*Req Type: %x \t Addr: %x", cf2ci_C0TxHdr[55:52], cf2ci_C0TxHdr[45:14]);

        /* synthesis translate_on */


        // Use for Debug- if no transactions going across the CCI interface # clks > inactivity threshold 
        // than set the flag. You may use this as a trigger signal in logic analyzer
        if(cf2ci_C1TxWrValid || cf2ci_C0TxRdValid)
            inact_cnt  <= 0;
        else if(re2xy_go)
            inact_cnt  <= inact_cnt + 1;

        if(inact_timeout==0)
        begin
            if(inact_cnt>=cr_inact_thresh)
                inact_timeout   <= 1;
        end
        else if(cf2ci_C1TxWrValid || cf2ci_C0TxRdValid)
        begin
            inact_timeout   <= 0;
        end

        if(!test_Resetb)
        begin
            Num_Reads               <= 0;
            Num_Writes              <= 0;
            Num_Pend                <= 0;
            Num_ticks_low           <= 0;
            Num_ticks_high          <= 0;
            re2xy_go                <= 0;
            rb2cf_C0RxData_q        <= 0;
            rb2cf_C0RxHdr_q         <= 0;
            rb2cf_C0RxHdr_qq        <= 0;
            rb2cf_C0RxWrValid_q     <= 0;
            rb2cf_C0RxWrValid_qq    <= 0;
            rb2cf_C0RxRdValid_q     <= 0;
            rb2cf_C0RxUMsgValid_q   <= 0;
            re2ab_CfgValid_d        <= 0;
            ErrorVector             <= 0;
            status_write            <= 0;
            interrupt_sent          <= 0;
            send_interrupt          <= 0;
            inact_cnt               <= 0;
            inact_timeout           <= 0;
            delay_lfsr              <= 1;
            Num_C0stall             <= 0;
            Num_C1stall             <= 0;
            Num_RdCredits           <= (2**PEND_THRESH-8);
            Num_WrPend              <= 0;
            tx_stallWr              <= 0;
        end
        if(!Resetb)
        begin
            afuid_updtd             <= 0;
            dsm_base_valid_q        <= 0;
        end
    end

    always @(posedge Clk_32UI)                                                      // Computes NLB start and end overheads
    begin                                                                           //-------------------------------------
        if(!test_go)
        begin
            penalty_start   <= 0;
            penalty_start_f <= 0;
            penalty_end     <= 2;
        end
        else
        begin
            if(!penalty_start_f & (cf2ci_C0TxRdValid | cf2ci_C1TxWrValid ))
            begin
                penalty_start_f   <= 1;
                penalty_start     <= Num_ticks_low[7:0];                    /* synthesis translate_off */
                $display ("NLB_INFO : start penalty = %d ", Num_ticks_low); /* synthesis translate_on */
            end

            penalty_end <= penalty_end + 1'b1;
            if(rb2cf_C0RxWrValid | rb2cf_C0RxRdValid | rb2cf_C0RxUMsgValid 
            | rb2cf_C1RxWrValid )
            begin
                penalty_end     <= 2;
            end

            if(ab2re_TestCmp
              && !ci2cf_C1TxAlmFull
              && !status_write)
            begin                                                       /* synthesis translate_off */
                $display ("NLB_INFO : end penalty = %d ", penalty_end); /* synthesis translate_on */
            end

        end
    end

    always @(*)
    begin
        RdHdr_valid = re2xy_go
        && !status_write
        && rnd_delay
        && !ci2cf_C0TxAlmFull
        && ab2re_RdEn
        && rate_mask_read_n;

        re2ab_RdSent= RdHdr_valid;

        txFifo_RdAck = re2xy_go && rnd_delay  && !ci2cf_C1TxAlmFull && WrHdr_valid && rate_mask_write_n;

    end

    //----------------------------------------------------------------------------------------------------------------------------------------------
    //                                                              Instances
    //----------------------------------------------------------------------------------------------------------------------------------------------
    // Tx Write request fifo. Some tests may have writes dependent on reads, i.e. a read response will generate a write request
    // If the CCI-S write channel is stalled, then the write requests will be queued up in this Tx fifo.

    wire [1+512+ADDR_LMT+13:0]txFifo_Din    = {ab2re_WrFence,
                                               ab2re_WrDin,
                                               ab2re_WrAddr, 
                                               ab2re_WrTID
                                              };
    wire [1+512+ADDR_LMT+13:0]txFifo_Dout;
    wire                    txFifo_Dout_v;
    assign                  txFifo_WrAddr   = txFifo_Dout[ADDR_LMT-1+14:14];
    assign                  WrData          = txFifo_Dout[511+ADDR_LMT+14:ADDR_LMT+14];
    assign                  txFifo_WrFence  = txFifo_Dout[1+512+ADDR_LMT+13];
    assign                  txFifo_WrReqId  = txFifo_Dout[13:0];
    assign                  WrHdr_valid     = txFifo_Dout_v && !tx_stallWr;
    nlb_sbv_gfifo  #(.DATA_WIDTH  (1+DATA_WIDTH+ADDR_LMT+14),
                     .DEPTH_BASE2 (PEND_THRESH),
                     .FULL_THRESH (2**PEND_THRESH-10)  
    )nlb_writeTx_fifo
    (                                       //--------------------- Input  ------------------
        .Resetb         (test_Resetb),
        .Clk            (Clk_32UI),    
        .fifo_din       (txFifo_Din),          
        .fifo_wen       (txFifo_WrEn),      
        .fifo_rdack     (txFifo_RdAck),
                                           //--------------------- Output  ------------------
        .fifo_dout      (txFifo_Dout),        
        .fifo_dout_v    (txFifo_Dout_v),
        .fifo_empty     (),
        .fifo_full      (txFifo_Full),
        .fifo_count     (),
        .fifo_almFull   (txFifo_AlmFull)
    ); 

    wire    rxfifo_RdAck    = rxfifo_Dout_v & ~rb2cf_C0RxWrValid_qq;
    
    // CCI-S could return two write responses per clock, but arbiter can accept only 1 write response per clock. 
    // This fifo will store the second write response
    nlb_sbv_gfifo  #(.DATA_WIDTH  ('d14),
                    .DEPTH_BASE2 (PEND_THRESH)
    )nlb_writeRx_fifo  
    (                                      //--------------------- Input  ------------------
        .Resetb         (test_Resetb),
        .Clk            (Clk_32UI),
        .fifo_din       (rxfifo_Din),          
        .fifo_wen       (rxfifo_WrEn),      
        .fifo_rdack     (rxfifo_RdAck),        
                                           //--------------------- Output  ------------------
        .fifo_dout      (rxfifo_Dout),
        .fifo_dout_v    (rxfifo_Dout_v),
        .fifo_empty     (),
        .fifo_full      (rxfifo_Full),
        .fifo_count     (),
        .fifo_almFull   ()
    );


    // Function: Returns physical address for a DSM register
    function automatic [31:0] dsm_offset2addr;
        input    [9:0]  offset_b;
        input    [63:0] base_b;
        begin
            dsm_offset2addr = base_b[37:6] + offset_b[9:6];
        end
    endfunction

    //----------------------------------------------------------------
    // For signal tap
    //----------------------------------------------------------------
    `ifdef DEBUG_NLB
        (* noprune *) reg [3:0]                 DEBUG_nlb_error;
        (* noprune *) reg [31:0]                DEBUG_Num_Reads;
        (* noprune *) reg [31:0]                DEBUG_Num_Writes;
        (* noprune *) reg                       DEBUG_inact_timeout;
        (* noprune *) reg [9:0]                 DEBUG_C0TxHdrID;
        (* noprune *) reg [31:0]                DEBUG_C0TxHdrAddr;
        (* noprune *) reg [9:0]                 DEBUG_C1TxHdrID;
        (* noprune *) reg [31:0]                DEBUG_C1TxHdrAddr;
        (* noprune *) reg [16:0]                 DEBUG_C1TxData;
        (* noprune *) reg [9:0]                 DEBUG_C0RxHdrID;
        (* noprune *) reg [8:0]                 DEBUG_C0RxData;
        (* noprune *) reg [9:0]                 DEBUG_C1RxHdrID;
        (* noprune *) reg                       DEBUG_C0TxRdValid;
        (* noprune *) reg                       DEBUG_C0RxRdValid;
        (* noprune *) reg                       DEBUG_C1TxWrValid;
        (* noprune *) reg                       DEBUG_C0RxWrValid;
        (* noprune *) reg                       DEBUG_C1RxWrValid;
        (* noprune *) reg [PEND_THRESH-1:0]     DEBUG_rxfifo_count;
        (* noprune *) reg [PEND_THRESH-1:0]     DEBUG_txfifo_count;
        (* noprune *) reg [31:0]                DEBUG_Num_Pend;


        always @(posedge Clk_32UI)
        begin
            DEBUG_nlb_error[3:0]    <= ErrorVector[3:0];
            DEBUG_Num_Reads         <= Num_Reads;
            DEBUG_Num_Writes        <= Num_Writes;
            DEBUG_inact_timeout     <= inact_timeout;
            DEBUG_C0TxHdrID         <= cf2ci_C0TxHdr[9:0];
            DEBUG_C0TxHdrAddr       <= cf2ci_C0TxHdr[45:14];
            DEBUG_C1TxHdrID         <= cf2ci_C1TxHdr[9:0];
            DEBUG_C1TxHdrAddr       <= cf2ci_C1TxHdr[45:14];
            DEBUG_C1TxData          <= cf2ci_C1TxData[16:0];
            DEBUG_C0RxHdrID         <= rb2cf_C0RxHdr[9:0];
            DEBUG_C0RxData          <= rb2cf_C0RxData[8:0];
            DEBUG_C1RxHdrID         <= rb2cf_C1RxHdr[9:0];
            DEBUG_C0TxRdValid       <= cf2ci_C0TxRdValid;
            DEBUG_C0RxRdValid       <= rb2cf_C0RxRdValid;
            DEBUG_C1TxWrValid       <= cf2ci_C1TxWrValid;
            DEBUG_C0RxWrValid       <= rb2cf_C0RxWrValid;
            DEBUG_C1RxWrValid       <= rb2cf_C1RxWrValid;
            DEBUG_rxfifo_count      <= rxfifo_count;
            DEBUG_txfifo_count      <= txfifo_count;
            DEBUG_Num_Pend          <= Num_Pend;
        end

    `endif // DEBUG_NLB

endmodule
