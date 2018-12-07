`include "cci_mpf_if.vh"
`include "glm_common.vh"

module execute_modify
(
    input  logic clk,
    input  logic reset,

    input  logic op_start,
    output logic op_done,

    input logic [31:0] regs [NUM_REGS],

    output bram_request memory2_request,
    input bram_read memory2_read,
    output bram_write memory2_write,

    input logic [31:0] dot_reg,

    output wordfifo_write dot_fifo_write
);

    typedef enum logic [2:0]
    {
        STATE_IDLE,
        STATE_REQUEST,
        STATE_READ,
        STATE_SUBTRACT,
        STATE_SUBTRACT_WAIT,
        STATE_MULTIPLY_WAIT,
        STATE_DONE
    } t_modifystate;
    t_modifystate modify_state;

    typedef struct packed {
        logic       trigger;
        logic[31:0] leftoperand;
        logic[31:0] rightoperand;
        logic[31:0] result;
        logic       done;
    } fp_compute_regs;


    localparam MULTIPLY_LATENCY = 3;
    logic [MULTIPLY_LATENCY-1:0] mult_status = 0;
    fp_compute_regs mult_regs;
    always_ff @(posedge clk)
    begin
        mult_status[0] <= mult_regs.trigger;
        for (int i = 1; i < MULTIPLY_LATENCY; i++)
        begin
            mult_status[i] <= mult_status[i-1];
        end
        mult_regs.done <= mult_status[MULTIPLY_LATENCY-1];
    end
    fp_mult_arria10
    multiply
    (
        .clk,
        .areset(reset),
        .a(mult_regs.leftoperand),
        .b(mult_regs.rightoperand),
        .q(mult_regs.result)
    );

    localparam SUBTRACT_LATENCY = 3;
    logic [SUBTRACT_LATENCY-1:0] sub_status = 0;
    fp_compute_regs sub_regs;
    always_ff @(posedge clk)
    begin
        sub_status[0] <= sub_regs.trigger;
        for (int i = 1; i < SUBTRACT_LATENCY; i++)
        begin
            sub_status[i] <= sub_status[i-1];
        end
        sub_regs.done <= sub_status[SUBTRACT_LATENCY-1];
    end
    fp_subtract_arria10
    subtract
    (
        .clk,
        .areset(reset),
        .a(sub_regs.leftoperand),
        .b(sub_regs.rightoperand),
        .q(sub_regs.result)
    );

    logic [15:0] offsetByIndex;
    logic [3:0] positionByIndex;
    logic [15:0] memory2_load_offset;
    logic [15:0] memory2_store_offset;
    logic [1:0] model_type;
    logic algorithm_type;
    logic [31:0] step_size;
    logic [31:0] lambda;

    logic [31:0] scalarFromMemory2;
    logic [511:0] lineFromMemory2;
    logic [31:0] final_result;

    always_ff @(posedge clk)
    begin
        if (reset)
        begin
            modify_state <= STATE_IDLE;
            memory2_request.re <= 1'b0;
            sub_regs.trigger <= 1'b0;
            mult_regs.trigger <= 1'b0;
            dot_fifo_write.we <= 1'b0;
            op_done <= 1'b0;
        end
        else
        begin
            memory2_request.re <= 1'b0;
            sub_regs.trigger <= 1'b0;
            mult_regs.trigger <= 1'b0;
            dot_fifo_write.we <= 1'b0;
            memory2_write.we <= 1'b0;
            op_done <= 1'b0;

            case (modify_state)
                STATE_IDLE:
                begin
                    if (op_start)
                    begin
                        offsetByIndex <= regs[0] >> 4;
                        positionByIndex <= regs[0][3:0];
                        memory2_load_offset <= regs[3][15:0];
                        memory2_store_offset <= regs[3][31:16];
                        model_type <= regs[4][1:0];
                        algorithm_type <= regs[4][2];
                        step_size <= regs[5];
                        lambda <= regs[6];
                        modify_state <= (regs[3][15:0] == 16'hFFFF) ? STATE_SUBTRACT : STATE_REQUEST;
                    end
                end

                STATE_REQUEST:
                begin
                    memory2_request.re <= 1'b1;
                    memory2_request.raddr <= memory2_load_offset + offsetByIndex;
                    modify_state <= STATE_READ;
                end

                STATE_READ:
                begin
                    if (memory2_read.valid)
                    begin
                        lineFromMemory2 <= memory2_read.rdata;
                        scalarFromMemory2 <= memory2_read.rdata[positionByIndex*32+31 -: 32];
                        modify_state <= STATE_SUBTRACT;
                    end
                end

                STATE_SUBTRACT:
                begin
                    sub_regs.trigger <= 1'b1;
                    sub_regs.leftoperand <= dot_reg;
                    sub_regs.rightoperand <= scalarFromMemory2;
                    modify_state <= STATE_SUBTRACT_WAIT;
                end

                STATE_SUBTRACT_WAIT:
                begin
                    if (sub_regs.done)
                    begin
                        mult_regs.trigger <= 1'b1;
                        mult_regs.leftoperand <= step_size;
                        mult_regs.rightoperand <= sub_regs.result;
                        modify_state <= STATE_MULTIPLY_WAIT;
                    end
                end

                STATE_MULTIPLY_WAIT:
                begin
                    if (mult_regs.done)
                    begin
                        modify_state <= STATE_DONE;
                        final_result <= mult_regs.result;
                    end
                end

                STATE_DONE:
                begin
                    dot_fifo_write.we <= 1'b1;
                    dot_fifo_write.wdata <= final_result;
                    if (memory2_store_offset != 16'hFFFF)
                    begin
                        memory2_write.we <= 1'b1;
                        memory2_write.waddr <= memory2_store_offset + offsetByIndex;
                        memory2_write.wdata <= memory2_read.rdata;
                        memory2_write.wdata[positionByIndex*32+31 -: 32] <= final_result;
                    end
                    op_done <= 1'b1;
                    modify_state <= STATE_IDLE;
                end

            endcase
        end
    end

endmodule // execute_modify