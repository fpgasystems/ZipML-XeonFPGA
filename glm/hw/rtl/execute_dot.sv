`include "cci_mpf_if.vh"
`include "glm_common.vh"

module execute_dot
(
    input  logic clk,
    input  logic reset,

    input  logic op_start,
    output logic op_done,

    input logic [31:0] regs0,
    input logic [31:0] regs1,

    output bram_request memory1_request,
    input bram_read memory1_read,
    output bram_request memory2_request,
    input bram_read memory2_read,

    output logic prefetch_fifo_tready,
    input logic prefetch_fifo_tvalid,
    input logic [511:0] prefetch_fifo_tdata,
    output logic model_forward_fifo_tready,
    input logic model_forward_fifo_tvalid,
    input logic [511:0] model_forward_fifo_tdata,

    output logic [31:0] dot_reg
);

    typedef enum logic [1:0]
    {
        STATE_IDLE,
        STATE_READ,
        STATE_DONE
    } t_dotstate;
    t_dotstate dot_state;

    
    clfifo_access leftoperand_fifo_access;
    normal2axis_fifo
    #(
        .FIFO_WIDTH(512),
        .LOG2_FIFO_DEPTH(LOG2_PREFETCH_SIZE)
    )
    leftoperand_fifo
    (
        .clk,
        .resetn(!reset),
        .write_enable(leftoperand_fifo_access.write.we),
        .write_data(leftoperand_fifo_access.write.wdata),
        .m_axis_tvalid(leftoperand_fifo_access.read.re_tvalid),
        .m_axis_tready(leftoperand_fifo_access.read.re_tready),
        .m_axis_tdata(leftoperand_fifo_access.read.re_tdata),
        .almostfull(leftoperand_fifo_access.status.almostfull),
        .count(leftoperand_fifo_access.status.count[LOG2_PREFETCH_SIZE-1:0])
    );

    clfifo_access rightoperand_fifo_access;
    normal2axis_fifo
    #(
        .FIFO_WIDTH(512),
        .LOG2_FIFO_DEPTH(LOG2_PREFETCH_SIZE)
    )
    rightoperand_fifo
    (
        .clk,
        .resetn(!reset),
        .write_enable(rightoperand_fifo_access.write.we),
        .write_data(rightoperand_fifo_access.write.wdata),
        .m_axis_tvalid(rightoperand_fifo_access.read.re_tvalid),
        .m_axis_tready(rightoperand_fifo_access.read.re_tready),
        .m_axis_tdata(rightoperand_fifo_access.read.re_tdata),
        .almostfull(rightoperand_fifo_access.status.almostfull),
        .count(rightoperand_fifo_access.status.count[LOG2_PREFETCH_SIZE-1:0])
    );

    logic [15:0] num_lines_to_process;
    logic from_prefetch_fifo;
    logic from_model_forward_fifo;
    logic [15:0] memory1_load_offset;
    logic [15:0] memory2_load_offset;

    logic [15:0] num_lines_left;
    logic [15:0] num_lines_right;
    logic [15:0] num_processed_lines;

    logic dot_trigger;
    logic dot_done;
    logic [31:0] dot_result;
    hybrid_dot_product
    #(
        .LOG2_VALUES_PER_LINE(4)
    )
    dot_compute
    (
        .clk,
        .resetn(!reset),
        .trigger(dot_trigger),
        .accumulation_count(32'(num_lines_to_process)),
        .vector1(leftoperand_fifo_access.read.re_tdata),
        .vector2(rightoperand_fifo_access.read.re_tdata),
        .result_valid(dot_done),
        .result(dot_result)
    );

    assign dot_trigger = leftoperand_fifo_access.read.re_tvalid && rightoperand_fifo_access.read.re_tvalid;
    assign leftoperand_fifo_access.read.re_tready = dot_trigger;
    assign rightoperand_fifo_access.read.re_tready = dot_trigger;

    always_ff @(posedge clk)
    begin
        if (reset)
        begin
            dot_state <= STATE_IDLE;
            memory1_request.re <= 1'b0;
            memory2_request.re <= 1'b0;
            prefetch_fifo_tready <= 1'b0;
            model_forward_fifo_tready <= 1'b0;
            op_done <= 1'b0;
        end
        else
        begin

            memory1_request.re <= 1'b0;
            memory2_request.re <= 1'b0;
            prefetch_fifo_tready <= 1'b0;
            model_forward_fifo_tready <= 1'b0;
            op_done <= 1'b0;

            case (dot_state)
                STATE_IDLE:
                begin
                    if (op_start)
                    begin
                        dot_state <= STATE_READ;
                        num_lines_to_process <= regs0[15:0];
                        from_prefetch_fifo <= regs0[16];
                        from_model_forward_fifo <= regs0[17];
                        memory1_load_offset <= regs1[15:0];
                        memory2_load_offset <= regs1[31:16];
                        num_lines_left <= 16'b0;
                        num_lines_right <= 16'b0;
                        num_processed_lines <= 16'b0;
                    end
                end

                STATE_READ:
                begin
                    if (num_lines_left < num_lines_to_process && !leftoperand_fifo_access.status.almostfull )
                    begin
                        if ( from_prefetch_fifo && prefetch_fifo_tvalid )
                        begin
                            prefetch_fifo_tready <= 1'b1;
                        end
                        else if ( !from_prefetch_fifo )
                        begin
                            memory2_request.re <= 1'b1;
                            memory2_request.raddr <= memory2_load_offset + num_lines_left;
                            num_lines_left <= num_lines_left + 1;
                        end
                    end

                    if (num_lines_right < num_lines_to_process && !rightoperand_fifo_access.status.almostfull )
                    begin
                        if ( from_model_forward_fifo && model_forward_fifo_tvalid )
                        begin
                            model_forward_fifo_tready <= 1'b1;
                        end
                        else if ( !from_model_forward_fifo )
                        begin
                            memory1_request.re <= 1'b1;
                            memory1_request.raddr <= memory1_load_offset + num_lines_right;
                            num_lines_right <= num_lines_right + 1;
                        end
                    end

                    // Put lines into local FIFOs
                    leftoperand_fifo_access.write.we <= 1'b0;
                    if ( from_prefetch_fifo && prefetch_fifo_tvalid && prefetch_fifo_tready )
                    begin
                        leftoperand_fifo_access.write.we <= 1'b1;
                        leftoperand_fifo_access.write.wdata <= prefetch_fifo_tdata;
                        num_lines_left <= num_lines_left + 1;
                    end
                    else if ( from_prefetch_fifo == 1'b0 && memory2_read.valid )
                    begin
                        leftoperand_fifo_access.write.we <= 1'b1;
                        leftoperand_fifo_access.write.wdata <= memory2_read.rdata;
                    end

                    rightoperand_fifo_access.write.we <= 1'b0;
                    if ( from_model_forward_fifo && model_forward_fifo_tvalid && model_forward_fifo_tready )
                    begin
                        rightoperand_fifo_access.write.we <= 1'b1;
                        rightoperand_fifo_access.write.wdata <= model_forward_fifo_tdata;
                        num_lines_right <= num_lines_right + 1;
                    end
                    else if ( from_model_forward_fifo == 1'b0 && memory1_read.valid )
                    begin
                        rightoperand_fifo_access.write.we <= 1'b1;
                        rightoperand_fifo_access.write.wdata <= memory1_read.rdata;
                    end


                    if (dot_trigger)
                    begin
                        num_processed_lines <= num_processed_lines + 1;
                        if (num_processed_lines == num_lines_to_process-1)
                        begin
                            dot_state <= STATE_DONE;
                        end
                    end

                    // if ( (num_lines_left == num_lines_to_process) && (num_lines_right == num_lines_to_process) )
                    // begin
                    //     dot_state <= STATE_DONE;
                    // end
                end

                STATE_DONE:
                begin
                    if (dot_done == 1'b1)
                    begin
                        op_done <= 1'b1;
                        dot_reg <= dot_result;
                        dot_state <= STATE_IDLE;
                    end
                end
            endcase

        end
    end

endmodule // execute_dot