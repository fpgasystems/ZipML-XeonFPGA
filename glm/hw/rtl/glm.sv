`include "cci_mpf_if.vh"
`include "csr_mgr.vh"
`include "afu_json_info.vh"
`include "glm_common.vh"

module glm
(
    input  logic clk,
    input  logic reset,

    // CCI-P request/response
    input  t_if_ccip_Rx cp2af_sRx,
    output t_if_ccip_Tx af2cp_sTx,

    // CSR connections
    app_csrs.app csrs,

    // MPF tracks outstanding requests.  These will be true as long as
    // reads or unacknowledged writes are still in flight.
    input  logic c0NotEmpty,
    input  logic c1NotEmpty
);

    bram_interface #(.WIDTH(512), .LOG2_DEPTH(LOG2_PROGRAM_SIZE)) program_access();
    dual_port_ram
    #(
        .DATA_WIDTH(512),
        .ADDR_WIDTH(LOG2_PROGRAM_SIZE)
    )
    proram_memory
    (
        .clk,
        .raddr(program_access.raddr),
        .waddr(program_access.waddr),
        .data(program_access.wdata),
        .we(program_access.we),
        .re(program_access.re),
        .qvalid(program_access.rvalid),
        .q(program_access.rdata)
    );

    // ====================================================================
    //
    //  CSRs (simple connections to the external CSR management engine)
    //
    // ====================================================================
    always_comb
    begin
        // The AFU ID is a unique ID for a given program.  Here we generated
        // one with the "uuidgen" program and stored it in the AFU's JSON file.
        // ASE and synthesis setup scripts automatically invoke afu_json_mgr
        // to extract the UUID into afu_json_info.vh.
        csrs.afu_id = `AFU_ACCEL_UUID;
        // Default
        for (int i = 0; i < NUM_APP_CSRS; i = i + 1)
        begin
            csrs.cpu_rd_csrs[i].data = 64'(0);
        end
    end

    t_ccip_clAddr in_addr;
    t_ccip_clAddr out_addr;
    t_ccip_clAddr program_addr;
    logic [15:0] program_length;
    logic start;

    always_ff @(posedge clk)
    begin
        if (reset)
        begin
            in_addr <= t_ccip_clAddr'(0);
            out_addr <= t_ccip_clAddr'(0);
            program_addr <= t_ccip_clAddr'(0);
            program_length <= 15'b0;
            start <= 1'b0;
        end
        else
        begin
            if (csrs.cpu_wr_csrs[0].en)
            begin
                in_addr <= byteAddrToClAddr(csrs.cpu_wr_csrs[0].data);
            end

            if (csrs.cpu_wr_csrs[1].en)
            begin
                out_addr <= byteAddrToClAddr(csrs.cpu_wr_csrs[1].data);
            end

            if (csrs.cpu_wr_csrs[2].en)
            begin
                program_addr <= byteAddrToClAddr(csrs.cpu_wr_csrs[2].data);
            end

            start <= csrs.cpu_wr_csrs[3].en;
            if (csrs.cpu_wr_csrs[3].en)
            begin
                program_length <= 15'(csrs.cpu_wr_csrs[3].data);
            end

        end
    end

    // =========================================================================
    //
    //   Execute Module Signal Definitions
    //
    // =========================================================================

    logic execute_load_c0TxAlmFull;
    t_if_ccip_c0_Rx execute_load_cp2af_sRx_c0;
    t_if_ccip_c0_Tx execute_load_af2cp_sTx_c0;

    logic execute_writeback_c1TxAlmFull;
    t_if_ccip_c1_Rx execute_writeback_cp2af_sRx_c1;
    t_if_ccip_c1_Tx execute_writeback_af2cp_sTx_c1;

    // =========================================================================
    //
    //   State Definitions
    //
    // =========================================================================

    t_rxtxstate request_state;
    t_rxtxstate receive_state;
    t_machinestate machine_state;

    // =========================================================================
    //
    //   Request/Receive State Machine
    //
    // =========================================================================
    
    logic [15:0] program_length_request;
    logic [15:0] program_length_receive;

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

    always_ff @(posedge clk)
    begin
        if (reset)
        begin
            request_state <= RXTX_STATE_IDLE;
            receive_state <= RXTX_STATE_IDLE;
            af2cp_sTx.c0.valid <= 1'b0;

            program_access.we <= 1'b0;
        end
        else
        begin
            // =================================
            //
            //   Request State Machine
            //
            // =================================
            af2cp_sTx.c0.valid <= 1'b0;
            case (request_state)
                RXTX_STATE_IDLE:
                begin
                    if (start)
                    begin
                        request_state <= RXTX_STATE_PROGRAM_READ;
                        program_length_request <= 15'b0;
                    end
                end

                RXTX_STATE_PROGRAM_READ:
                begin
                    if (program_length_request < program_length && !cp2af_sRx.c0TxAlmFull)
                    begin
                        af2cp_sTx.c0.valid <= 1'b1;
                        af2cp_sTx.c0.hdr <= rd_hdr;
                        af2cp_sTx.c0.hdr.address <= program_addr + program_length_request;
                        program_length_request <= program_length_request + 1;
                        if (program_length_request == program_length - 1)
                        begin
                            request_state <= RXTX_STATE_PROGRAM_EXECUTE;
                        end
                    end
                end

                RXTX_STATE_PROGRAM_EXECUTE:
                begin
                    if (machine_state == MACHINE_STATE_DONE)
                    begin
                        request_state <= RXTX_STATE_DONE;
                    end
                    else
                    begin
                        af2cp_sTx.c0 <= execute_load_af2cp_sTx_c0;
                    end
                end

                RXTX_STATE_DONE:
                begin
                    request_state <= RXTX_STATE_IDLE;
                end
            endcase

            // =================================
            //
            //   Receive State Machine
            //
            // =================================
            program_access.we <= 1'b0;
            case (receive_state)
                RXTX_STATE_IDLE:
                begin
                    if (start)
                    begin
                        receive_state <= RXTX_STATE_PROGRAM_READ;
                        program_length_receive <= 15'b0;
                    end
                end

                RXTX_STATE_PROGRAM_READ:
                begin
                    if (cci_c0Rx_isReadRsp(cp2af_sRx.c0))
                    begin
                        program_access.we <= 1'b1;
                        program_access.waddr <= program_length_receive;
                        program_access.wdata <= cp2af_sRx.c0.data;
                        program_length_receive <= program_length_receive + 1;
                        if (program_length_receive == program_length-1)
                        begin
                            receive_state <= RXTX_STATE_PROGRAM_EXECUTE;
                        end 
                    end
                end

                RXTX_STATE_PROGRAM_EXECUTE:
                begin
                    if (machine_state == MACHINE_STATE_DONE && !cp2af_sRx.c1TxAlmFull)
                    begin
                        receive_state <= RXTX_STATE_DONE;
                    end
                    else
                    begin
                        execute_load_cp2af_sRx_c0 <= cp2af_sRx.c0;
                        execute_load_c0TxAlmFull <= cp2af_sRx.c0TxAlmFull;
                    end
                end

                RXTX_STATE_DONE:
                begin
                    receive_state <= RXTX_STATE_IDLE;
                end
            endcase
        end
    end

    // =========================================================================
    
    //   Write Back
    
    // =========================================================================
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

    always_ff @(posedge clk)
    begin
        if (reset)
        begin
            af2cp_sTx.c1.valid <= 1'b0;
        end
        else
        begin
            af2cp_sTx.c1.valid <= 1'b0;

            if (receive_state == RXTX_STATE_PROGRAM_EXECUTE)
            begin
                execute_writeback_cp2af_sRx_c1 <= cp2af_sRx.c1;
                execute_writeback_c1TxAlmFull <= cp2af_sRx.c1TxAlmFull;
                af2cp_sTx.c1 <= execute_writeback_af2cp_sTx_c1;
            end
            else if (receive_state == RXTX_STATE_DONE)
            begin
                af2cp_sTx.c1.valid <= 1'b1;
                af2cp_sTx.c1.data <= t_ccip_clData'(64'h1);
                af2cp_sTx.c1.hdr <= wr_hdr;
                af2cp_sTx.c1.hdr.address <= out_addr;
            end
        end
    end

    //
    // This AFU never handles MMIO reads.  MMIO is managed in the CSR module.
    //
    assign af2cp_sTx.c2.mmioRdValid = 1'b0;

    // =========================================================================
    //
    //   Local Memories
    //
    // =========================================================================

    bram_access memory1_access;
    dual_port_ram
    #(
        .DATA_WIDTH(512),
        .ADDR_WIDTH(LOG2_MEMORY_SIZE)
    )
    memory1
    (
        .clk,
        .raddr(memory1_access.request.raddr),
        .waddr(memory1_access.write.waddr),
        .data(memory1_access.write.wdata),
        .we(memory1_access.write.we),
        .re(memory1_access.request.re),
        .qvalid(memory1_access.read.valid),
        .q(memory1_access.read.rdata)
    );

    bram_access memory2_access;
    dual_port_ram
    #(
        .DATA_WIDTH(512),
        .ADDR_WIDTH(LOG2_MEMORY_SIZE)
    )
    memory2
    (
        .clk,
        .raddr(memory2_access.request.raddr),
        .waddr(memory2_access.write.waddr),
        .data(memory2_access.write.wdata),
        .we(memory2_access.write.we),
        .re(memory2_access.request.re),
        .qvalid(memory2_access.read.valid),
        .q(memory2_access.read.rdata)
    );

    clfifo_access samples_fifo_access;
    normal2axis_fifo
    #(
        .FIFO_WIDTH(512),
        .LOG2_FIFO_DEPTH(LOG2_MEMORY_SIZE)
    )
    samples_fifo
    (
        .clk,
        .resetn(!reset),
        .write_enable(samples_fifo_access.write.we),
        .write_data(samples_fifo_access.write.wdata),
        .m_axis_tvalid(samples_fifo_access.read.re_tvalid),
        .m_axis_tready(samples_fifo_access.read.re_tready),
        .m_axis_tdata(samples_fifo_access.read.re_tdata),
        .almostfull(samples_fifo_access.status.almostfull),
        .count(samples_fifo_access.status.count[LOG2_MEMORY_SIZE-1:0])
    );

    clfifo_access prefetch_fifo_access;
    normal2axis_fifo
    #(
        .FIFO_WIDTH(512),
        .LOG2_FIFO_DEPTH(LOG2_PREFETCH_SIZE)
    )
    prefetch_fifo
    (
        .clk,
        .resetn(!reset),
        .write_enable(prefetch_fifo_access.write.we),
        .write_data(prefetch_fifo_access.write.wdata),
        .m_axis_tvalid(prefetch_fifo_access.read.re_tvalid),
        .m_axis_tready(prefetch_fifo_access.read.re_tready),
        .m_axis_tdata(prefetch_fifo_access.read.re_tdata),
        .almostfull(prefetch_fifo_access.status.almostfull),
        .count(prefetch_fifo_access.status.count[LOG2_PREFETCH_SIZE-1:0])
    );

    clfifo_access model_forward_fifo_access;
    normal2axis_fifo
    #(
        .FIFO_WIDTH(512),
        .LOG2_FIFO_DEPTH(LOG2_PREFETCH_SIZE)
    )
    model_forward_fifo
    (
        .clk,
        .resetn(!reset),
        .write_enable(model_forward_fifo_access.write.we),
        .write_data(model_forward_fifo_access.write.wdata),
        .m_axis_tvalid(model_forward_fifo_access.read.re_tvalid),
        .m_axis_tready(model_forward_fifo_access.read.re_tready),
        .m_axis_tdata(model_forward_fifo_access.read.re_tdata),
        .almostfull(model_forward_fifo_access.status.almostfull),
        .count(model_forward_fifo_access.status.count[LOG2_PREFETCH_SIZE-1:0])
    );

    wordfifo_access dot_fifo_access;
    normal2axis_fifo
    #(
        .FIFO_WIDTH(32),
        .LOG2_FIFO_DEPTH(LOG2_PREFETCH_SIZE)
    )
    dot_fifo
    (
        .clk,
        .resetn(!reset),
        .write_enable(dot_fifo_access.write.we),
        .write_data(dot_fifo_access.write.wdata),
        .m_axis_tvalid(dot_fifo_access.read.re_tvalid),
        .m_axis_tready(dot_fifo_access.read.re_tready),
        .m_axis_tdata(dot_fifo_access.read.re_tdata),
        .almostfull(dot_fifo_access.status.almostfull),
        .count(dot_fifo_access.status.count[LOG2_PREFETCH_SIZE-1:0])
    );

    bram_write execute_load_memory1_write;
    bram_write execute_load_memory2_write;

    bram_request execute_writeback_memory1_request;
    bram_request execute_writeback_memory2_request;

    bram_request execute_dot_memory1_request;
    bram_request execute_dot_memory2_request;

    bram_request execute_modify_memory2_request;
    bram_write execute_modify_memory2_write;

    bram_request execute_update_memory1_request;
    bram_write execute_update_memory1_write;

    // =========================================================================
    //
    //   Register Machine
    //
    // =========================================================================

    function automatic logic[31:0] updateIndex(logic[31:0] instruction, logic[31:0] regs);
        logic[31:0] result;
        case(instruction)
            32'hFFFFFFFF:
            begin
                result = regs;
            end

            32'h0FFFFFFF:
            begin
                result = regs + 1;
            end

            32'h01FFFFFF:
            begin
                result = regs - 1;
            end

            default:
            begin
                result = instruction;
            end
        endcase
        return result;
    endfunction

    // register file
    // reg[0]: update index
    // reg[1]: partition index
    // reg[2]: epoch index

    //  if instruction[0,1,2] == 0xFFFFFFFF
    //      reg[0,1,2] = reg[0,1,2]
    //  else if instruction[0,1,2] == 0xFFFFFFF
    //      reg[0,1,2] = reg[0,1,2]+1
    //  else if instruction[0,1,2] == 0x1FFFFFF
    //      reg[0,1,2] = reg[0,1,2]-1
    //  else
    //      reg[0,1,2] = instruction[0,1,2]

    // ----  ISA
    // opcode = instruction[15][7:0]
    // nonblocking = instruction[15][8]

    // if opcode == 0 ---- load
    // reg[3] = instruction[3]+reg[1]*instruction[11]+reg[0]*instruction[10]    // DRAM read offset in cachelines
                                                                                // instruction[10]: read offset change per update
                                                                                // instruction[11]: read offset change per partiton (partition size)
    // reg[4] = instruction[4]                                                  // DRAM read length in cachelines
    // reg[5] = instruction[5]                                                  // [15:0]: memory1 store offset in cachelines
                                                                                // [15:0]: memory1 store length in cachelines
    // reg[6] = instruction[6]                                                  // [15:0]: memory2 store offset in cachelines
                                                                                // [15:0]: memory2 store length in cachelines
    // reg[7] = instruction[7]                                                  // [15:0]: samples_fifo store length in cachelines
                                                                                // [31:16]: prefetch_fifo store length in cachelines

    // if opcode == 1 ---- dot
    // reg[3] = instruction[3]                                                  // [15:0] Read length in cachelines
                                                                                // [16] Read from memory2 (0) or prefetch_fifo (1)
                                                                                // [17] Read from memory1 (0) or model_forward_fifo (1)
    // reg[4] = instruction[4]                                                  // [15:0] memory1 load offset in cachelines
                                                                                // [31:16] memory2 load offset in cachelines

    // if opcode == 2 ---- modify
    // reg[3] = instruction[3]                                                  // [15:0]: memory2 load offset in cachelines
                                                                                // [31:16]: memory2 store offset in cachelines
    // reg[4] = instruction[4]                                                  // [1:0]: (0 linreg) (1 logreg) (2 SVM)
                                                                                // [2]: (0 SGD) (1 SCD)
    // reg[5] = instruction[5]                                                  // step size
    // reg[6] = instruction[6]                                                  // lambda

    // if opcode == 3 ---- update
    // reg[3] = instruction[3]                                                  // [15:0] memory1 load/store offset in cacheline
                                                                                // [31:16] memory1 load/store length in cachelines
    // reg[4] = instruction[4]                                                  // [0] write to model_forward_fifo

    // if opcode == 4 ---- writeback
    // reg[3] = instruction[3]+reg[1]*instruction[11]+reg[2]*instruction[10]    // DRAM store offset in cachelines
    // reg[4] = instruction[4]                                                  // DRAM store length in cachelines
    // reg[5] = instruction[5]                                                  // memory load offset in cachelines
    // reg[6] = instruction[6]                                                  // [0] (0 memory1) (1 memory2)
                                                                                // [1] DRAM buffer (0 out) (1 in)

    // if opcode == 5 ---- prefetch
    // reg[3] = instruction[3]+reg[1]*instruction[11]+reg[0]*instruction[10]    // DRAM read offset in cachelines
                                                                                // instruction[10]: read offset change per update
                                                                                // instruction[11]: read offset change per partiton (partition size)
    // reg[4] = instruction[4]                                                  // DRAM read length in cachelines

    // if opcode == 10 ---- jump0
    //  if reg[0] == instruction[12]:
    //      programCounter = instruction[13]
    //  else:
    //      programCounter = instruction[14]

    // if opcode == 11 ---- jump1
    //  if reg[1] == instruction[12]:
    //      programCounter = instruction[13]
    //  else:
    //      programCounter = instruction[14]

    // if opcode == 12 ---- jump3
    //  if reg[2] == instruction[12]:
    //      programCounter = instruction[13]
    //  else:
    //      programCounter = instruction[14]


    logic [15:0] program_counter;

    logic [31:0] instruction [16];
    logic [31:0] regs [NUM_REGS];
    logic [7:0] opcode;
    logic nonblocking;
    
    logic [31:0] dot_reg;

    logic [5:0] op_start;
    logic [5:0] op_done;

    always_ff @(posedge clk)
    begin
        if (reset)
        begin
            program_counter <= 32'h0;
            machine_state <= MACHINE_STATE_IDLE;
            op_start <= 6'b0;
            opcode <= 8'b0;
            nonblocking <= 1'b0;
            program_access.re <= 1'b0;
        end
        else
        begin
            op_start <= 6'b0;
            program_access.re <= 1'b0;

            case(machine_state)
                MACHINE_STATE_IDLE:
                begin
                    {<<{regs}} <= REGS_WIDTH'(0);
                    if (receive_state == RXTX_STATE_PROGRAM_EXECUTE)
                    begin
                        machine_state <= MACHINE_STATE_INSTRUCTION_FETCH;
                    end
                end

                MACHINE_STATE_INSTRUCTION_FETCH:
                begin
                    program_access.re <= 1'b1;
                    program_access.raddr <= program_counter;
                    machine_state <= MACHINE_STATE_INSTRUCTION_RECEIVE;
                end

                MACHINE_STATE_INSTRUCTION_RECEIVE:
                begin
                    if (program_access.rvalid)
                    begin
                        for (int i=0; i < 16; i=i+1)
                        begin
                            instruction[i] <= program_access.rdata[ (i*32)+31 -: 32 ];
                        end
                        machine_state <= MACHINE_STATE_INSTRUCTION_DECODE;
                    end
                end

                MACHINE_STATE_INSTRUCTION_DECODE:
                begin
                    machine_state <= MACHINE_STATE_EXECUTE;

                    opcode <= instruction[15][7:0];
                    nonblocking <= instruction[15][8];
                    case(instruction[15][7:0])
                        8'h0: // load
                        begin
                            op_start[0] <= 1'b1;
                            regs[3] <= instruction[3] + regs[1]*instruction[11] + regs[0]*instruction[10];
                            regs[4] <= instruction[4];
                            regs[5] <= instruction[5];
                            regs[6] <= instruction[6];
                            regs[7] <= instruction[7];
                            program_counter <= program_counter + 1;
                        end

                        8'h1: // dot
                        begin
                            op_start[1] <= 1'b1;
                            regs[3] <= instruction[3];
                            regs[4] <= instruction[4];
                            program_counter <= program_counter + 1;
                        end

                        8'h2: // modify
                        begin
                            op_start[2] <= 1'b1;
                            regs[3] <= instruction[3];
                            regs[4] <= instruction[4];
                            regs[5] <= instruction[5];
                            regs[6] <= instruction[6];
                            program_counter <= program_counter + 1;
                        end

                        8'h3: // update
                        begin
                            op_start[3] <= 1'b1;
                            regs[3] <= instruction[3];
                            regs[4] <= instruction[4];
                            program_counter <= program_counter + 1;
                        end

                        8'h4: // writeback
                        begin
                            op_start[4] <= 1'b1;
                            regs[3] <= instruction[3] + regs[1]*instruction[11] + regs[2]*instruction[10];
                            regs[4] <= instruction[4];
                            regs[5] <= instruction[5];
                            regs[6] <= instruction[6];
                            program_counter <= program_counter + 1;
                        end

                        8'h5: // prefetch
                        begin
                            op_start[5] <= 1'b1;
                            regs[3] <= instruction[3] + regs[1]*instruction[11] + regs[0]*instruction[10];
                            regs[4] <= instruction[4];
                            program_counter <= program_counter + 1;
                        end

                        8'hA:
                        begin
                            program_counter <= (regs[0] == instruction[12]) ? instruction[13] : instruction[14];
                        end

                        8'hB:
                        begin
                            program_counter <= (regs[1] == instruction[12]) ? instruction[13] : instruction[14];
                        end

                        8'hC:
                        begin
                            program_counter <= (regs[2] == instruction[12]) ? instruction[13] : instruction[14];
                        end

                    endcase
                end

                MACHINE_STATE_EXECUTE:
                begin
                    if (program_counter == 16'hFFFF)
                    begin
                        machine_state <= MACHINE_STATE_DONE;
                    end
                    else if (nonblocking == 1'b1 || op_done[opcode] || opcode > 8'h9)
                    begin
                        machine_state <= MACHINE_STATE_INSTRUCTION_FETCH;
                        regs[0] <= updateIndex(instruction[0], regs[0]);
                        regs[1] <= updateIndex(instruction[1], regs[1]);
                        regs[2] <= updateIndex(instruction[2], regs[2]);
                    end
                end

                MACHINE_STATE_DONE:
                begin
                    machine_state <= MACHINE_STATE_IDLE;
                end
            endcase

            // memory1 request arbitration
            if (execute_update_memory1_request.re)
            begin
                memory1_access.request <= execute_update_memory1_request;
            end
            else if (execute_dot_memory1_request.re)
            begin
                memory1_access.request <= execute_dot_memory1_request;
            end
            else
            begin
                memory1_access.request <= execute_writeback_memory1_request;
            end

            // memory1 write arbitration
            if (execute_update_memory1_write.we)
            begin
                memory1_access.write <= execute_update_memory1_write;
            end
            else
            begin
                memory1_access.write <= execute_load_memory1_write;
            end

            // memory2 request arbitration
            if (execute_modify_memory2_request.re)
            begin
                memory2_access.request <= execute_modify_memory2_request;
            end
            else if (execute_dot_memory2_request.re)
            begin
                memory2_access.request <= execute_dot_memory2_request;
            end
            else
            begin
                memory2_access.request <= execute_writeback_memory2_request;
            end

            // memory2 write arbitration
            if (execute_modify_memory2_write.we)
            begin
                memory2_access.write <= execute_modify_memory2_write;
            end
            else
            begin
                memory2_access.write <= execute_load_memory2_write;
            end

        end
    end

    logic execute_afterprefetch_c0TxAlmFull;
    t_if_ccip_c0_Rx execute_afterprefetch_cp2af_sRx_c0;
    t_if_ccip_c0_Tx execute_afterprefetch_af2cp_sTx_c0;
    logic op_request_done;

    execute_prefetch
    app_execute_prefetch
    (
        .clk,
        .reset,
        .op_start(op_start[5]),
        .op_done(op_done[5]),
        .regs0(regs[3]),
        .regs1(regs[4]),
        .in_addr,
        .c0TxAlmFull(execute_load_c0TxAlmFull),
        .cp2af_sRx_c0(execute_load_cp2af_sRx_c0),
        .af2cp_sTx_c0(execute_load_af2cp_sTx_c0),
        .get_c0TxAlmFull(execute_afterprefetch_c0TxAlmFull),
        .get_cp2af_sRx_c0(execute_afterprefetch_cp2af_sRx_c0),
        .get_af2cp_sTx_c0(execute_afterprefetch_af2cp_sTx_c0)
    );

    execute_load
    app_execute_load
    (
        .clk,
        .reset,
        .op_start(op_start[0]),
        .op_done(op_done[0]),
        .op_request_done(op_request_done),
        .regs,
        .in_addr,
        .samples_fifo_status(samples_fifo_access.status),
        .samples_fifo_write(samples_fifo_access.write),
        .prefetch_fifo_status(prefetch_fifo_access.status),
        .prefetch_fifo_write(prefetch_fifo_access.write),
        .memory1_write(execute_load_memory1_write),
        .memory2_write(execute_load_memory2_write),
        .c0TxAlmFull(execute_afterprefetch_c0TxAlmFull),
        .cp2af_sRx_c0(execute_afterprefetch_cp2af_sRx_c0),
        .af2cp_sTx_c0(execute_afterprefetch_af2cp_sTx_c0)
    );

    execute_dot
    app_execute_dot
    (
        .clk,
        .reset,
        .op_start(op_start[1]),
        .op_done(op_done[1]),
        .regs0(regs[3]),
        .regs1(regs[4]),
        .memory1_request(execute_dot_memory1_request),
        .memory1_read(memory1_access.read),
        .memory2_request(execute_dot_memory2_request),
        .memory2_read(memory2_access.read),
        .prefetch_fifo_tready(prefetch_fifo_access.read.re_tready),
        .prefetch_fifo_tvalid(prefetch_fifo_access.read.re_tvalid),
        .prefetch_fifo_tdata(prefetch_fifo_access.read.re_tdata),
        .model_forward_fifo_tready(model_forward_fifo_access.read.re_tready),
        .model_forward_fifo_tvalid(model_forward_fifo_access.read.re_tvalid),
        .model_forward_fifo_tdata(model_forward_fifo_access.read.re_tdata),
        .dot_reg
    );

    execute_modify
    app_execute_modify
    (
        .clk,
        .reset,
        .op_start(op_start[2]),
        .op_done(op_done[2]),
        .regs,
        .memory2_request(execute_modify_memory2_request),
        .memory2_read(memory2_access.read),
        .memory2_write(execute_modify_memory2_write),
        .dot_reg,
        .dot_fifo_write(dot_fifo_access.write)
    );

    execute_update
    app_execute_update
    (
        .clk,
        .reset,
        .op_start(op_start[3]),
        .op_done(op_done[3]),
        .regs0(regs[3]),
        .regs1(regs[4]),
        .memory1_request(execute_update_memory1_request),
        .memory1_read(memory1_access.read),
        .memory1_write(execute_update_memory1_write),
        .dot_fifo_tready(dot_fifo_access.read.re_tready),
        .dot_fifo_tvalid(dot_fifo_access.read.re_tvalid),
        .dot_fifo_tdata(dot_fifo_access.read.re_tdata),
        .samples_fifo_tready(samples_fifo_access.read.re_tready),
        .samples_fifo_tvalid(samples_fifo_access.read.re_tvalid),
        .samples_fifo_tdata(samples_fifo_access.read.re_tdata),
        .model_forward_fifo_write(model_forward_fifo_access.write)
    );

    execute_writeback
    app_execute_writeback
    (
        .clk,
        .reset,
        .op_start(op_start[4]),
        .op_done(op_done[4]),
        .regs,
        .in_addr,
        .out_addr,
        .memory1_request(execute_writeback_memory1_request),
        .memory2_request(execute_writeback_memory2_request),
        .memory1_read(memory1_access.read),
        .memory2_read(memory2_access.read),
        .c1TxAlmFull(execute_writeback_c1TxAlmFull),
        .cp2af_sRx_c1(execute_writeback_cp2af_sRx_c1),
        .af2cp_sTx_c1(execute_writeback_af2cp_sTx_c1)
    );

endmodule