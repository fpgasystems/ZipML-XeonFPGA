`include "cci_mpf_if.vh"
`include "glm_common.vh"

module execute_writeback
(
    input  logic clk,
    input  logic reset,

    input  logic op_start,
    output logic op_done,

    input logic [31:0] regs [NUM_REGS],
    input t_ccip_clAddr in_addr,
    input t_ccip_clAddr out_addr,

    output bram_request memory1_request,
    output bram_request memory2_request,
    input bram_read memory1_read,
    input bram_read memory2_read,

    // CCI-P request/response
    input  logic c1TxAlmFull,
    input  t_if_ccip_c1_Rx cp2af_sRx_c1,
    output t_if_ccip_c1_Tx af2cp_sTx_c1
);

    typedef enum logic [1:0]
    {
        STATE_IDLE,
        STATE_WRITE,
        STATE_DONE
    } t_writestate;
    t_writestate send_state;
    t_writestate ack_state;

    logic which_DRAM;
    t_ccip_clAddr DRAM_store_offset;
    logic [15:0] DRAM_store_length;

    logic [15:0] memory_load_offset;
    logic which_memory = 1'b0;

    t_cci_c1_ReqMemHdr wr_hdr;
    always_comb
    begin
        wr_hdr = t_cci_c1_ReqMemHdr'(0);
        // Write request type
        wr_hdr.req_type = eREQ_WRLINE_I;
        // Let the FIU pick the channel
        wr_hdr.vc_sel = eVC_VA;
        // Write 1 line
        wr_hdr.cl_len = eCL_LEN_1;
        // Start of packet is true (single line write)
        wr_hdr.sop = 1'b1;
    end

    // Counters
    logic [15:0] num_send_lines;
    logic [15:0] num_sent_lines;
    logic [15:0] num_ack_lines;

    bram_request memory_request;
    bram_read memory_read;

    always_comb
    begin
        case(which_memory)
            1'b0:
            begin
                memory1_request = memory_request;
            end

            1'b1:
            begin
                memory2_request = memory_request;
            end
        endcase
    end
    assign memory_read = (which_memory == 1'b0) ? memory1_read : memory2_read;

    always_ff @(posedge clk)
    begin
        if (reset)
        begin
            send_state <= STATE_IDLE;
            ack_state <= STATE_IDLE;
            num_send_lines <= 16'b0;
            num_sent_lines <= 16'b0;
            num_ack_lines <= 16'b0;
            memory_request.re <= 1'b0;
            af2cp_sTx_c1.valid <= 1'b0;
            op_done <= 1'b0;
        end
        else
        begin
            // =================================
            //
            //   Send State Machine
            //
            // =================================
            memory_request.re <= 1'b0;
            af2cp_sTx_c1.valid <= 1'b0;
            case(send_state)
                STATE_IDLE:
                begin
                    if (op_start)
                    begin
                        which_DRAM <= regs[6][1];
                        DRAM_store_offset <= (regs[6][1] == 1'b0) ? out_addr + regs[3] : in_addr + regs[3];
                        DRAM_store_length <= regs[4];
                        memory_load_offset <= regs[5];
                        which_memory <= regs[6][0];
                        num_send_lines <= 16'b0;
                        num_sent_lines <= 16'b0;
                        send_state <= STATE_WRITE;
                    end
                end

                STATE_WRITE:
                begin
                    if (num_send_lines < DRAM_store_length && !c1TxAlmFull)
                    begin
                        memory_request.re <= 1'b1;
                        memory_request.raddr <= memory_load_offset + num_send_lines;
                        num_send_lines <= num_send_lines + 1;
                        
                    end

                    if (memory_read.valid)
                    begin
                        af2cp_sTx_c1.valid <= 1'b1;
                        af2cp_sTx_c1.data <= memory_read.rdata;
                        af2cp_sTx_c1.hdr <= wr_hdr;
                        af2cp_sTx_c1.hdr.address <= DRAM_store_offset + num_sent_lines;
                        num_sent_lines <= num_sent_lines + 1;
                        if (num_sent_lines == DRAM_store_length-1)
                        begin
                            send_state <= STATE_DONE;
                        end
                    end
                end

                STATE_DONE:
                begin
                    send_state <= STATE_IDLE;
                end
            endcase

            // =================================
            //
            //   Ack State Machine
            //
            // =================================
            op_done <= 1'b0;
            case(ack_state)
                STATE_IDLE:
                begin
                    if (op_start)
                    begin
                        num_ack_lines <= 16'b0;
                        ack_state <= STATE_WRITE;
                    end
                end

                STATE_WRITE:
                begin
                    if (cp2af_sRx_c1.rspValid)
                    begin
                        num_ack_lines <= num_ack_lines + 1;
                        if (num_ack_lines == DRAM_store_length-1)
                        begin
                            ack_state <= STATE_DONE;
                        end
                    end
                end

                STATE_DONE:
                begin
                    op_done <= 1'b1;
                    ack_state <= STATE_IDLE;
                end
            endcase

        end
    end


endmodule // execute_writeback