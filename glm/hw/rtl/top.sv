//
// Copyright (c) 2017, Intel Corporation
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimer.
//
// Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// Neither the name of the Intel Corporation nor the names of its contributors
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

`include "cci_mpf_if.vh"
`include "csr_mgr.vh"
`include "afu_json_info.vh"

//
// AFU wrapper -- convert MPF interface to CCI-P structures and pass them
//                to the AFU implementation.
//
module app_afu
   (
    input  logic clk,

    // Connection toward the host.  Reset comes in here.
    cci_mpf_if.to_fiu fiu,

    // CSR connections
    app_csrs.app csrs,

    // MPF tracks outstanding requests.  These will be true as long as
    // reads or unacknowledged writes are still in flight.
    input  logic c0NotEmpty,
    input  logic c1NotEmpty
    );

    // Local reset to reduce fan-out
    logic reset = 1'b1;
    always @(posedge clk)
    begin
        reset <= fiu.reset;
    end

    //
    // Convert MPF interfaces back to the standard CCI structures.
    //
    t_if_ccip_Rx mpf2af_sRx;
    t_if_ccip_Tx af2mpf_sTx;

    //
    // The base module has already registered the Rx wires heading
    // toward the AFU, so wires are acceptable.
    //
    always_comb
    begin
        //
        // Response wires
        //

        mpf2af_sRx.c0 = fiu.c0Rx;
        mpf2af_sRx.c1 = fiu.c1Rx;

        mpf2af_sRx.c0TxAlmFull = fiu.c0TxAlmFull;
        mpf2af_sRx.c1TxAlmFull = fiu.c1TxAlmFull;


        //
        // Request wires
        //

        fiu.c0Tx = cci_mpf_cvtC0TxFromBase(af2mpf_sTx.c0);
        if (cci_mpf_c0TxIsReadReq(fiu.c0Tx))
        begin
            // Treat all addresses as virtual.  If MPF's VTP isn't
            // enabled this field is ignored and addresses will remain
            // physical.
            fiu.c0Tx.hdr.ext.addrIsVirtual = 1'b1;

            // Enable eVC_VA to physical channel mapping.  This will only
            // be triggered when MPF's ENABLE_VC_MAP is set.
            fiu.c0Tx.hdr.ext.mapVAtoPhysChannel = 1'b1;

            // Enforce load/store and store/store ordering within lines.
            // This will only be triggered when ENFORCE_WR_ORDER is set.
            fiu.c0Tx.hdr.ext.checkLoadStoreOrder = 1'b1;
        end

        fiu.c1Tx = cci_mpf_cvtC1TxFromBase(af2mpf_sTx.c1);
        if (cci_mpf_c1TxIsWriteReq(fiu.c1Tx))
        begin
            // See comments on the c0Tx fields above
            fiu.c1Tx.hdr.ext.addrIsVirtual = 1'b1;
            fiu.c1Tx.hdr.ext.mapVAtoPhysChannel = 1'b1;
            fiu.c1Tx.hdr.ext.checkLoadStoreOrder = 1'b1;

            // Don't ever request an MPF partial write
            fiu.c1Tx.hdr.pwrite = t_cci_mpf_c1_PartialWriteHdr'(0);
        end

        fiu.c2Tx = af2mpf_sTx.c2;
    end

    // Connect to the AFU
    glm
    app_glm
    (
    .clk,
    .reset,
    .cp2af_sRx(mpf2af_sRx),
    .af2cp_sTx(af2mpf_sTx),
    .csrs,
    .c0NotEmpty,
    .c1NotEmpty
    );

endmodule // app_afu