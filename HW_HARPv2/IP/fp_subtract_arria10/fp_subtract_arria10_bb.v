
module fp_subtract_arria10 (
	a,
	areset,
	b,
	clk,
	q);	

	input	[31:0]	a;
	input		areset;
	input	[31:0]	b;
	input		clk;
	output	[31:0]	q;
endmodule
