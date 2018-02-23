library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity selector_cross is
generic(
	ADDRESS_WIDTH : integer := 32);
port(
	clk_400 : in std_logic;
	clk_200 : in std_logic;
	resetn : in std_logic;

	-- Read request signals
	read_request : out std_logic;
	read_request_accept : in std_logic;
	read_request_address : out std_logic_vector(ADDRESS_WIDTH-1 downto 0);
	read_request_transactionID : out std_logic_vector(15 downto 0);

	-- Read response signals
	read_response : in std_logic;
	read_response_data : in std_logic_vector(511 downto 0);
	read_response_config : in std_logic;
	read_response_transactionID : in std_logic_vector(15 downto 0);

	-- Write request signals
	write_request : out std_logic;
	write_request_address : out std_logic_vector(ADDRESS_WIDTH-1 downto 0);
	write_request_data : out std_logic_vector(511 downto 0);
	write_request_transactionID : out std_logic_vector(15 downto 0);
	write_request_almostfull : in std_logic;
	
	-- Write response signals
	write_response: in std_logic;
	write_response_transactionID : in std_logic_vector(15 downto 0);

	read_length : out std_logic_vector(1 downto 0);
	read_sop : out std_logic;
	write_length : out std_logic_vector(1 downto 0);
	write_sop : out std_logic;
	  
	read_response_format : in std_logic;
	read_response_cacheline_number : in std_logic_vector(1 downto 0);
	write_response_format : in std_logic;
	write_response_cacheline_number : in std_logic_vector(1 downto 0);
	multi_cacheline_length : in std_logic_vector(1 downto 0);

	start : in std_logic;
	done : out std_logic;

	src_addr : in std_logic_vector(ADDRESS_WIDTH-1 downto 0);
	dst_addr : in std_logic_vector(ADDRESS_WIDTH-1 downto 0);
	number_of_CL_to_process : in std_logic_vector(31 downto 0);
	addr_reset : in std_logic_vector(31 downto 0);
	read_offset : in std_logic_vector(31 downto 0);
	write_offset : in std_logic_vector(31 downto 0);
	config1 : in std_logic_vector(63 downto 0);
	config2 : in std_logic_vector(63 downto 0);
	config3 : in std_logic_vector(63 downto 0);
	config4 : in std_logic_vector(63 downto 0);
	config5 : in std_logic_vector(63 downto 0));
end selector_cross;

architecture behavioral of selector_cross is

signal resetn_200 : std_logic;
signal start_200 : std_logic;
signal done_200 : std_logic;

----------------------------------------------------------- read_request Signals START
signal read_request_200 : std_logic;
signal read_request_accept_200 : std_logic;
signal read_request_address_200 : std_logic_vector(ADDRESS_WIDTH-1 downto 0);
signal read_request_transactionID_200 : std_logic_vector(15 downto 0);

signal read_request_fifo_data : std_logic_vector(57 downto 0);
signal read_request_fifo_wrreq : std_logic;
signal read_request_fifo_rdreq : std_logic;
signal read_request_fifo_q : std_logic_vector(57 downto 0);
signal read_request_fifo_rdempty : std_logic;
signal read_request_fifo_wrfull : std_logic;
----------------------------------------------------------- read_request Signals END

----------------------------------------------------------- read_response Signals START
signal read_response_200 : std_logic;
signal read_response_data_200 : std_logic_vector(511 downto 0);
signal read_response_config_200 : std_logic;
signal read_response_transactionID_200 : std_logic_vector(15 downto 0);

signal read_response_fifo_data : std_logic_vector(527 downto 0);
signal read_response_fifo_wrreq : std_logic;
signal read_response_fifo_rdreq : std_logic;
signal read_response_fifo_q : std_logic_vector(527 downto 0);
signal read_response_fifo_rdempty : std_logic;
signal read_response_fifo_wrfull : std_logic;
----------------------------------------------------------- read_response Signals END

----------------------------------------------------------- write_request Signals START
signal write_request_200 : std_logic;
signal write_request_address_200 : std_logic_vector(ADDRESS_WIDTH-1 downto 0);
signal write_request_data_200 : std_logic_vector(511 downto 0);
signal write_request_transactionID_200 : std_logic_vector(15 downto 0);
signal write_request_almostfull_200 : std_logic;

signal write_request_fifo_data : std_logic_vector(569 downto 0);
signal write_request_fifo_wrreq : std_logic;
signal write_request_fifo_rdreq : std_logic;
signal write_request_fifo_q : std_logic_vector(569 downto 0);
signal write_request_fifo_rdempty : std_logic;
signal write_request_fifo_wrfull : std_logic;
----------------------------------------------------------- write_request Signals END

----------------------------------------------------------- write_response Signals START
signal write_response_200 : std_logic;
signal write_response_transactionID_200 : std_logic_vector(15 downto 0);

signal write_response_fifo_data : std_logic_vector(15 downto 0);
signal write_response_fifo_wrreq : std_logic;
signal write_response_fifo_rdreq : std_logic;
signal write_response_fifo_q : std_logic_vector(15 downto 0);
signal write_response_fifo_rdempty : std_logic;
signal write_response_fifo_wrfull : std_logic;
----------------------------------------------------------- write_response Signals START

----------------------------------------------------------- Configuration Signals START
signal src_addr_1d : std_logic_vector(ADDRESS_WIDTH-1 downto 0);
signal dst_addr_1d : std_logic_vector(ADDRESS_WIDTH-1 downto 0);
signal number_of_CL_to_process_1d : std_logic_vector(31 downto 0);
signal addr_reset_1d : std_logic_vector(31 downto 0);
signal read_offset_1d : std_logic_vector(31 downto 0);
signal write_offset_1d : std_logic_vector(31 downto 0);
signal config1_1d : std_logic_vector(63 downto 0);
signal config2_1d : std_logic_vector(63 downto 0);
signal config3_1d : std_logic_vector(63 downto 0);
signal config4_1d : std_logic_vector(63 downto 0);
signal config5_1d : std_logic_vector(63 downto 0);

signal src_addr_200 : std_logic_vector(ADDRESS_WIDTH-1 downto 0);
signal dst_addr_200 : std_logic_vector(ADDRESS_WIDTH-1 downto 0);
signal number_of_CL_to_process_200 : std_logic_vector(31 downto 0);
signal addr_reset_200 : std_logic_vector(31 downto 0);
signal read_offset_200 : std_logic_vector(31 downto 0);
signal write_offset_200 : std_logic_vector(31 downto 0);
signal config1_200 : std_logic_vector(63 downto 0);
signal config2_200 : std_logic_vector(63 downto 0);
signal config3_200 : std_logic_vector(63 downto 0);
signal config4_200 : std_logic_vector(63 downto 0);
signal config5_200 : std_logic_vector(63 downto 0);

signal configfifo_data    : std_logic_vector(448+2*ADDRESS_WIDTH-1 downto 0);
signal configfifo_wrreq   : std_logic;
signal configfifo_rdreq   : std_logic;
signal configfifo_q       : std_logic_vector(448+2*ADDRESS_WIDTH-1 downto 0);
signal configfifo_rdempty : std_logic;
signal configfifo_wrfull  : std_logic;
----------------------------------------------------------- Configuration Signals END

component async_fifo
    generic (
        FIFO_WIDTH : integer;
        FIFO_DEPTH_BITS : integer;
        ACK : integer
    );
    port (
        data    : in  std_logic_vector(FIFO_WIDTH-1 downto 0);
        wrreq   : in  std_logic;
        rdreq   : in  std_logic;
        wrclk   : in  std_logic;
        rdclk   : in  std_logic;
        q       : out std_logic_vector(FIFO_WIDTH-1 downto 0);
        rdempty : out std_logic;
        wrfull  : out std_logic
    );
end component;

component selector
generic(
    ADDRESS_WIDTH : integer := 32);
port(
    clk_200 : in std_logic;
    resetn : in std_logic;

    -- Read request signals
    read_request : out std_logic;
    read_request_accept : in std_logic;
    read_request_address : out std_logic_vector(ADDRESS_WIDTH-1 downto 0);
    read_request_transactionID : out std_logic_vector(15 downto 0);

    -- Read response signals
    read_response : in std_logic;
    read_response_data : in std_logic_vector(511 downto 0);
    read_response_config : in std_logic;
    read_response_transactionID : in std_logic_vector(15 downto 0);

    -- Write request signals
    write_request : out std_logic;
    write_request_address : out std_logic_vector(ADDRESS_WIDTH-1 downto 0);
    write_request_data : out std_logic_vector(511 downto 0);
    write_request_transactionID : out std_logic_vector(15 downto 0);
    write_request_almostfull : in std_logic;
    
    -- Write response signals
    write_response: in std_logic;
    write_response_transactionID : in std_logic_vector(15 downto 0);

    read_length : out std_logic_vector(1 downto 0);
    read_sop : out std_logic;
    write_length : out std_logic_vector(1 downto 0);
    write_sop : out std_logic;
      
    read_response_format : in std_logic;
    read_response_cacheline_number : in std_logic_vector(1 downto 0);
    write_response_format : in std_logic;
    write_response_cacheline_number : in std_logic_vector(1 downto 0);
    multi_cacheline_length : in std_logic_vector(1 downto 0);

    start : in std_logic;
    done : out std_logic;

    src_addr : in std_logic_vector(ADDRESS_WIDTH-1 downto 0);
    dst_addr : in std_logic_vector(ADDRESS_WIDTH-1 downto 0);
    number_of_CL_to_process : in std_logic_vector(31 downto 0);
    addr_reset : in std_logic_vector(31 downto 0);
    read_offset : in std_logic_vector(31 downto 0);
    write_offset : in std_logic_vector(31 downto 0);
    config1 : in std_logic_vector(63 downto 0);
    config2 : in std_logic_vector(63 downto 0);
    config3 : in std_logic_vector(63 downto 0);
    config4 : in std_logic_vector(63 downto 0);
    config5 : in std_logic_vector(63 downto 0));
end component;

begin

process(clk_200)
begin
if clk_200'event and clk_200 = '1' then
    resetn_200 <= resetn;
    start_200 <= start;
end if;
end process;
process(clk_400)
begin
if clk_400'event and clk_400 = '1' then
    done <= done_200;
end if;
end process;

selector_inst : selector
generic map(
    ADDRESS_WIDTH => ADDRESS_WIDTH)
port map(
    clk_200 => clk_200,
    resetn => resetn_200,

    -- Read request signals
    read_request => read_request_200,
    read_request_accept => read_request_accept_200,
    read_request_address => read_request_address_200,
    read_request_transactionID => read_request_transactionID_200,

    read_response => read_response_200,
    read_response_data => read_response_data_200,
    read_response_config => '0',
    read_response_transactionID => read_response_transactionID_200,

    write_request => write_request_200,
    write_request_address => write_request_address_200,
    write_request_data => write_request_data_200,
    write_request_transactionID => write_request_transactionID_200,
    write_request_almostfull => write_request_almostfull_200,

    write_response => write_response_200,
    write_response_transactionID => write_response_transactionID_200,

    read_length => read_length,
    read_sop => read_sop,
    write_length => write_length,
    write_sop => write_sop,

    read_response_format => read_response_format,
    read_response_cacheline_number => read_response_cacheline_number,
    write_response_format => write_response_format,
    write_response_cacheline_number => write_response_cacheline_number,
    multi_cacheline_length => multi_cacheline_length,

    start => start_200,
    done => done_200,

    src_addr => src_addr_200,
    dst_addr => dst_addr_200,
    number_of_CL_to_process => number_of_CL_to_process_200,
    addr_reset => addr_reset_200,
    read_offset => read_offset_200,
    write_offset => write_offset_200,
    config1 => config1_200,
    config2 => config2_200,
    config3 => config3_200,
    config4 => config4_200,
    config5 => config5_200);

----------------------------------------------------------- read_request START
read_request_fifo_data <= read_request_transactionID_200 & read_request_address_200;
read_request_fifo_wrreq <= read_request_200;
read_request_fifo_rdreq <= read_request_accept;
read_request_address <= read_request_fifo_q(ADDRESS_WIDTH-1 downto 0);
read_request_transactionID <= read_request_fifo_q(15+ADDRESS_WIDTH downto ADDRESS_WIDTH);
read_request <= not read_request_fifo_rdempty;
read_request_accept_200 <= not read_request_fifo_wrfull;

read_request_fifo: async_fifo
generic map (
    FIFO_WIDTH => 58,
    FIFO_DEPTH_BITS => 8,
    ACK => 1)
port map(
    data => read_request_fifo_data,
    wrreq => read_request_fifo_wrreq,
    rdreq => read_request_fifo_rdreq,
    wrclk => clk_200,
    rdclk => clk_400,
    q => read_request_fifo_q,
    rdempty => read_request_fifo_rdempty,
    wrfull => read_request_fifo_wrfull);
----------------------------------------------------------- read_request END

----------------------------------------------------------- read_response START
read_response_fifo_data <= read_response_transactionID & read_response_data;
read_response_fifo_wrreq <= read_response;
read_response_data_200 <= read_response_fifo_q(511 downto 0);
read_response_transactionID_200 <= read_response_fifo_q(527 downto 512);

process(clk_200)
begin
if clk_200'event and clk_200 = '1' then
    read_response_fifo_rdreq <= '0';
    if read_response_fifo_rdempty = '0' then
        read_response_fifo_rdreq <= '1';
    end if;
    read_response_200 <= read_response_fifo_rdreq and (not read_response_fifo_rdempty);
end if;
end process;

read_response_fifo: async_fifo
generic map (
    FIFO_WIDTH => 528,
    FIFO_DEPTH_BITS => 8,
    ACK => 0)
port map(
    data => read_response_fifo_data,
    wrreq => read_response_fifo_wrreq,
    rdreq => read_response_fifo_rdreq,
    wrclk => clk_400,
    rdclk => clk_200,
    q => read_response_fifo_q,
    rdempty => read_response_fifo_rdempty,
    wrfull => read_response_fifo_wrfull);
----------------------------------------------------------- read_response END

----------------------------------------------------------- write_request START
write_request_fifo_data <= write_request_transactionID_200 & write_request_address_200 & write_request_data_200;
write_request_fifo_wrreq <= write_request_200;
write_request_address <= write_request_fifo_q(512+ADDRESS_WIDTH-1 downto 512);
write_request_data <= write_request_fifo_q(511 downto 0);
write_request_transactionID <= write_request_fifo_q(15+512+ADDRESS_WIDTH downto 512+ADDRESS_WIDTH);

process(clk_400)
begin
if clk_400'event and clk_400 = '1' then
    write_request_fifo_rdreq <= '0';
    if write_request_fifo_rdempty = '0' and write_request_almostfull = '0' then
        write_request_fifo_rdreq <= '1';
    end if;
    write_request <= write_request_fifo_rdreq and (not write_request_fifo_rdempty);
end if;
end process;
process(clk_200)
begin
if clk_200'event and clk_200 = '1' then
    write_request_almostfull_200 <= write_request_almostfull;
end if;
end process;

write_request_fifo: async_fifo
generic map (
    FIFO_WIDTH => 570,
    FIFO_DEPTH_BITS => 8,
    ACK => 0)
port map (
    data => write_request_fifo_data,
    wrreq => write_request_fifo_wrreq,
    rdreq => write_request_fifo_rdreq,
    wrclk => clk_200,
    rdclk => clk_400,
    q => write_request_fifo_q,
    rdempty => write_request_fifo_rdempty,
    wrfull => write_request_fifo_wrfull);
----------------------------------------------------------- write_request END

----------------------------------------------------------- write_response START
write_response_fifo_data <= write_response_transactionID;
write_response_fifo_wrreq <= write_response;
write_response_transactionID_200 <= write_response_fifo_q;

process(clk_200)
begin
if clk_200'event and clk_200 = '1' then
    write_response_fifo_rdreq <= '0';
    if write_response_fifo_rdempty = '0' then
        write_response_fifo_rdreq <= '1';
    end if;
    write_response_200 <= write_response_fifo_rdreq and (not write_response_fifo_rdempty);
end if;
end process;

write_response_fifo: async_fifo
generic map (
    FIFO_WIDTH => 16,
    FIFO_DEPTH_BITS => 8,
    ACK => 0)
port map (
    data => write_response_fifo_data,
    wrreq => write_response_fifo_wrreq,
    rdreq => write_response_fifo_rdreq,
    wrclk => clk_400,
    rdclk => clk_200,
    q => write_response_fifo_q,
    rdempty => write_response_fifo_rdempty,
    wrfull => write_response_fifo_wrfull);
----------------------------------------------------------- write_response END

----------------------------------------------------------- Configuration START
process(clk_400)
begin
if clk_400'event and clk_400 = '1' then
    src_addr_1d <= src_addr;
    dst_addr_1d <= dst_addr;
    number_of_CL_to_process_1d <= number_of_CL_to_process;
    addr_reset_1d <= addr_reset;
    read_offset_1d <= read_offset;
    write_offset_1d <= write_offset;
    config1_1d <= config1;
    config2_1d <= config2;
    config3_1d <= config3;
    config4_1d <= config4;
    config5_1d <= config5;

    configfifo_data <= src_addr & dst_addr & config5 & config4 & config3 & config2 & config1 & number_of_CL_to_process & addr_reset & read_offset & write_offset;
    configfifo_wrreq <= '0';
    if  src_addr_1d /= src_addr or 
        dst_addr_1d /= dst_addr or
        number_of_CL_to_process_1d /= number_of_CL_to_process or
        addr_reset_1d /= addr_reset or
        read_offset_1d /= read_offset or
        write_offset_1d /= write_offset or
        config1_1d /= config1 or
        config2_1d /= config2 or
        config3_1d /= config3 or
        config4_1d /= config4 or
        config5_1d /= config5
    then
        configfifo_wrreq <= '1';
    end if;
    
end if;
end process;

process(clk_200)
begin
if clk_200'event and clk_200 = '1' then
    configfifo_rdreq <= '0';
    if configfifo_rdempty = '0' then
        configfifo_rdreq <= '1';
    end if;
end if;
end process;

src_addr_200                    <= configfifo_q(448+2*ADDRESS_WIDTH-1 downto 448+ADDRESS_WIDTH);
dst_addr_200                    <= configfifo_q(448+ADDRESS_WIDTH-1 downto 448);
config5_200                     <= configfifo_q(447 downto 384);
config4_200                     <= configfifo_q(383 downto 320);
config3_200                     <= configfifo_q(319 downto 256);
config2_200                     <= configfifo_q(255 downto 192);
config1_200                     <= configfifo_q(191 downto 128);
number_of_CL_to_process_200     <= configfifo_q(127 downto 96);
addr_reset_200                  <= configfifo_q(95 downto 64);
read_offset_200                 <= configfifo_q(63 downto 32);
write_offset_200                <= configfifo_q(31 downto 0);
configfifo: async_fifo
generic map (
    FIFO_WIDTH => 448+2*ADDRESS_WIDTH,
    FIFO_DEPTH_BITS => 6,
    ACK => 0)
port map(
	data => configfifo_data,
	wrreq => configfifo_wrreq,
	rdreq => configfifo_rdreq,
	wrclk => clk_400,
	rdclk => clk_200,
	q => configfifo_q,
	rdempty => configfifo_rdempty,
	wrfull => configfifo_wrfull);
----------------------------------------------------------- Configuration END

end architecture;
