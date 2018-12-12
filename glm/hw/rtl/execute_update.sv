`include "cci_mpf_if.vh"
`include "glm_common.vh"

module execute_update
(
    input  logic clk,
    input  logic reset,

    input  logic op_start,
    output logic op_done,

    input logic [31:0] regs0,
    input logic [31:0] regs1,

    output bram_request memory1_request,
    input bram_read memory1_read,
    output bram_write memory1_write,

    output logic dot_fifo_tready,
    input logic dot_fifo_tvalid,
    input logic [31:0] dot_fifo_tdata,

    output logic samples_fifo_tready,
    input logic samples_fifo_tvalid,
    input logic [511:0] samples_fifo_tdata,

    output clfifo_write model_forward_fifo_write
);

    typedef enum logic [1:0]
    {
        STATE_IDLE,
        STATE_DOT_GET,
        STATE_MAIN,
        STATE_DONE
    } t_updatestate;
    t_updatestate update_state;

    logic multiply_trigger;
    logic [31:0] multiply_scalar;
    logic [511:0] multiply_vector;
    logic multiply_valid;
    logic [511:0] multiply_result;
    float_scalar_vector_mult
    #(
        .VALUES_PER_LINE(16)
    )
    multiply
    (
        .clk,
        .resetn(!reset),
        .trigger(multiply_trigger),
        .scalar(multiply_scalar),
        .vector(multiply_vector),
        .result_valid(multiply_valid),
        .result(multiply_result)
    );
    assign multiply_trigger = samples_fifo_tvalid && samples_fifo_tready;
    assign multiply_vector = samples_fifo_tdata;

    logic subtract_trigger;
    logic [511:0] subtract_vector1;
    logic [511:0] subtract_vector2;
    logic subtract_valid;
    logic [511:0] subtract_result;
    float_vector_subtract
    #(
        .VALUES_PER_LINE(16)
    )
    subtract
    (
        .clk,
        .resetn(!reset),
        .trigger(subtract_trigger),
        .vector1(subtract_vector1),
        .vector2(subtract_vector2),
        .result_valid(subtract_valid),
        .result(subtract_result)
    );
    
    logic [15:0] memory1_access_offset;
    logic [15:0] memory1_access_length;
    logic write_to_model_forward;

    logic [15:0] num_lines_multiplied_requested;
    logic [15:0] num_lines_multiplied;
    logic [15:0] num_lines_subtracted;

    logic memory1_read_valid_1d;
    logic [511:0] memory1_read_data_1d;

    always_ff @(posedge clk)
    begin
        memory1_read_valid_1d <= memory1_read.valid;
        memory1_read_data_1d <= memory1_read.rdata;

        if (reset)
        begin
            update_state <= STATE_IDLE;
            num_lines_multiplied_requested <= 16'b0;
            num_lines_multiplied <= 16'b0;
            num_lines_subtracted <= 16'b0;
            subtract_trigger <= 1'b0;

            dot_fifo_tready <= 1'b0;
            samples_fifo_tready <= 1'b0;
            memory1_request.re <= 1'b0;
            memory1_write.we <= 1'b0;
            model_forward_fifo_write.we <= 1'b0;
            op_done <= 1'b0;
        end
        else
        begin
            subtract_trigger <= 1'b0;
            dot_fifo_tready <= 1'b0;
            samples_fifo_tready <= 1'b0;
            memory1_request.re <= 1'b0;
            memory1_write.we <= 1'b0;
            model_forward_fifo_write.we <= 1'b0;
            op_done <= 1'b0;

            case(update_state)
                STATE_IDLE:
                begin
                    num_lines_multiplied_requested <= 16'b0;
                    num_lines_multiplied <= 16'b0;
                    num_lines_subtracted <= 16'b0;
                    if (op_start)
                    begin
                        memory1_access_offset <= regs0[15:0];
                        memory1_access_length <= regs0[31:16];
                        write_to_model_forward <= regs1[0];
                        update_state <= STATE_DOT_GET;
                    end
                end

                STATE_DOT_GET:
                begin
                    if (dot_fifo_tvalid)
                    begin
                        dot_fifo_tready <= 1'b1;
                        multiply_scalar <= dot_fifo_tdata;
                        update_state <= STATE_MAIN;
                    end
                end

                STATE_MAIN:
                begin
                    if ( (num_lines_multiplied_requested < memory1_access_length) && samples_fifo_tvalid)
                    begin
                        samples_fifo_tready <= 1'b1;
                        num_lines_multiplied_requested <= num_lines_multiplied_requested + 1;
                    end

                    if (samples_fifo_tvalid && samples_fifo_tready)
                    begin
                        memory1_request.re <= 1'b1;
                        memory1_request.raddr <= memory1_access_offset + num_lines_multiplied;
                        num_lines_multiplied <= num_lines_multiplied + 1;
                    end

                    if (memory1_read_valid_1d)
                    begin
                        subtract_trigger <= 1'b1;
                        subtract_vector1 <= memory1_read_data_1d;
                        subtract_vector2 <= multiply_result;
                    end

                    if (write_to_model_forward)
                    begin
                        model_forward_fifo_write.we <= subtract_valid;
                        model_forward_fifo_write.wdata <= subtract_result;
                    end

                    if (subtract_valid)
                    begin
                        memory1_write.we <= 1'b1;
                        memory1_write.waddr <= memory1_access_offset + num_lines_subtracted;
                        memory1_write.wdata <= subtract_result;
                        num_lines_subtracted <= num_lines_subtracted + 1;
                        if (num_lines_subtracted == memory1_access_length-1)
                        begin
                            update_state <= STATE_DONE;
                        end
                    end
                end

                STATE_DONE:
                begin
                    op_done <= 1'b1;
                    update_state <= STATE_IDLE;
                end

            endcase
        end
    end

endmodule // execute_update