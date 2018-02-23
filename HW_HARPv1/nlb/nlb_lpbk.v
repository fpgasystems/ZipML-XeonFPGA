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
// Engineer:            Pratik Marolia
// Create Date:         Tue Feb 21 17:18:22 PDT 2012
// Edited on:           Wed Apr 09 11:28:01 PDT 2014
// Module Name:         nlb_lpbk.v
// Project:             NLB AFU v 1.1
//                      Compliant with CCI v2.1
// Description:         top level wrapper for NLB, it instantiates requestor
//                      & arbiter
// ***************************************************************************
//
// Change Log
// Date             Comments
// 7/2/2014         Supports extended 64KB CSR space. Remapped all NLB CSRs
//
// ---------------------------------------------------------------------------------------------------------------------------------------------------
//                                         NLB - Native Loopback test
//  ------------------------------------------------------------------------------------------------------------------------------------------------
//
// This is a reference CCI-S AFU implementation compatible with CCI specification v2.10
// The purpose of this design is to generate different memory access patterns for validation.
// The test can also be used to measure following performance metrics:
// Bandwidth: 100% Read, 100% Write, 100% Read + 100% Write
// Latency: Read, Write
//
//   Block Diagram:
//
//   +------------------------------------------------------------------+                       
//   |    +----------+           +---------+      +------------+        |                           
//   |    |          |  Wr       |         |<---->| Test_lpbk1 |        |                           
//  CCI-S |Requestor |<--------->| Arbiter |<--+  +------------+        |                            
// <----->|          |  Rd       |/Selector|<+ |  +------------+        |                       
//   |    |          |<--------->|         | | +->|Test_rdwr   |        |                       
//   |    +----------+           +---------+ |    +------------+        |                       
//   |                                    /\ |    +------------+        |               
//   |                                    |  +--->| Test_lpbk2 |        |
//   |                                    |       +------------+        |
//   |                                    |       +------------+        |                       
//   |                                    ------->| Test_lpbk3 |        |
//   |                                    |       +------------+        |
//   |                                    |       +------------+        |                       
//   |                                    ------->| Test_SW1   |        |
//   | nlb_lpbk                                   +------------+        |
//   +------------------------------------------------------------------+
//
//
//  NLB Revision and feature tracking
//-------------------------------------------------------------------------------------------
//      Rev     CCI spec        Comments
//-------------------------------------------------------------------------------------------
//      1.0     0.9             Uses proprietary memory mapped CSR read mapping
//      1.1     2.0             Device Status Memory Compliant                          -- Current
//      1.3     2.0             Portability across CCI and SPL                          -- Planned
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
//
// 1 Cacheline = 64B i.e 2^6 Bytes
// Let N be the number of cachelines in the source & destination buffers. Then select CSR_SRC_ADDR & CSR_DEST_ADDR to be 2^(N+6) aligned.
// CSR_NUM_LINES should be less than or equal to N.
//
// CSR_SRC_ADDR:
// [31:N]   WO   2^(N+6)MB aligned address points to the start of read buffer
// [N-1:0]  WO   'h0
//
// CSR_DST_ADDR:
// [31:N]   WO   2^(N+6)MB aligned address points to the start of write buffer
// [N-1:0]  WO   'h0
//
// CSR_NUM_LINES:
// [31:N]   WO   'h0
// [N-1:0]  WO    # cache lines to be read/written to. This threshold may be different for each test AFU. IMPORTANT- Ensure that source and destination buffers 
//              are large enough to accomodate the N cache lines.
//
// Let's assume N=14, then CSR_SRC_ADDR and CSR_DST_ADDR will accept a 2^20, i.e. 1MB aligned addresses.
//
// CSR_SRC_ADDR:
// [31:14]  WO    1MB aligned address
// [13:0]   WO   'h0
//
// CSR_DST_ADDR:
// [31:14]  WO    1MB aligned address
// [13:0]   WO   'h0
//
// CSR_NUM_LINES:
// [31:14]  WO    'h0
// [13:0]   WO    # cache lines to be read/written to. This threshold may be different for each test AFU. IMPORTANT- Ensure that source and destination buffers 
//              are large enough to accomodate the # cache lines.
//
// CSR_CTL:
// [31:3]   WO    Rsvd
// [2]      WO    Force test completion. Writes test completion flag and other performance counters to csr_stat. It appears to be like a normal test completion.
// [1]      WO    Starts test execution.
// [0]      WO    Active low test Reset. All configuration parameters change to reset defaults.
//
//
// CSR_CFG:
// [29]     WO    cr_interrupt_testmode - used to test interrupt. Generates an interrupt at end of each test.
// [28]     WO    cr_interrupt_on_error - send an interrupt when error detected
// [27:20]  WO    cr_test_cfg  -may be used to configure the behavior of each test mode
// [18:16]  WO    cr_rate_limit-rate limit the read or write requests
//                cr_rate_limit[2]   - 1: select write, 0: select read
//                cr_rate_limit[1:0] - rate limit ratio; min- 4:1, max- 1:1 (disable)
// [10:9]   WO    cr_rdsel     -configure read request type. 0- RdLine_S, 1- RdLine_I, 2- RdLine_O, 3- Mixed mode
// [8]      WO    cr_delay_en  -enable random delay insertion between requests
// [4:2]    WO    cr_mode      -configures test mode
// [1]      WO    cr_cont      - 1- test rollsover to start address after it reaches the CSR_NUM_LINES count. Such a test terminates only on an error.
//                               0- test terminates, updated the status csr when CSR_NUM_LINES count is reached.
// [0]      WO    cr_wrthru_en -switch between write back to write through request type. 0- Wr Back, 1- WrThru
// 
//
// CSR_INACT_THRESHOLD:
// [31:0]   WO  inactivity threshold limit. The idea is to detect longer duration of stalls during a test run. Inactivity counter will count number of consecutive idle cycles,
//              i.e. no requests are sent and no responses are received. If the inactivity count > CSR_INACT_THRESHOLD then it sets the inact_timeout signal. The inactivity counter
//              is activated only after test is started by writing 1 to CSR_CTL[1].
//
// CSR_INTERRUPT0:
// [23:16]  WO    vector      - Interrupt Vector # for the device
// [15:0]   WO    apic id     - Interrupt APIC ID for the device 
//
// DSM_STATUS:
// [511:256] RO  Error dump from Test Mode
// [255:224] RO  end overhead
// [223:192] RO  start overhead
// [191:160] RO  Number of writes
// [159:128] RO  Number of reads
// [127:64]  RO  Number of clocks
// [63:32]   RO  test error register
// [31:0]    RO  test completion flag
//
// DSM_AFU_ID:
// [512:144] RO   Zeros
// [143:128] RO   Version
// [127:0]   RO   AFU ID 
//
// High Level Test flow:
//---------------------------------------------------------------
// 1.   SW initalizes Device Status Memory (DSM) to zero.
// 2.   SW writes DSM BASE address to AFU. CSR Write(DSM_BASE_H), CSR Write(DSM_BASE_L)
// 3.   AFU writes the AFU_ID to DSM_AFU_ID.
// 4.   SW polls on DSM_AFU_ID.
// 5.   SW prepares source & destination memory buffer- this is test specific.
// 4.   SW CSR Write(CSR_CTL)=3'h1. This brings the test out of reset and puts it in configuration mode. Configuration is allowed only when CSR_CTL[0]=1 & CSR_CTL[1]=0.
// 5.   SW configures the test parameters, i.e. src/dest address, csr_cfg, num lines etc. 
// 6.   SW CSR Write(CSR_CTL)=3'h3. AFU begins test execution.
// 7.   Test completion:
//      a. HW completes- When the test completes or detects an error, the HW AFU writes to DSM_STATUS. SW is polling on DSM_STATUS[31:0]==1.
//      b. SW forced completion- The SW forces a test completion, CSR Write(CSR_CTL)=3'h7. HW AFU writes to DSM_STATUS.
//      The test completion method used depends on the test mode. Some test configuration have no defined end state. When using continuous mode, you must use 7.b. 
//                              
// Test modes:
//---------------------------------------------------------------
//      Test Mode       Encoding- CSR_CFG[4:2]        #cache line threshold- CSR_NUM_LINES[N-1:0]       #cache line threshold for N=14
//      --------------------------------------------------------------------------------------------------------------------------------------
// 1.     LPBK1         3'b000                          2^N                                             14'h3fff
// 2.     READ          3'b001                          2^N                                             14'h3fff
// 3.     WRITE         3'b010                          2^N                                             14'h3fff
// 4.     TRPUT         3'b011                          2^N                                             14'h3fff
// 5.     LPBK2         3'b101                          smaller of 2^N or 14'h80                        14'h80
// 6.     LPBK3         3'b110                          smaller of 2^(N-16) or 14'h80                   14'h10
// 7.     SW1           3'b111                          2^N                                             14'h3ffe
//
// 1. LPBK1:
// This is a memory copy test. AFU copies CSR_NUM_LINES from source buffer to destination buffer. On test completion, the software compares the source and destination buffers.
//
// 2. READ:
// This is a read only test with NO data checking. AFU reads CSR_NUM_LINES starting from CSR_SRC_ADDR. This test is used to stress the read path and 
// measure 100% read bandwidth or latency.
//
// 3. WRITE:
// This is a write only test with NO data checking. AFU writes CSR_NUM_LINES starting from CSR_DST_ADDR location. This test is used to stress the write path and
// measure 100% write bandwidth or latency.
// 
// 4. TRPUT:
// This test combines the read and write streams. There is NO data checking and no dependency between read & writes. It reads CSR_NUM_LINES starting from CSR_SRC_ADDR location and 
// writes CSR_NUM_LINES to CSR_DST_ADDR. It is also used to measure 50% Read + 50% Write bandwdith.
//
// 5. LPBK2:
// This is a cache coherency test, both CPU and Test are fighting for the same set of cache lines. 
// Upper Limit on # cache lines (CSR_NUM_LINES) = 128
// For this test set CSR_SRC_ADDR= CSR_DST_ADDR, because to test coherency we would like to read and write same set of addresses.
// This test will read CSR_NUM_LINES starting from CSR_SRC_ADDR but it will write to only half of those cache lines.
//
// A. Lets classify the cache lines into two sets, ones that can be owned by FPGA and ones that can be owned by CPU.
// cacheline address[0] = 1- Line is owned by FPGA
// cacheline address[0] = 0- Line is owned by CPU
//
// B. Also lets divide the cache data into two fields: deterministic and random data fields.
// Determinitistic data fields: data[31:0] = data[256+31:256]
// Random data fields: data[255:32] & data [511:288]
//
// C. Rules of the game:
//   1. Both agents, FPGA and CPU can read all cache lines.
//   2. A cache line can only be written by its owner as determined by section A above. The agent must write the cache line address to deterministic data fields. The random data
//      fields can ofcourse be random.
//   3. Reading owned lines - An agent reading owned lines must check the deterministic data fields and full or part of the random fields.
//      Reading not owned lines - An agent reading these lines, must only check the deterministic data fields.
//      If error is detected then set the errorvalid signal and send out the error report. Look at the test modules for details on error report.
//
// This test must be used in cont mode. The test ends only when it detects an error or it is stopped using csr_ctl.
//
// 6. LPBK3:
// This test is identical to LPBK2 test with two exceptions:
// 1. This test uses only those cache lines that maps to set 0 only. For example, if CSR_SRC_ADDR='hf000 and CSR_NUM_LINES='h4 then it will select cache lines:
//   'hf000 + 'h0*'h400 = 'hf000
//   'hf000 + 'h1*'h400 = 'hf400
//   'hf000 + 'h2*'h400 = 'hf800
//   'hf000 + 'h3*'h400 = 'hfc00
//
// *Note that you select large enough memory space to accomodate CSR_NUM_LINES.
// 2. The ownership classification is based of address bit [10] instead of address bit[0] in lpbk2.
//   cacheline address[10] = 1- Line is owned by FPGA
//   cacheline address[10] = 0- Line is owned by CPU
// 
// The test rules and other constraints are same as LPBK2. 
//
// 7. SW1:
// This test measures the full round trip data movement latency between CPU & FPGA. 
// The test can be configured to use different 4 different CPU to FPGA messaging methods- 
//      a. polling from AFU
//      b. UMsg without Data
//      c. UMsg with Data
//      d. CSR Write
// test flow:
// 1. Wait on test_go
// 2. Start timer. Write N cache lines. WrData= {16{32'h0000_0001}}
// 3. Write Fence.
// 4. FPGA -> CPU Message. Write to address N+1. WrData = {{14{32'h0000_0000}},{64{1'b1}}}
// 5. CPU -> FPGA Message. Configure one of the following methods:
//   a. Poll on Addr N+1. Expected Data [63:32]==32'hffff_ffff
//   b. CSR write to Address 0xB00. Data= Dont Care
//   c. UMsg Mode 0 (with data). UMsg ID = 0
//   d. UMsgH Mode 1 (without data). UMsg ID = 0
// 7. Read N cache lines. Wait for all read completions.
// 6. Stop timer Send test completion.
//


module nlb_lpbk #(parameter TXHDR_WIDTH=61, RXHDR_WIDTH=18, DATA_WIDTH =512)
(
	// ---------------------------global signals-------------------------------------------------
	Clk_32UI,                         //              in    std_logic;  -- Core clock
	Resetb,                           //              in    std_logic;  -- Use SPARINGLY only for control
	// ---------------------------IF signals between SPL and FPL  --------------------------------
	rb2cf_C0RxHdr,                    // [RXHDR_WIDTH-1:0]   cci_intf:           Rx header to SPL channel 0
	rb2cf_C0RxData,                   // [DATA_WIDTH -1:0]   cci_intf:           Rx data response to SPL | no back pressure
	rb2cf_C0RxWrValid,                //                     cci_intf:           Rx write response enable
	rb2cf_C0RxRdValid,                //                     cci_intf:           Rx read response enable
	rb2cf_C0RxCfgValid,               //                     cci_intf:           Rx config response enable
	rb2cf_C0RxUMsgValid,              //                     cci_intf:           Rx UMsg valid
	rb2cf_C0RxIntrValid,                //                     cci_intf:           Rx interrupt valid
	rb2cf_C1RxHdr,                    // [RXHDR_WIDTH-1:0]   cci_intf:           Rx header to SPL channel 1
	rb2cf_C1RxWrValid,                //                     cci_intf:           Rx write response valid
	rb2cf_C1RxIntrValid,                //                     cci_intf:           Rx interrupt valid

	cf2ci_C0TxHdr,                    // [TXHDR_WIDTH-1:0]   cci_intf:           Tx Header from SPL channel 0
	cf2ci_C0TxRdValid,                //                     cci_intf:           Tx read request enable
	cf2ci_C1TxHdr,                    //                     cci_intf:           Tx Header from SPL channel 1
	cf2ci_C1TxData,                   //                     cci_intf:           Tx data from SPL
	cf2ci_C1TxWrValid,                //                     cci_intf:           Tx write request enable
	cf2ci_C1TxIntrValid,              //                     cci_intf:           Tx interrupt valid
	ci2cf_C0TxAlmFull,                //                     cci_intf:           Tx memory channel 0 almost full
	ci2cf_C1TxAlmFull,                //                     cci_intf:           TX memory channel 1 almost full

	ci2cf_InitDn                      // Link initialization is complete
);


input                        Clk_32UI;             //              in    std_logic;  -- Core clock
input                        Resetb;               //              in    std_logic;  -- Use SPARINGLY only for control

input [RXHDR_WIDTH-1:0]      rb2cf_C0RxHdr;        // [RXHDR_WIDTH-1:0]cci_intf:           Rx header to SPL channel 0
input [DATA_WIDTH -1:0]      rb2cf_C0RxData;       // [DATA_WIDTH -1:0]cci_intf:           data response to SPL | no back pressure
input                        rb2cf_C0RxWrValid;    //                  cci_intf:           write response enable
input                        rb2cf_C0RxRdValid;    //                  cci_intf:           read response enable
input                        rb2cf_C0RxCfgValid;   //                  cci_intf:           config response enable
input                        rb2cf_C0RxUMsgValid;  //                  cci_intf:           Rx UMsg valid
input                        rb2cf_C0RxIntrValid;    //                  cci_intf:           interrupt response enable
input [RXHDR_WIDTH-1:0]      rb2cf_C1RxHdr;        // [RXHDR_WIDTH-1:0]cci_intf:           Rx header to SPL channel 1
input                        rb2cf_C1RxWrValid;    //                  cci_intf:           write response valid
input                        rb2cf_C1RxIntrValid;    //                  cci_intf:           interrupt response valid

output [TXHDR_WIDTH-1:0]     cf2ci_C0TxHdr;        // [TXHDR_WIDTH-1:0]cci_intf:           Tx Header from SPL channel 0
output                       cf2ci_C0TxRdValid;    //                  cci_intf:           Tx read request enable
output [TXHDR_WIDTH-1:0]     cf2ci_C1TxHdr;        //                  cci_intf:           Tx Header from SPL channel 1
output [DATA_WIDTH -1:0]     cf2ci_C1TxData;       //                  cci_intf:           Tx data from SPL
output                       cf2ci_C1TxWrValid;    //                  cci_intf:           Tx write request enable
output                       cf2ci_C1TxIntrValid;  //                  cci_intf:           Tx interrupt valid
input                        ci2cf_C0TxAlmFull;    //                  cci_intf:           Tx memory channel 0 almost full
input                        ci2cf_C1TxAlmFull;    //                  cci_intf:           TX memory channel 1 almost full

input                        ci2cf_InitDn;         //                  cci_intf:           Link initialization is complete

localparam      PEND_THRESH = 8;
localparam      ADDR_LMT    = 32;
localparam      MDATA       = 'd11;
//--------------------------------------------------------
// Test Modes
//--------------------------------------------------------
localparam              M_LPBK1         = 3'b000;
localparam              M_READ          = 3'b001;
localparam              M_WRITE         = 3'b010;
localparam              M_TRPUT         = 3'b011;
localparam              M_LPBK2         = 3'b101;
localparam              M_LPBK3         = 3'b110;
//--------------------------------------------------------

wire                         Clk_32UI;
wire                         Resetb;

wire [RXHDR_WIDTH-1:0]       rb2cf_C0RxHdr;
wire [DATA_WIDTH -1:0]       rb2cf_C0RxData;
wire                         rb2cf_C0RxWrValid;
wire                         rb2cf_C0RxRdValid;
wire                         rb2cf_C0RxCfgValid;
wire                         rb2cf_C0RxUMsgValid;
wire [RXHDR_WIDTH-1:0]       rb2cf_C1RxHdr;
wire                         rb2cf_C1RxWrValid;

wire [TXHDR_WIDTH-1:0]       cf2ci_C0TxHdr;
wire                         cf2ci_C0TxRdValid;
wire [TXHDR_WIDTH-1:0]       cf2ci_C1TxHdr;
wire [DATA_WIDTH -1:0]       cf2ci_C1TxData;
wire                         cf2ci_C1TxWrValid;
wire                         cf2ci_C1TxIntrValid;

wire                         ci2cf_InitDn;

wire [ADDR_LMT-1:0]          ab2re_WrAddr;
wire [13:0]                  ab2re_WrTID;
wire [DATA_WIDTH -1:0]       ab2re_WrDin;
wire                         ab2re_WrEn;
wire                         re2ab_WrSent;
wire                         re2ab_WrAlmFull;
wire [ADDR_LMT-1:0]          ab2re_RdAddr;
wire [13:0]                  ab2re_RdTID;
wire                         ab2re_RdEn;
wire                         re2ab_RdSent;
wire                         re2ab_RdRspValid;
wire                         re2ab_UMsgValid;
wire                         re2ab_CfgValid;
wire [13:0]                  re2ab_RdRsp;
wire [DATA_WIDTH -1:0]       re2ab_RdData;
wire                         re2ab_stallRd;
wire                         re2ab_WrRspValid;
wire [13:0]                  re2ab_WrRsp;
wire                         re2xy_go;
wire [31:0]                  re2xy_src_addr;
wire [31:0]                  re2xy_dst_addr;
wire [31:0]                  re2xy_NumLines;
wire [31:0]                  re2xy_addr_reset;
wire [31:0]                  re2xy_read_offset;
wire [31:0]                  re2xy_write_offset;
wire [31:0]                  re2xy_my_config1;
wire [31:0]                  re2xy_my_config2;
wire [31:0]                  re2xy_my_config3;
wire [31:0]                  re2xy_my_config4;
wire [31:0]                  re2xy_my_config5;
wire                         re2xy_Cont;
wire [7:0]                   re2xy_test_cfg;
wire [2:0]                   re2ab_Mode;
wire                         ab2re_TestCmp;
wire [255:0]                 ab2re_ErrorInfo;
wire                         ab2re_ErrorValid;

wire                         test_Resetb;

wire ab2re_WrFence;
assign ab2re_WrFence = 1'b0;

requestor #(.PEND_THRESH(PEND_THRESH),
	.ADDR_LMT   (ADDR_LMT),
	.TXHDR_WIDTH(TXHDR_WIDTH),
	.RXHDR_WIDTH(RXHDR_WIDTH),
	.DATA_WIDTH (DATA_WIDTH )
	)
requestor(
//      ---------------------------global signals-------------------------------------------------
Clk_32UI               ,        //                       in    std_logic;  -- Core clock
Resetb                 ,        //                       in    std_logic;  -- Use SPARINGLY only for control
//      ---------------------------CCI IF signals between CCI and requestor  ---------------------
cf2ci_C0TxHdr,                  //   [TXHDR_WIDTH-1:0]  cci_top:         Tx hdr
cf2ci_C0TxRdValid,              //                      cci_top:         Tx hdr is valid
cf2ci_C1TxHdr,                  //   [TXHDR_WIDTH-1:0]  cci_top:         Tx hdr
cf2ci_C1TxData,                 //                      cci_top:         Tx data
cf2ci_C1TxWrValid,              //                      cci_top:         Tx hdr is valid
cf2ci_C1TxIntrValid,            //                      cci_top:         Tx Interrupt valid

rb2cf_C0RxHdr,                  //  [TXHDR_WIDTH-1:0]   cci_rb:          Rx hdr
rb2cf_C0RxData,                 //  [DATA_WIDTH -1:0]   cci_rb:          Rx data
rb2cf_C0RxWrValid,              //                      cci_rb:          Rx hdr is valid
rb2cf_C0RxRdValid,              //                      cci_rb:          Rx hdr is valid
rb2cf_C0RxCfgValid,             //                      cci_rb:          Rx hdr is valid
rb2cf_C0RxUMsgValid,            //                      cci_intf:        Rx UMsg valid
rb2cf_C0RxIntrValid,              //                      cci_intf:        Rx interrupt valid
rb2cf_C1RxHdr,                  //  [TXHDR_WIDTH-1:0]   cci_rb:          Rx hdr
rb2cf_C1RxWrValid,              //                      cci_rb:          Rx hdr is valid
rb2cf_C1RxIntrValid,              //                      cci_intf:        Rx interrupt valid

ci2cf_C0TxAlmFull,              //                      cci_top:         Tx channel is almost full
ci2cf_C1TxAlmFull,              //                      cci_top:         Tx channel is almost full
ci2cf_InitDn,                   //                                       Link initialization is complete

ab2re_WrAddr,                   // [ADDR_LMT-1:0]        arbiter:        Writes are guaranteed to be accepted
ab2re_WrTID,                    // [13:0]                arbiter:        meta data
ab2re_WrDin,                    // [DATA_WIDTH -1:0]     arbiter:        Cache line data
ab2re_WrFence,              //                       arbiter:        write fence
ab2re_WrEn,                     //                       arbiter:        write enable
re2ab_WrSent,                   //                       arbiter:        write issued
re2ab_WrAlmFull,                //                       arbiter:        write fifo almost full

ab2re_RdAddr,                   // [ADDR_LMT-1:0]        arbiter:        Reads may yield to writes
ab2re_RdTID,                    // [13:0]                arbiter:        meta data
ab2re_RdEn,                     //                       arbiter:        read enable
re2ab_RdSent,                   //                       arbiter:        read issued

re2ab_RdRspValid,               //                       arbiter:        read response valid
re2ab_UMsgValid,                //                       arbiter:        UMsg valid
re2ab_CfgValid,                 //                       arbiter:        Cfg Valid
re2ab_RdRsp,                    // [ADDR_LMT-1:0]        arbiter:        read response header
re2ab_RdData,                   // [DATA_WIDTH -1:0]     arbiter:        read data
re2ab_stallRd,                  //                       arbiter:        stall read requests FOR LPBK1

re2ab_WrRspValid,               //                       arbiter:        write response valid
re2ab_WrRsp,                    // [ADDR_LMT-1:0]        arbiter:        write response header
re2xy_go,                       //                       requestor:      start the test
re2xy_NumLines,                 // [31:0]                requestor:      number of cache lines
re2xy_addr_reset,
re2xy_read_offset,
re2xy_write_offset,
re2xy_my_config1,
re2xy_my_config2,
re2xy_my_config3,
re2xy_my_config4,
re2xy_my_config5,
re2xy_Cont,                     //                       requestor:      continuous mode
re2xy_src_addr,                 // [31:0]                requestor:      src address
re2xy_dst_addr,                 // [31:0]                requestor:      destination address
re2xy_test_cfg,                 // [7:0]                 requestor:      8-bit test cfg register.
re2ab_Mode,                     // [2:0]                 requestor:      test mode

ab2re_TestCmp,                  //                       arbiter:        Test completion flag
ab2re_ErrorInfo,                // [255:0]               arbiter:        error information
ab2re_ErrorValid,               //                       arbiter:        test has detected an error
test_Resetb                     //                       requestor:      rest the app
);


top #(
	.ADDRESS_WIDTH(ADDR_LMT))
top (
	.clk(Clk_32UI),
	.resetn(test_Resetb),

	.read_request(ab2re_RdEn),
	.read_request_accept(re2ab_RdSent),
	.read_request_address(ab2re_RdAddr),
	.read_request_transactionID(ab2re_RdTID),

	.read_response(re2ab_RdRspValid),
	.read_response_data(re2ab_RdData),
	.read_response_config(re2ab_CfgValid),
	.read_response_transactionID(re2ab_RdRsp),

	.write_request(ab2re_WrEn),
	.write_request_address(ab2re_WrAddr),
	.write_request_data(ab2re_WrDin),
	.write_request_transactionID(ab2re_WrTID),
	.write_request_almostfull(re2ab_WrAlmFull),

	.write_response(re2ab_WrRspValid),
	.write_response_transactionID(re2ab_WrRsp),

	.start(re2xy_go),
	.done(ab2re_TestCmp),

	.src_addr(re2xy_src_addr),
	.dst_addr(re2xy_dst_addr),
	.number_of_CL_to_process(re2xy_NumLines),
	.addr_reset(re2xy_addr_reset),
	.read_offset(re2xy_read_offset),
	.write_offset(re2xy_write_offset),
	.config1(re2xy_my_config1),
	.config2(re2xy_my_config2),
	.config3(re2xy_my_config3),
	.config4(re2xy_my_config4),
	.config5(re2xy_my_config5)
);

endmodule
