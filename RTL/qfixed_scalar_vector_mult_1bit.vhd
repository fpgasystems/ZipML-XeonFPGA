library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity qfixed_scalar_vector_mult_1bit is
generic (
	VECTOR_SIZE : integer := 16);
port (
	clk : in std_logic;

	trigger : in std_logic;
	scalar : in std_logic_vector(31 downto 0);
	vector : in std_logic_vector(127 downto 0);
	result_almost_valid : out std_logic;
	result_valid : out std_logic;
	result : out std_logic_vector(32*VECTOR_SIZE-1 downto 0));
end qfixed_scalar_vector_mult_1bit;

architecture behavioral of qfixed_scalar_vector_mult_1bit is

constant QUANTIZATION_BITS : integer := 1;

begin

result_almost_valid <= trigger;

process(clk)
begin
if clk'event and clk = '1' then
	result_valid <= trigger;
	for k in 0 to 127 loop
		if vector(k) = '0' then
			result(32*k+31 downto 32*k) <= (others => '0');
		else
			result(32*k+31 downto 32*k) <= scalar;
		end if;
	end loop;
end if;
end process;

end architecture;