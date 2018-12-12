bram_access prefetch_memory_access;
    dual_port_ram
    #(
        .DATA_WIDTH(512),
        .ADDR_WIDTH(LOG2_PREFETCH_SIZE)
    )
    memory1
    (
        .clk,
        .raddr(prefetch_memory_access.request.raddr[LOG2_PREFETCH_SIZE-1:0]),
        .waddr(prefetch_memory_access.write.waddr[LOG2_PREFETCH_SIZE-1:0]),
        .data(prefetch_memory_access.write.wdata),
        .we(prefetch_memory_access.write.we),
        .re(prefetch_memory_access.request.re),
        .qvalid(prefetch_memory_access.read.valid),
        .q(prefetch_memory_access.read.rdata)
    );

    wordbram_access requested_addresses_access;
    dual_port_ram
    #(
        .DATA_WIDTH(32),
        .ADDR_WIDTH(LOG2_PREFETCH_SIZE)
    )
    requested_addresses
    (
        .clk,
        .raddr(requested_addresses_access.request.raddr),
        .waddr(requested_addresses_access.write.waddr),
        .data(requested_addresses_access.write.wdata),
        .we(requested_addresses_access.write.we),
        .re(requested_addresses_access.request.re),
        .qvalid(requested_addresses_access.read.valid),
        .q(requested_addresses_access.read.rdata)
    );
    assign requested_addresses_access.request.re = get_af2cp_sTx_c0.valid;
    assign requested_addresses_access.request.raddr = get_af2cp_sTx_c0.hdr.address[LOG2_PREFETCH_SIZE-1:0];

    logic [PREFETCH_SIZE-1:0] received_addresses_bitmap;

    wordfifo_access wait_fifo_access;
    normal2axis_fifo
    #(
        .FIFO_WIDTH(1+LOG2_PREFETCH_SIZE),
        .LOG2_FIFO_DEPTH(LOG2_PREFETCH_SIZE)
    )
    wait_fifo
    (
        .clk,
        .resetn(!reset),
        .write_enable(wait_fifo_access.write.we),
        .write_data(wait_fifo_access.write.wdata[LOG2_PREFETCH_SIZE:0]),
        .m_axis_tvalid(wait_fifo_access.read.re_tvalid),
        .m_axis_tready(wait_fifo_access.read.re_tready),
        .m_axis_tdata(wait_fifo_access.read.re_tdata[LOG2_PREFETCH_SIZE:0]),
        .almostfull(wait_fifo_access.status.almostfull),
        .count(wait_fifo_access.status.count[LOG2_PREFETCH_SIZE-1:0])
    );

    assign prefetch_memory_access.request.re = wait_fifo_access.read.re_tvalid;
    assign prefetch_memory_access.request.raddr = wait_fifo_access.read.re_tdata;

    clfifo_access overflow_fifo_access;
    normal2axis_fifo
    #(
        .FIFO_WIDTH(512),
        .LOG2_FIFO_DEPTH(5)
    )
    overflow_fifo
    (
        .clk,
        .resetn(!reset),
        .write_enable(overflow_fifo_access.write.we),
        .write_data(overflow_fifo_access.write.wdata),
        .m_axis_tvalid(overflow_fifo_access.read.re_tvalid),
        .m_axis_tready(overflow_fifo_access.read.re_tready),
        .m_axis_tdata(overflow_fifo_access.read.re_tdata),
        .almostfull(overflow_fifo_access.status.almostfull),
        .count(overflow_fifo_access.status.count[4:0])
    );

    t_cci_c0_RspMemHdr response_hdr;
    always_comb
    begin
        response_hdr = t_cci_c0_RspMemHdr'(0);
        response_hdr.vc_used = eVC_VA;
        response_hdr.hit_miss = 1'b0;
        response_hdr.cl_num = 2'b0;
        response_hdr.resp_type = eRSP_RDLINE;
        response_hdr.mdata = 0;
    end

    t_cci_c0_ReqMemHdr rd_hdr;
    always_comb
    begin
        rd_hdr = t_cci_c0_ReqMemHdr'(0);
        rd_hdr.req_type = eREQ_RDLINE_I;
        rd_hdr.vc_sel = eVC_VA;
        rd_hdr.cl_len = eCL_LEN_1;
    end

    // Local variables
    t_ccip_clAddr DRAM_load_offset;
    logic [31:0] DRAM_load_length;
    t_if_ccip_c0_Tx internal_af2cp_sTx_c0;
    t_ccip_clAddr temp_address;

    // Counters
    logic [31:0] num_requested_lines;
    logic [31:0] num_wait_fifo_lines;
    logic [31:0] num_received_lines;
    logic [31:0] num_lines_in_flight;
    logic signed [31:0] num_allowed_lines_to_request;

    always_ff @(posedge clk)
    begin
        num_lines_in_flight <= num_requested_lines - num_received_lines;
        num_allowed_lines_to_request <= PREFETCH_SIZE - $signed(num_lines_in_flight);

        internal_af2cp_sTx_c0 <= get_af2cp_sTx_c0;
        get_cp2af_sRx_c0.mmioRdValid <= 0;
        get_cp2af_sRx_c0.mmioWrValid <= 0;

        if (reset)
        begin
            request_state <= STATE_IDLE;
            receive_state <= STATE_IDLE;

            prefetch_memory_access.write.we <= 1'b0;
            requested_addresses_access.write.we <= 1'b0;
            wait_fifo_access.write.we <= 1'b0;
            wait_fifo_access.read.re_tready <= 1'b0;
            overflow_fifo_access.write.we <= 1'b0;
            overflow_fifo_access.read.re_tready <= 1'b0;

            num_requested_lines <= 32'b0;
            num_wait_fifo_lines <= 32'b0;
            num_received_lines <= 32'b0;

            DRAM_load_offset <= t_ccip_clAddr'(0);
            DRAM_load_length <= 32'b0;

            get_c0TxAlmFull <= 1'b0;
            get_cp2af_sRx_c0.rspValid <= 1'b0;
            op_done <= 1'b0;
        end
        else
        begin

            // =================================
            //
            //   Request State Machine
            //
            // =================================
            af2cp_sTx_c0.valid <= 1'b0;
            requested_addresses_access.write.we <= 1'b0;
            wait_fifo_access.write.we <= 1'b0;
            case(request_state)
                STATE_IDLE:
                begin
                    get_c0TxAlmFull <= c0TxAlmFull;
                    af2cp_sTx_c0 <= get_af2cp_sTx_c0;

                    if (op_start)
                    begin
                        num_requested_lines <= 32'b0;
                        num_wait_fifo_lines <= 32'b0;
                        DRAM_load_offset <= in_addr + regs0;
                        DRAM_load_length <= (regs1 > PREFETCH_SIZE) ? PREFETCH_SIZE : regs1;
                        request_state <= STATE_READ;
                    end
                end

                STATE_READ:
                begin
                    get_c0TxAlmFull <= c0TxAlmFull || wait_fifo_access.status.almostfull;

                    if (internal_af2cp_sTx_c0.valid) // Put to wait FIFO
                    begin
                        wait_fifo_access.write.we <= 1'b1;
                        wait_fifo_access.write.wdata <= num_wait_fifo_lines[LOG2_PREFETCH_SIZE-1:0];
                        // 0: read from prefetch_memory, 1: read from overflow_fifo
                        wait_fifo_access.write.wdata[LOG2_PREFETCH_SIZE] <= (requested_addresses_access.read.rdata != internal_af2cp_sTx_c0.hdr.address[31:0]);
                        num_wait_fifo_lines <= num_wait_fifo_lines + 1;
                    end

                    if (internal_af2cp_sTx_c0.valid && (requested_addresses_access.read.rdata != internal_af2cp_sTx_c0.hdr.address[31:0]) ) // Create new memory request
                    begin
                        af2cp_sTx_c0 <= internal_af2cp_sTx_c0;
                        af2cp_sTx_c0.hdr.mdata <= 16'hFFFF;
                    end
                    else
                    begin
                        if (num_requested_lines < DRAM_load_length && !c0TxAlmFull && (num_allowed_lines_to_request > 0) )
                        begin
                            temp_address = t_ccip_clAddr'(DRAM_load_offset + num_requested_lines);
                            af2cp_sTx_c0.valid <= 1'b1;
                            af2cp_sTx_c0.hdr <= rd_hdr;
                            af2cp_sTx_c0.hdr.address <= temp_address
                            af2cp_sTx_c0.hdr.mdata <= 16'h0001;

                            requested_addresses_access.write.we <= 1'b1;
                            requested_addresses_access.write.waddr <= temp_address[LOG2_PREFETCH_SIZE-1:0];
                            requested_addresses_access.write.wdata <= temp_address[31:0];

                            num_requested_lines <= num_requested_lines + 1;
                            if (num_requested_lines == DRAM_load_length-1)
                            begin
                                request_state <= STATE_DONE;
                            end
                        end
                    end
                    else if (DRAM_load_length == 32'b0)
                    begin
                        request_state <= STATE_DONE;
                    end
                end

                STATE_DONE:
                begin
                    request_state <= STATE_IDLE;
                end
            endcase

            // =================================
            //
            //   Receive State Machine
            //
            // =================================
            prefetch_memory_access.write.we <= 1'b0;
            wait_fifo_access.read.re_tready <= 1'b0;
            overflow_fifo_access.write.we <= 1'b0;
            overflow_fifo_access.read.re_tready <= 1'b0;
            get_cp2af_sRx_c0.rspValid <= 1'b0;
            op_done <= 1'b0;
            case(receive_state)
                STATE_IDLE:
                begin
                    get_cp2af_sRx_c0 <= cp2af_sRx_c0;
                    if (op_start)
                    begin
                        num_received_lines <= 32'b0;
                        receive_state <= STATE_WAIT;
                    end
                end

                STATE_READ:
                begin

                    if (cp2af_sRx_c0.rspValid)
                    begin
                        if (cp2af_sRx_c0.hdr.mdata == 16'hFFFF)
                        begin
                            overflow_fifo_access.write.we <= 1'b1;
                            overflow_fifo_access.write.wdata <= cp2af_sRx_c0.data;
                        end
                        else
                        begin
                            prefetch_memory_access.write.we <= 1'b1;
                            prefetch_memory_access.write.wdata <= cp2af_sRx_c0.data;
                            prefetch_memory_access.write.waddr <= num_received_lines[LOG2_PREFETCH_SIZE-1:0];
                            received_addresses_bitmap[num_received_lines[LOG2_PREFETCH_SIZE-1:0]] <= 1'b1;
                            num_received_lines <= num_received_lines + 1;
                        end
                        wait_fifo_access.read.re_tready <= 1'b1;
                    end


                    if (wait_fifo_access.read.re_tvalid && wait_fifo_access.read.re_tready)
                    begin
                        if (wait_fifo_access.read.re_tdata[LOG2_PREFETCH_SIZE] == 1'b1) // read from overflow fifo
                        begin

                        end
                        else
                        begin

                        end
                    end








                    if (wait_fifo_access.read.re_tvalid && wait_fifo_access.read.re_tdata[LOG2_PREFETCH_SIZE] == 1'b1) // read from overflow fifo
                    begin
                        if (overflow_fifo_access.read.re_tvalid)
                        begin
                            wait_fifo_access.read.re_tready <= 1'b1;
                            overflow_fifo_access.read.re_tready <= 1'b1;
                        end
                    end
                    else if (wait_fifo_access.read.re_tvalid && wait_fifo_access.read.re_tdata[LOG2_PREFETCH_SIZE] == 1'b0) // read from prefetch_memory
                    begin
                        if (received_addresses_bitmap[wait_fifo_access.read.re_tdata] == 1'b1)
                        begin
                            wait_fifo_access.read.re_tready <= 1'b1;
                        end
                    end

                    if (overflow_fifo_access.read.re_tvalid && overflow_fifo_access.read.re_tready)
                    begin
                        get_cp2af_sRx_c0.rspValid <= 1'b1;
                        get_cp2af_sRx_c0.hdr <= response_hdr;
                        get_cp2af_sRx_c0.data <= overflow_fifo_access.read.re_tdata;
                    end
                    else if (prefetch_memory_access.read.valid)
                    begin
                        get_cp2af_sRx_c0.rspValid <= 1'b1;
                        get_cp2af_sRx_c0.hdr <= response_hdr;
                        get_cp2af_sRx_c0.data <= prefetch_memory_access.read.rdata;
                    end

                    if (num_received_lines == DRAM_load_length-1 && wait_fifo_access.read.count[LOG2_PREFETCH_SIZE-1:0] == 0)
                    begin
                        receive_state <= STATE_DONE;
                    end

                STATE_DONE:
                begin
                    op_done <= 1'b1;
                    receive_state <= STATE_IDLE;
                end
            endcase

        end
    end