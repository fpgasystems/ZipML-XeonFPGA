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
//
// Engineer:            Pratik Marolia
// Create Date:         Fri Jul 29 14:45:20 PDT 2011
// Module Name:         nlb_sbv_gfifo.v
// Project:             NLB AFU 
// Description:
//
// ***************************************************************************
//  
//---------------------------------------------------------------------------------------------------------------------------------------------------
//                                        sbv_gfifo with Read Store & Read-Write forwarding
//---------------------------------------------------------------------------------------------------------------------------------------------------
//	       The read engine in the fifo presents valid data on output ports. When data out is used, rdack should be asserted.
//	       This is different from a traditional fifo where the fifo pops out a new data in response to a rden in the previous clk.
//	       Instead this fifo presents the valid data on output port & expects a rdack in the same clk when data is consumed.
//
// Read latency  = 1
// Write latency = 1
//
// Bypass Logic = 0     - illegal to read an empty fifo
//              = 1     - reading an empty fifo, returns the data from the write data port (fifo_din). 1 clk delay
//
// Read Store   = 0     - fifo_dout valid for only 1 clock after read enable
//              = 1     - fifo_dout stores the last read value. The value chanegs on next fifo read.
//
// Full thresh          - value should be less than/ euqual to 2**DEPTH_BASE2. If # entries more than threshold than fifo_almFull is set
//
//

`include "nlb_cfg_pkg.vh"
module nlb_sbv_gfifo #(parameter DATA_WIDTH      =51,
                       parameter DEPTH_BASE2     =3, 
                       parameter BYPASS_LOGIC    =0,          // Read-write forwarding, enabling this adds significant delay on write control path
                       parameter FULL_THRESH     =7,          // fifo_almFull will be asserted if there are more entries than FULL_THRESH
                       parameter GRAM_STYLE       =`GRAM_AUTO) // GRAM_AUTO, GRAM_BLCK, GRAM_DIST
                (
                Resetb,            //Active low reset
                Clk,               //global clock
                fifo_din,          //Data input to the FIFO tail
                fifo_wen,          //Write to the tail
                fifo_rdack,        //Read ack, pop the next entry
                                   //--------------------- Output  ------------------
                fifo_dout,         //FIFO read data out     
                fifo_dout_v,       //FIFO data out is valid 
                fifo_empty,        //FIFO is empty
                fifo_full,         //FIFO is full
                fifo_count,        //Number of entries in the FIFO
                fifo_almFull       //fifo_count > FULL_THRESH
                ); 

input                    Resetb;           // Active low reset
input                    Clk;              // global clock    
input  [DATA_WIDTH-1:0]  fifo_din;         // FIFO write data in
input                    fifo_wen;         // FIFO write enable
input                    fifo_rdack;       // Read ack, pop the next entry

output [DATA_WIDTH-1:0]  fifo_dout;        // FIFO read data out
output                   fifo_dout_v;      // FIFO data out is valid 
output                   fifo_empty;       // set when FIFO is empty
output                   fifo_full;        // set if Fifo full
output [DEPTH_BASE2-1:0] fifo_count;       // Number of entries in the fifo
output                   fifo_almFull;
//------------------------------------------------------------------------------------

localparam               READ_STORE = 1;	// Holds output data until next rdack

reg                      fifo_wen_x;
reg    [DATA_WIDTH-1:0]  rd_data_q;
reg                      bypass_en;
reg                      rd_saved;
reg                      fifo_dout_v;

wire                     fifo_almFull;
wire                     fifo_empty_x;
wire   [DATA_WIDTH-1:0]  rd_data;
wire   [DATA_WIDTH-1:0]  fifo_din;
wire                     fifo_ren 	= fifo_rdack 
                                         |!fifo_dout_v;
wire                     fifo_ren_x     = fifo_ren & !fifo_empty_x;
wire   [DATA_WIDTH-1:0]  fifo_dout      = (BYPASS_LOGIC & bypass_en)
                                         |(READ_STORE   & rd_saved ) ? rd_data_q
                                                                     : rd_data;
wire			 fifo_empty 	= fifo_empty_x 
                                         &!fifo_dout_v;
   
always @(*)
begin
        if(BYPASS_LOGIC)        // Data forwarding Enabled - reading an empty fifo, returns the data from the write data port (fifo_din)
                fifo_wen_x = fifo_wen & !(fifo_empty_x & fifo_ren);
        else                    // Data forwarding Disabled
                fifo_wen_x = fifo_wen;
end

always @(posedge Clk)
begin
        if(!Resetb)
          begin
                fifo_dout_v <= 0;
          end
        else
          begin
                case(1)    // synthesis parallel_case
                        (BYPASS_LOGIC & fifo_wen & fifo_ren & fifo_empty_x): fifo_dout_v <= 1;
                        (fifo_ren & !fifo_empty_x)                         : fifo_dout_v <= 1;
                        (!fifo_ren)                                        : fifo_dout_v <= 1;
                        default                                            : fifo_dout_v <= 0;
                endcase

          end // Resetb
          
                if( BYPASS_LOGIC                               // Allows reading an empty fifo
                  & fifo_empty_x                               // Fifo is transparent, it forwards the input data
                  & fifo_ren )                                 // to the output data port. 1 clk delay
                  begin
                        rd_data_q        <= fifo_din;
                        bypass_en        <= 1;
                  end
                 else
                 begin
                        bypass_en        <= 0;
                        if(READ_STORE)
                        begin                                 
                                rd_data_q    <= fifo_dout;
                        end
                  end

                  if ( READ_STORE
                     & fifo_ren  )rd_saved   <= 0;
                  else            rd_saved   <= 1;

          
end

//---------------------------------------------------------------------------------------------------------------------
//              Module instantiations
//---------------------------------------------------------------------------------------------------------------------

wire    fifo_overflow;
wire    fifo_underflow;
wire    fifo_valid;

nlb_gfifo_v         #( .BUS_SIZE_ADDR   (DEPTH_BASE2),     // number of bits of address bus
                   .BUS_SIZE_DATA   (DATA_WIDTH ),     // number of bits of data bus
                   .PROG_FULL_THRESH(FULL_THRESH),     // prog_full will be asserted if there are more entries than PROG_FULL_THRESH 
                   .GRAM_STYLE       (GRAM_STYLE  ))     // GRAM_AUTO, GRAM_BLCK, GRAM_DIST
inst_nlb_gfifo_v
                (                
                .rst_x    (Resetb),           // input   reset, reset polarity defined by SYNC_RESET_POLARITY
                .clk      (Clk),              // input   clock
                .wr_data  (fifo_din),         // input   write data with configurable width
                .wr_en    (fifo_wen_x),       // input   write enable
                .overflow (fifo_overflow),    // output  overflow being asserted indicates one incoming data gets discarded
                .rd_en    (fifo_ren_x),       // input   read enable
                .rd_data  (rd_data),          // output  read data with configurable width
                .valid    (fifo_valid),       // output  active valid indicate valid read data
                .underflow(fifo_underflow),   // output  underflow being asserted indicates one unsuccessfully read
                .empty    (fifo_empty_x),     // output  FIFO empty
                .full     (fifo_full),        // output  FIFO full
                .count    (fifo_count),       // output  FIFOcount
                .prog_full(fifo_almFull)
                );

//---------------------------------------------------------------------------------------------------------------------
//              Error Logic
//---------------------------------------------------------------------------------------------------------------------
/*synthesis translate_off */
always @(*)
begin
        if(BYPASS_LOGIC==0)
        if(fifo_underflow)      $display("%m ERROR: fifo underflow detected");
                
        if(fifo_overflow)       $display("%m ERROR: fifo overflow detected");
end
/*synthesis translate_on */
endmodule 



