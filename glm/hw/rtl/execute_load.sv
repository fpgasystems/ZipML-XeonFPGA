`include "cci_mpf_if.vh"
`include "glm_common.vh"

module execute_load
(
    input  logic clk,
    input  logic reset,

    input  logic op_start,
    output logic op_done,
    output logic op_request_done,

    input logic [31:0] regs [NUM_REGS],
    input t_ccip_clAddr in_addr,

    input clfifo_status samples_fifo_status,
    input clfifo_status prefetch_fifo_status,
    output clfifo_write samples_fifo_write,
    output clfifo_write prefetch_fifo_write,

    output bram_write memory1_write,
    output bram_write memory2_write,

    // CCI-P request/response
    input  c0TxAlmFull,
    input  t_if_ccip_c0_Rx cp2af_sRx_c0,
    output t_if_ccip_c0_Tx af2cp_sTx_c0
);

    typedef enum logic [1:0]
    {
        STATE_IDLE,
        STATE_READ,
        STATE_DONE
    } t_readstate;
    t_readstate request_state;
    t_readstate receive_state;

    t_ccip_clAddr DRAM_load_offset;
    logic [31:0] DRAM_load_length;
    
    logic[15:0] samples_fifo_store_length;
    logic[15:0] prefetch_fifo_store_length;

    typedef struct packed {
        logic[15:0] store_offset;
        logic[15:0] store_length;
    } bram_access_properties;
    bram_access_properties memory1_store;
    bram_access_properties memory2_store;

    t_cci_c0_ReqMemHdr rd_hdr;
    always_comb
    begin
        rd_hdr = t_cci_c0_ReqMemHdr'(0);
        // Read request type
        rd_hdr.req_type = eREQ_RDLINE_I;
        // Let the FIU pick the channel
        rd_hdr.vc_sel = eVC_VA;
        // Read 4 lines (the size of an entry in the list)
        rd_hdr.cl_len = eCL_LEN_1;
    end

    // Counters
    logic [31:0] num_requested_lines;
    logic [31:0] num_received_lines;
    logic [31:0] num_lines_in_flight;
    logic signed [31:0] prefetch_fifo_free_count;
    logic signed [31:0] num_allowed_lines_to_request;

    always_ff @(posedge clk)
    begin
        num_lines_in_flight <= num_requested_lines - num_received_lines;
        prefetch_fifo_free_count <= PREFETCH_SIZE - prefetch_fifo_status.count[LOG2_PREFETCH_SIZE-1:0];
        num_allowed_lines_to_request <= prefetch_fifo_free_count - $signed(num_lines_in_flight);
        if (reset)
        begin
            request_state <= STATE_IDLE;
            receive_state <= STATE_IDLE;
            af2cp_sTx_c0.valid <= 1'b0;
            num_requested_lines <= 32'b0;
            num_received_lines <= 32'b0;
            memory1_write.we <= 1'b0;
            memory2_write.we <= 1'b0;
            samples_fifo_write.we <= 1'b0;
            prefetch_fifo_write.we <= 1'b0;
            op_done <= 1'b0;
            op_request_done <= 1'b0;
        end
        else
        begin
            af2cp_sTx_c0.valid <= 1'b0;
            op_request_done <= 1'b0;
            // =================================
            //
            //   Request State Machine
            //
            // =================================
            case (request_state)
                STATE_IDLE:
                begin
                    if (op_start)
                    begin
                        DRAM_load_offset            <= in_addr + regs[3];
                        DRAM_load_length            <= regs[4];
                        memory1_store.store_offset  <= regs[5][15:0];
                        memory1_store.store_length  <= regs[5][31:16];
                        memory2_store.store_offset  <= regs[6][15:0];
                        memory2_store.store_length  <= regs[6][31:16];
                        samples_fifo_store_length   <= regs[7][15:0];
                        prefetch_fifo_store_length  <= regs[7][31:16];
                        num_requested_lines         <= 32'b0;
                        request_state               <= STATE_READ;
                    end
                end

                STATE_READ:
                begin
                    if (num_requested_lines < DRAM_load_length && !c0TxAlmFull && (num_allowed_lines_to_request > 0) )
                    begin
                        af2cp_sTx_c0.valid <= 1'b1;
                        af2cp_sTx_c0.hdr <= rd_hdr;
                        af2cp_sTx_c0.hdr.address <= DRAM_load_offset + num_requested_lines;

                        num_requested_lines <= num_requested_lines + 1;
                        if (num_requested_lines == DRAM_load_length-1)
                        begin
                            request_state <= STATE_DONE;
                        end
                    end
                    else if (DRAM_load_length == 32'b0)
                    begin
                        request_state <= STATE_DONE;
                    end
                end

                STATE_DONE:
                begin
                    op_request_done <= 1'b1;
                    request_state <= STATE_IDLE;
                end
            endcase


            memory1_write.we <= 1'b0;
            memory2_write.we <= 1'b0;
            samples_fifo_write.we <= 1'b0;
            prefetch_fifo_write.we <= 1'b0;
            op_done <= 1'b0;
            // =================================
            //
            //   Receive State Machine
            //
            // =================================
            case (receive_state)
                STATE_IDLE:
                begin
                    if (op_start)
                    begin
                        num_received_lines <= 32'b0;
                        receive_state <= STATE_READ;
                    end
                end

                STATE_READ:
                begin
                    if (cci_c0Rx_isReadRsp(cp2af_sRx_c0))
                    begin
                        if (num_received_lines < memory1_store.store_length)
                        begin
                            memory1_write.we <= 1'b1;
                            memory1_write.waddr <= memory1_store.store_offset + num_received_lines;
                            memory1_write.wdata <= cp2af_sRx_c0.data;
                        end

                        if (num_received_lines < memory2_store.store_length)
                        begin
                            memory2_write.we <= 1'b1;
                            memory2_write.waddr <= memory2_store.store_offset + num_received_lines;
                            memory2_write.wdata <= cp2af_sRx_c0.data;
                        end

                        if (num_received_lines < samples_fifo_store_length)
                        begin
                            samples_fifo_write.we <= 1'b1;
                            samples_fifo_write.wdata <= cp2af_sRx_c0.data;
                        end

                        if (num_received_lines < prefetch_fifo_store_length)
                        begin
                            prefetch_fifo_write.we <= 1'b1;
                            prefetch_fifo_write.wdata <= cp2af_sRx_c0.data;
                        end

                        num_received_lines <= num_received_lines + 1;
                        if (num_received_lines == DRAM_load_length-1)
                        begin
                            receive_state <= STATE_DONE;
                        end
                    end
                    else if (DRAM_load_length == 32'b0)
                    begin
                        receive_state <= STATE_DONE;
                    end
                end

                STATE_DONE:
                begin
                    op_done <= 1'b1;
                    receive_state <= STATE_IDLE;
                end
            endcase
        end
    end

endmodule // execute_load