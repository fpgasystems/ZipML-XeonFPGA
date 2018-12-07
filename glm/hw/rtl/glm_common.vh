`ifndef GLM_COMMON
`define GLM_COMMON

parameter LOG2_MEMORY_SIZE = 10;
parameter PROGRAM_SIZE = 32;
parameter NUM_REGS = 12;
parameter REGS_WIDTH = 32*NUM_REGS;
parameter LOG2_PREFETCH_SIZE = 8;
parameter PREFETCH_SIZE = 2**LOG2_PREFETCH_SIZE - 16;

// =================================
//
//   BRAM
//
// =================================

typedef struct packed {
    logic[LOG2_MEMORY_SIZE-1:0] waddr;
    logic[511:0] wdata;
    logic we;
} bram_write;

typedef struct packed {
    logic[LOG2_MEMORY_SIZE-1:0] raddr;
    logic re;
} bram_request;

typedef struct packed {
    logic[511:0] rdata;
    logic valid;
} bram_read;

typedef struct packed {
	bram_write write;
	bram_request request;
	bram_read read;
} bram_access;

// =================================
//
//   CLFIFO
//
// =================================

typedef struct packed {
    logic almostfull;
    logic[15:0] count;
} clfifo_status;

typedef struct packed {
    logic we;
    logic[511:0] wdata;
} clfifo_write;

typedef struct packed {
    logic re_tvalid;
    logic re_tready;
    logic[511:0] re_tdata;
} clfifo_read;

typedef struct packed {
    clfifo_write write;
    clfifo_read read;
    clfifo_status status;
} clfifo_access;

// =================================
//
//   WORDFIFO
//
// =================================

typedef struct packed {
    logic almostfull;
} wordfifo_status;

typedef struct packed {
    logic we;
    logic[31:0] wdata;
} wordfifo_write;

typedef struct packed {
    logic re_tvalid;
    logic re_tready;
    logic[31:0] re_tdata;
} wordfifo_read;

typedef struct packed {
    wordfifo_write write;
    wordfifo_read read;
    wordfifo_status status;
} wordfifo_access;

// =================================
//
//   COMMON STATES
//
// =================================

typedef enum logic [1:0]
{
    RXTX_STATE_IDLE,
    RXTX_STATE_PROGRAM_READ,
    RXTX_STATE_PROGRAM_EXECUTE,
    RXTX_STATE_DONE
} t_rxtxstate;

typedef enum logic [2:0]
{
    MACHINE_STATE_IDLE,
    MACHINE_STATE_INSTRUCTION_FETCH,
    MACHINE_STATE_INSTRUCTION_DECODE,
    MACHINE_STATE_EXECUTE,
    MACHINE_STATE_DONE
} t_machinestate;


// =================================
//
//   COMMON FUNCTIONS
//
// =================================

//
// Convert between byte addresses and line addresses.  The conversion
// is simple: adding or removing low zero bits.
//
localparam CL_BYTE_IDX_BITS = 6;
typedef logic [$bits(t_cci_clAddr) + CL_BYTE_IDX_BITS - 1 : 0] t_byteAddr;

function automatic t_cci_clAddr byteAddrToClAddr(t_byteAddr addr);
    return addr[CL_BYTE_IDX_BITS +: $bits(t_cci_clAddr)];
endfunction

function automatic t_byteAddr clAddrToByteAddr(t_cci_clAddr addr);
    return {addr, CL_BYTE_IDX_BITS'(0)};
endfunction


`endif //  GLM_COMMON