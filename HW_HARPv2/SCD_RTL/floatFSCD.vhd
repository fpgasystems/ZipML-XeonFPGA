----------------------------------------------------------------------------
--  Copyright (C) 2018 Kaan Kara - Systems Group, ETH Zurich

--  This program is free software: you can redistribute it and/or modify
--  it under the terms of the GNU Affero General Public License as published
--  by the Free Software Foundation, either version 3 of the License, or
--  (at your option) any later version.

--  This program is distributed in the hope that it will be useful,
--  but WITHOUT ANY WARRANTY; without even the implied warranty of
--  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
--  GNU Affero General Public License for more details.

--  You should have received a copy of the GNU Affero General Public License
--  along with this program. If not, see <http://www.gnu.org/licenses/>.
----------------------------------------------------------------------------

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity floatFSCD is
generic(ADDRESS_WIDTH : integer := 32;
		LOG2_MAX_iBATCHSIZE : integer := 9);
port(
	clk: in std_logic;
	resetn : in std_logic;

	read_request : out std_logic;
	read_request_address : out std_logic_vector(ADDRESS_WIDTH-1 downto 0);
	read_request_tid : out std_logic_vector(15 downto 0);
	read_request_almostfull : in std_logic;

	read_response : in std_logic;
	read_response_data : in std_logic_vector(511 downto 0);
	read_response_tid : in std_logic_vector(15 downto 0);

	write_request : out std_logic;
	write_request_address : out std_logic_vector(ADDRESS_WIDTH-1 downto 0);
	write_request_data : out std_logic_vector(511 downto 0);
	write_request_almostfull : in std_logic;

	write_response : in std_logic;

	start : in std_logic;
	done : out std_logic;

	enable_staleness : in std_logic;
	a_address : in std_logic_vector(ADDRESS_WIDTH-1 downto 0);
	b_address : in std_logic_vector(ADDRESS_WIDTH-1 downto 0);
	step_address : in std_logic_vector(ADDRESS_WIDTH-1 downto 0);
	residual_address : in std_logic_vector(ADDRESS_WIDTH-1 downto 0);
	number_of_features : in std_logic_vector(31 downto 0);
	number_of_batches : in std_logic_vector(15 downto 0);
	batch_size : in std_logic_vector(15 downto 0);
	step_size : in std_logic_vector(31 downto 0);
	number_of_epochs : in std_logic_vector(15 downto 0));
end floatFSCD;

architecture behavioral of floatFSCD is

signal reset : std_logic;
constant LOG2_VALUE_WIDTH : integer := 5;
constant LOG2_LINE_WIDTH : integer := 9;
constant LOG2_VALUES_PER_LINE : integer := LOG2_LINE_WIDTH-LOG2_VALUE_WIDTH;
constant VALUES_PER_LINE : integer := 2**LOG2_VALUES_PER_LINE;

signal iNUMBER_OF_EPOCHS : unsigned(15 downto 0) := (others => '0');
signal iNUMBER_OF_FEATURES : unsigned(31 downto 0) := (others => '0');
signal iNUMBER_OF_BATCHES : unsigned(15 downto 0) := (others => '0');
signal iBATCH_SIZE : unsigned(15 downto 0) := (others => '0');
signal iBATCH_OFFSET : unsigned(31 downto 0) := (others => '0');
signal write_iBATCH_OFFSET : unsigned(31 downto 0) := (others => '0');
signal iCOLUMN_SIZE : unsigned(31 downto 0) := (others => '0');
signal iCOLUMN_OFFSET : unsigned(63 downto 0) := (others => '0');

-- 0 read residual, 1 read b, 3 read a
signal read_state : std_logic_vector(1 downto 0) := (others => '0');
signal receive_state : std_logic_vector(1 downto 0) := (others => '0');
signal completed_epochs : unsigned(15 downto 0);

signal NumberOfRequestedReads : unsigned(31 downto 0) := (others => '0');
signal residual_NumberOfRequestedReads : unsigned(31 downto 0) := (others => '0');
signal b_NumberOfRequestedReads : unsigned(31 downto 0) := (others => '0');
signal a_NumberOfRequestedReads : unsigned(31 downto 0) := (others => '0');
signal NumberOfReceivedReads : unsigned(31 downto 0) := (others => '0');
signal residual_NumberOfReceivedReads : unsigned(31 downto 0) := (others => '0');
signal b_NumberOfReceivedReads : unsigned(31 downto 0) := (others => '0');
signal a_NumberOfReceivedReads : unsigned(31 downto 0) := (others => '0');
signal residual_NumberOfWriteRequests : unsigned(31 downto 0) := (others => '0');
signal step_NumberOfWriteRequests : unsigned(31 downto 0) := (others => '0');
signal NumberOfWriteResponses : unsigned(31 downto 0) := (others => '0');

signal feature_index : unsigned(31 downto 0) := (others => '0');
signal feature_receive_index : unsigned(31 downto 0) := (others => '0');
signal feature_update_index : unsigned(31 downto 0) := (others => '0');
signal batch_index : unsigned(15 downto 0) := (others => '0');
signal write_batch_index : unsigned(15 downto 0) := (others => '0');
signal i_index : unsigned(LOG2_MAX_iBATCHSIZE-1 downto 0) := (others => '0');
signal i_receive_index : unsigned(LOG2_MAX_iBATCHSIZE-1 downto 0) := (others => '0');
signal i_write_index : unsigned(LOG2_MAX_iBATCHSIZE-1 downto 0) := (others => '0');
signal i_update_index : unsigned(LOG2_MAX_iBATCHSIZE-1 downto 0) := (others => '0');
signal i_writeback_index : unsigned(LOG2_MAX_iBATCHSIZE-1 downto 0) := (others => '0');
signal i_writerequest_index : unsigned(LOG2_MAX_iBATCHSIZE-1 downto 0) := (others => '0');

signal new_column_read_allowed : std_logic;
signal write_residual_back : std_logic;
signal finish_allowed : std_logic;

constant INPUT_VECTOR_DELAY_CYCLES : integer := 4;
type input_vector_type is array(INPUT_VECTOR_DELAY_CYCLES downto 0) of std_logic_vector(511 downto 0);
signal input_vector : input_vector_type;

signal reorder_start_address_adjust : std_logic;
signal reorder_start_address : std_logic_vector(15 downto 0);
signal reordered_response_data : std_logic_vector(511 downto 0);
signal reordered_resonse : std_logic;

signal residual_store_raddr : std_logic_vector(LOG2_MAX_iBATCHSIZE-1 downto 0);
signal residual_store_waddr : std_logic_vector(LOG2_MAX_iBATCHSIZE-1 downto 0);
signal residual_store_din : std_logic_vector(511 downto 0);
signal residual_store_we : std_logic;
signal residual_store_re : std_logic;
signal residual_store_dout : std_logic_vector(511 downto 0);

signal residual_store_loading_raddr : std_logic_vector(LOG2_MAX_iBATCHSIZE-1 downto 0);
signal residual_store_loading_waddr : std_logic_vector(LOG2_MAX_iBATCHSIZE-1 downto 0);
signal residual_store_loading_din : std_logic_vector(511 downto 0);
signal residual_store_loading_we : std_logic;
signal residual_store_loading_re : std_logic;
signal residual_store_loading_dout : std_logic_vector(511 downto 0);
signal residual_writeback_re : std_logic;
signal residual_writeback_re_1d : std_logic;

signal b_store_raddr : std_logic_vector(LOG2_MAX_iBATCHSIZE-1 downto 0);
signal b_store_waddr : std_logic_vector(LOG2_MAX_iBATCHSIZE-1 downto 0);
signal b_store_din : std_logic_vector(511 downto 0);
signal b_store_we : std_logic;
signal b_store_re : std_logic;
signal b_store_dout : std_logic_vector(511 downto 0);

signal a_fifo_we : std_logic;
signal a_fifo_din : std_logic_vector(511 downto 0);
signal a_fifo_re : std_logic;
signal a_fifo_valid : std_logic;
signal a_fifo_dout : std_logic_vector(511 downto 0);
signal a_fifo_count : std_logic_vector(LOG2_MAX_iBATCHSIZE-1 downto 0);
signal a_fifo_empty : std_logic;
signal a_fifo_full : std_logic;
signal a_fifo_almostfull: std_logic;
signal a_fifo_free_count : unsigned(LOG2_MAX_iBATCHSIZE-1 downto 0);

signal residual_minus_b_trigger : std_logic;
signal residual_minus_b_almost_valid : std_logic;
signal residual_minus_b_valid : std_logic;
signal residual_minus_b : std_logic_vector(511 downto 0);

signal dot_almost_valid : std_logic;
signal dot_valid : std_logic;
signal dot_valid_1d : std_logic;
signal dot_valid_2d : std_logic;
signal dot_valid_3dn : std_logic;
signal dot : std_logic_vector(31 downto 0);
signal step : std_logic_vector(31 downto 0);
signal step_valid : std_logic;

signal delta_almost_valid : std_logic;
signal delta_valid : std_logic;
signal delta : std_logic_vector(511 downto 0);
signal delta0 : std_logic_vector(31 downto 0);

signal new_residual_almost_valid : std_logic;
signal new_residual_valid : std_logic;
signal new_residual : std_logic_vector(511 downto 0);
signal new_residual0 : std_logic_vector(31 downto 0);

signal step_writeback_valid : std_logic;
signal step_writeback : std_logic_vector(511 downto 0);
signal step_writeback_index : integer range 0 to 15 := 0;


component simple_dual_port_ram_single_clock
generic(
	DATA_WIDTH : natural := 8;
	ADDR_WIDTH : natural := 6);
port(
	clk		: in std_logic;
	raddr	: in std_logic_vector(ADDR_WIDTH-1 downto 0);
	waddr	: in std_logic_vector(ADDR_WIDTH-1 downto 0);
	data	: in std_logic_vector((DATA_WIDTH-1) downto 0);
	we		: in std_logic := '1';
	q		: out std_logic_vector((DATA_WIDTH -1) downto 0));
end component;

component fifo
generic(
	FIFO_WIDTH : integer := 32;
	FIFO_DEPTH_BITS : integer := 8;
	FIFO_ALMOSTFULL_THRESHOLD : integer := 220);
port(
	clk :		in std_logic;
	resetn :	in std_logic;

	we :		in std_logic;
	din :		in std_logic_vector(FIFO_WIDTH-1 downto 0);	
	re :		in std_logic;
	valid :		out std_logic;
	dout :		out std_logic_vector(FIFO_WIDTH-1 downto 0);
	count :		out std_logic_vector(FIFO_DEPTH_BITS-1 downto 0);
	empty :		out std_logic;
	full :		out std_logic;
	almostfull: out std_logic);
end component;

component reorder
generic(
	LOG2_BUFFER_DEPTH : integer := 8;
	ADDRESS_WIDTH : integer := 32);
port (
	clk : in std_logic;
	resetn : in std_logic;
	start_address_adjust : std_logic;
	start_address : in std_logic_vector(ADDRESS_WIDTH-1 downto 0);
	in_trigger : in std_logic;
	in_address : in std_logic_vector(ADDRESS_WIDTH-1 downto 0);
	in_data : in std_logic_vector(511 downto 0);
	out_data : out std_logic_vector(511 downto 0);
	out_valid : out std_logic);
end component;

component float_vector_subtract
generic (VALUES_PER_LINE : integer := 16);
port (
	clk : in std_logic;
	resetn : in std_logic;
	trigger : in std_logic;
	vector1 : in std_logic_vector(32*VALUES_PER_LINE-1 downto 0);
	vector2 : in std_logic_vector(32*VALUES_PER_LINE-1 downto 0);
	result_almost_valid : out std_logic;
	result_valid : out std_logic;
	result : out std_logic_vector(32*VALUES_PER_LINE-1 downto 0));
end component;

component hybrid_dot_product
generic (
	LOG2_VALUES_PER_LINE : integer := 5);
port (
	clk : in std_logic;
	resetn : in std_logic;
	trigger : in std_logic;
	accumulation_count : in std_logic_vector(15 downto 0);
	vector1 : in std_logic_vector(511 downto 0);
	vector2 : in std_logic_vector(511 downto 0);
	result_almost_valid : out std_logic;
	result_valid : out std_logic;
	result : out std_logic_vector(31 downto 0));
end component;

component fp_mult_arria10
	port (
		a      : in  std_logic_vector(31 downto 0) := (others => '0'); --      a.a
		areset : in  std_logic                     := '0';             -- areset.reset
		b      : in  std_logic_vector(31 downto 0) := (others => '0'); --      b.b
		clk    : in  std_logic                     := '0';             --    clk.clk
		q      : out std_logic_vector(31 downto 0)                     --      q.q
	);
end component;

component fp_subtract_arria10
	port (
		a      : in  std_logic_vector(31 downto 0) := (others => '0'); --      a.a
		areset : in  std_logic                     := '0';             -- areset.reset
		b      : in  std_logic_vector(31 downto 0) := (others => '0'); --      b.b
		clk    : in  std_logic                     := '0';             --    clk.clk
		q      : out std_logic_vector(31 downto 0)                     --      q.q
	);
end component;

component float_scalar_vector_mult
generic (VALUES_PER_LINE : integer := 16);
port (
	clk : in std_logic;
	resetn : in std_logic;
	trigger : in std_logic;
	scalar : in std_logic_vector(31 downto 0);
	vector : in std_logic_vector(32*VALUES_PER_LINE-1 downto 0);
	result_almost_valid : out std_logic;
	result_valid : out std_logic;
	result : out std_logic_vector(32*VALUES_PER_LINE-1 downto 0));
end component;

begin

delta0 <= delta(31 downto 0);
new_residual0 <= new_residual(31 downto 0);

reset <= not resetn;

reordering: reorder
generic map (
	LOG2_BUFFER_DEPTH => 8,
	ADDRESS_WIDTH => 16)
port map (
	clk => clk,
	resetn => resetn,

	start_address_adjust => reorder_start_address_adjust,
	start_address => reorder_start_address,
	in_trigger => read_response,
	in_address => read_response_tid,
	in_data => read_response_data,
	out_data => reordered_response_data,
	out_valid => reordered_resonse);

residual_store: simple_dual_port_ram_single_clock
generic map (
	DATA_WIDTH => 512,
	ADDR_WIDTH => LOG2_MAX_iBATCHSIZE)
port map (
	clk => clk,
	raddr => residual_store_raddr,
	waddr => residual_store_waddr,
	data => residual_store_din,
	we => residual_store_we,
	q => residual_store_dout);

residual_store_loading: simple_dual_port_ram_single_clock
generic map (
	DATA_WIDTH => 512,
	ADDR_WIDTH => LOG2_MAX_iBATCHSIZE)
port map (
	clk => clk,
	raddr => residual_store_loading_raddr,
	waddr => residual_store_loading_waddr,
	data => residual_store_loading_din,
	we => residual_store_loading_we,
	q => residual_store_loading_dout);

b_store: simple_dual_port_ram_single_clock
generic map (
	DATA_WIDTH => 512,
	ADDR_WIDTH => LOG2_MAX_iBATCHSIZE)
port map (
	clk => clk,
	raddr => b_store_raddr,
	waddr => b_store_waddr,
	data => b_store_din,
	we => b_store_we,
	q => b_store_dout);

a_fifo_re <= dot_valid;
a_fifo: fifo
generic map (
	FIFO_WIDTH => 512,
	FIFO_DEPTH_BITS => LOG2_MAX_iBATCHSIZE,
	FIFO_ALMOSTFULL_THRESHOLD => 2**LOG2_MAX_iBATCHSIZE-30)
port map (
	clk => clk,
	resetn => resetn,

	we => a_fifo_we,
	din => a_fifo_din,
	re => a_fifo_re,
	valid => a_fifo_valid,
	dout => a_fifo_dout,
	count => a_fifo_count,
	empty => a_fifo_empty,
	full => a_fifo_full,
	almostfull => a_fifo_almostfull);

COMP_residual_minus_b: float_vector_subtract
generic map (
	VALUES_PER_LINE => VALUES_PER_LINE)
port map (
	clk => clk,
	resetn => resetn,
	trigger => residual_minus_b_trigger,
	vector1 => residual_store_loading_dout,
	vector2 => b_store_dout,
	result_almost_valid => residual_minus_b_almost_valid,
	result_valid => residual_minus_b_valid,
	result => residual_minus_b);

COMP_dot: hybrid_dot_product
generic map (
	LOG2_VALUES_PER_LINE => LOG2_VALUES_PER_LINE)
port map (
	clk => clk,
	resetn => resetn,
	trigger => residual_minus_b_valid,
	accumulation_count => batch_size,
	vector1 => residual_minus_b,
	vector2 => input_vector(INPUT_VECTOR_DELAY_CYCLES),
	result_almost_valid => dot_almost_valid,
	result_valid => dot_valid,
	result => dot);

COMP_step_size_mult: fp_mult_arria10
port map (
	a => dot,
	areset => reset,
	b => step_size,
	clk => clk,
	q => step);

COMP_scalar_vector_mult: float_scalar_vector_mult
generic map (
	VALUES_PER_LINE => VALUES_PER_LINE)
port map (
	clk => clk,
	resetn => resetn,
	trigger => a_fifo_valid,
	scalar => step,
	vector => a_fifo_dout,
	result_almost_valid => delta_almost_valid,
	result_valid => delta_valid,
	result => delta);

COMP_residual_minus_delta: float_vector_subtract
generic map (
	VALUES_PER_LINE => VALUES_PER_LINE)
port map (
	clk => clk,
	resetn => resetn,
	trigger => delta_valid,
	vector1 => residual_store_loading_dout,
	vector2 => delta,
	result_almost_valid => new_residual_almost_valid,
	result_valid => new_residual_valid,
	result => new_residual);

process(clk)
begin
if clk'event and clk = '1' then
	for k in 1 to INPUT_VECTOR_DELAY_CYCLES loop
		input_vector(k) <= input_vector(k-1);
	end loop;
	residual_minus_b_trigger <= residual_store_re and b_store_re;
	dot_valid_1d <= dot_valid;
	dot_valid_2d <= dot_valid_1d;
	dot_valid_3dn <= not dot_valid_2d;
	step_valid <= dot_valid_3dn and dot_valid_2d;

	iNUMBER_OF_EPOCHS <= unsigned(number_of_epochs);
	iNUMBER_OF_FEATURES <= unsigned(number_of_features);
	iNUMBER_OF_BATCHES <= unsigned(number_of_batches);
	iBATCH_SIZE <= unsigned(batch_size);
	iBATCH_OFFSET <= batch_index*iBATCH_SIZE;
	write_iBATCH_OFFSET <= write_batch_index*iBATCH_SIZE;
	iCOLUMN_SIZE <= iNUMBER_OF_BATCHES*iBATCH_SIZE;
	iCOLUMN_OFFSET <= feature_index*iCOLUMN_SIZE + iBATCH_OFFSET;

	if resetn = '0' then
		read_request <= '0';
		write_request <= '0';
		done <= '0';

		read_state <= B"00";
		receive_state <= B"00";
		completed_epochs <= (others => '0');

		NumberOfRequestedReads <= (others => '0');
		residual_NumberOfRequestedReads <= (others => '0');
		b_NumberOfRequestedReads <= (others => '0');
		a_NumberOfRequestedReads <= (others => '0');
		residual_NumberOfReceivedReads <= (others => '0');
		b_NumberOfReceivedReads <= (others => '0');
		a_NumberOfReceivedReads <= (others => '0');
		residual_NumberOfWriteRequests <= (others => '0');
		step_NumberOfWriteRequests <= (others => '0');
		NumberOfWriteResponses <= (others => '0');

		feature_index <= (others => '0');
		feature_receive_index <= (others => '0');
		feature_update_index <= (others => '0');
		batch_index <= (others => '0');
		write_batch_index <= (others => '0');
		i_index <= (others => '0');
		i_receive_index <= (others => '0');
		i_write_index <= (others => '0');
		i_update_index <= (others => '0');
		i_writeback_index <= (others => '0');
		i_writerequest_index <= (others => '0');

		new_column_read_allowed <= '1';
		write_residual_back <= '0';
		finish_allowed <= '0';

		reorder_start_address_adjust <= '0';

		a_fifo_we <= '0';

		step_writeback_valid <= '0';
		step_writeback_index <= 0;
	else
		-- Request lines
		read_request <= '0';
		reorder_start_address_adjust <= '0';
		if start = '1' and write_request_almostfull = '0' and read_request_almostfull = '0' and a_NumberOfRequestedReads < iCOLUMN_SIZE*iNUMBER_OF_FEATURES and new_column_read_allowed = '1' then
			read_request <= '1';
			read_request_tid <= std_logic_vector(NumberOfRequestedReads(15 downto 0));
			NumberOfRequestedReads <= NumberOfRequestedReads + 1;
			if NumberOfRequestedReads = 0 then
				reorder_start_address_adjust <= '1';
				reorder_start_address <= (others => '0');
			end if;

			if read_state = B"00" then --read residual
				read_request_address <= std_logic_vector(unsigned(residual_address) + iBATCH_OFFSET + i_index);

				if i_index = iBATCH_SIZE-1 then
					i_index <= (others => '0');
					read_state <= B"01";
				else
					i_index <= i_index + 1;
				end if;

				residual_NumberOfRequestedReads <= residual_NumberOfRequestedReads + 1;
			elsif read_state = B"01" then --read b
				read_request_address <= std_logic_vector(unsigned(b_address) + iBATCH_OFFSET + i_index);

				if i_index = iBATCH_SIZE-1 then
					i_index <= (others => '0');
					read_state <= B"11";
				else
					i_index <= i_index + 1;
				end if;

				b_NumberOfRequestedReads <= b_NumberOfRequestedReads + 1;
			else -- read a
				read_request_address <= std_logic_vector(unsigned(a_address) + iCOLUMN_OFFSET(ADDRESS_WIDTH-1 downto 0) + i_index);

				if i_index = iBATCH_SIZE-1 then
					new_column_read_allowed <= enable_staleness;
					i_index <= (others => '0');	
					if feature_index = iNUMBER_OF_FEATURES-1 then
						read_state <= B"00";
						feature_index <= (others => '0');
						if batch_index = iNUMBER_OF_BATCHES-1 then
							batch_index <= (others => '0');
						else
							batch_index <= batch_index + 1;
						end if;
					else
						feature_index <= feature_index + 1;
					end if;
				else
					i_index <= i_index + 1;
				end if;

				a_NumberOfRequestedReads <= a_NumberOfRequestedReads + 1;
			end if;
		end if;

		-- Receive lines
		residual_store_we <= '0';
		residual_store_loading_we <= '0';
		b_store_we <= '0';
		a_fifo_we <= '0';
		residual_store_re <= '0';
		b_store_re <= '0';
		if reordered_resonse = '1' then
			NumberOfReceivedReads <= NumberOfReceivedReads + 1;
			if receive_state = B"00" then --receive residual
				residual_store_we <= '1';
				residual_store_loading_we <= '1';
				residual_store_din <= reordered_response_data;
				residual_store_loading_din <= reordered_response_data;
				residual_store_waddr <= std_logic_vector(i_receive_index);
				residual_store_loading_waddr <= std_logic_vector(i_receive_index);
				if i_receive_index = iBATCH_SIZE-1 then
					i_receive_index <= (others => '0');
					receive_state <= B"01";
				else
					i_receive_index <= i_receive_index + 1;
				end if;
				residual_NumberOfReceivedReads <= residual_NumberOfReceivedReads + 1;
			elsif receive_state = B"01" then --receive b
				b_store_we <= '1';
				b_store_din <= reordered_response_data;
				b_store_waddr <= std_logic_vector(i_receive_index);
				if i_receive_index = iBATCH_SIZE-1 then
					i_receive_index <= (others => '0');
					receive_state <= B"11";
				else
					i_receive_index <= i_receive_index + 1;
				end if;
				b_NumberOfReceivedReads <= b_NumberOfReceivedReads + 1;
			else --receive a
				a_fifo_we <= '1';
				a_fifo_din <= reordered_response_data;

				input_vector(0) <= reordered_response_data;

				residual_store_re <= '1';
				b_store_re <= '1';
				residual_store_raddr <= std_logic_vector(i_receive_index);
				b_store_raddr <= std_logic_vector(i_receive_index);

				if i_receive_index = iBATCH_SIZE-1 then
					i_receive_index <= (others => '0');
					if feature_receive_index = iNUMBER_OF_FEATURES-1 then
						receive_state <= B"00";
						feature_receive_index <= (others => '0');
					else
						feature_receive_index <= feature_receive_index + 1;
					end if;
				else
					i_receive_index <= i_receive_index + 1;
				end if;
				a_NumberOfReceivedReads <= a_NumberOfReceivedReads + 1;
			end if;
		end if;


		residual_store_loading_re <= '0';
		if delta_almost_valid = '1' then
			residual_store_loading_re <= '1';
			residual_store_loading_raddr <= std_logic_vector(i_write_index);
			if i_write_index = iBATCH_SIZE-1 then
				i_write_index <= (others => '0');
			else
				i_write_index <= i_write_index + 1;
			end if;
		end if;


		step_writeback_valid <= '0';
		if new_residual_valid = '1' then
			residual_store_we <= '1';
			residual_store_waddr <= std_logic_vector(i_update_index);
			residual_store_din <= new_residual;
			residual_store_loading_we <= '1';
			residual_store_loading_waddr <= std_logic_vector(i_update_index);
			residual_store_loading_din <= new_residual;

			if i_update_index = 0 then
				new_column_read_allowed <= '1';
			end if;
			if i_update_index = iBATCH_SIZE-1 then
				i_update_index <= (others => '0');
				if feature_update_index = iNUMBER_OF_FEATURES-1 then
					feature_update_index <= (others => '0');
					write_residual_back <= '1';
					step_writeback_valid <= '1';
				else
					feature_update_index <= feature_update_index + 1;
				end if;
			else
				i_update_index <= i_update_index + 1;
			end if;
		end if;


		residual_writeback_re <= '0';
		if write_residual_back = '1' and write_request_almostfull = '0' then
			residual_store_loading_re <= '1';
			residual_writeback_re <= '1';
			residual_store_loading_raddr <= std_logic_vector(i_writeback_index);
			if i_writeback_index = iBATCH_SIZE-1 then
				i_writeback_index <= (others => '0');
				write_residual_back <= '0';
				new_column_read_allowed <= '1';
			else
				i_writeback_index <= i_writeback_index + 1;
			end if;
		end if;
		residual_writeback_re_1d <= residual_writeback_re;


		write_request <= '0';
		if residual_writeback_re_1d = '1' then
			write_request <= '1';
			write_request_address <= std_logic_vector(unsigned(residual_address) + write_iBATCH_OFFSET + i_writerequest_index);
			write_request_data <= residual_store_loading_dout;
			if i_writerequest_index = iBATCH_SIZE-1 then
				i_writerequest_index <= (others => '0');
				if write_batch_index = iNUMBER_OF_BATCHES-1 then
					write_batch_index <= (others => '0');

					if completed_epochs = iNUMBER_OF_EPOCHS-1 then
						finish_allowed <= '1';
					else
						residual_NumberOfRequestedReads <= (others => '0');
						b_NumberOfRequestedReads <= (others => '0');
						a_NumberOfRequestedReads <= (others => '0');
						residual_NumberOfReceivedReads <= (others => '0');
						b_NumberOfReceivedReads <= (others => '0');
						a_NumberOfReceivedReads <= (others => '0');
					end if;
					completed_epochs <= completed_epochs + 1;

				else
					write_batch_index <= write_batch_index + 1;
				end if;
			else
				i_writerequest_index <= i_writerequest_index + 1;
			end if;
			residual_NumberOfWriteRequests <= residual_NumberOfWriteRequests + 1;
		end if;


		if step_valid = '1' then
			step_writeback( 32*(step_writeback_index+1)-1 downto 32*step_writeback_index ) <= step;
			if step_writeback_index = 15 then
				step_writeback_index <= 0;
				step_writeback_valid <= '1';
			else
				step_writeback_index <= step_writeback_index+1;
			end if;
		end if;
		if step_writeback_valid = '1' then
			step_writeback_index <= 0;
			write_request <= '1';
			write_request_address <= std_logic_vector(unsigned(step_address) + step_NumberOfWriteRequests);
			write_request_data <= step_writeback;
			step_NumberOfWriteRequests <= step_NumberOfWriteRequests + 1;
		end if;


		if write_response = '1' then
			NumberOfWriteResponses <= NumberOfWriteResponses + 1;
		end if;


		if finish_allowed = '1' and NumberOfWriteResponses = (residual_NumberOfWriteRequests+step_NumberOfWriteRequests) and NumberOfWriteResponses > 0 then
			finish_allowed <= '0';
			done <= '1';
		end if;


	end if;
end if;
end process;

end architecture;