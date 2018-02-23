# Important!!!
# After running this, start vsim with options (e.g., add to Makefile of ASE):
# -L altera_fp_functions_160 -L altera_ver -L lpm_ver -L sgate_ver -L altera_mf_ver -L altera_lnsim_ver -L twentynm_ver -L twentynm_hssi_ver -L twentynm_hip_ver -L altera -L lpm -L sgate -L altera_mf -L altera_lnsim -L twentynm -L twentynm_hssi -L twentynm_hip

foreach lib [list \
	altera_ver \
	lpm_ver \
	sgate_ver \
	altera_mf_ver \
	altera_lnsim_ver \
	twentynm_ver \
	twentynm_hssi_ver \
	twentynm_hip_ver \
	altera \
	lpm \
	sgate \
	altera_mf \
	altera_lnsim \
	twentynm \
	twentynm_hssi \
	twentynm_hip \
	altera_fp_functions_160
] {
	echo "Creating $lib"
	vlib $::env(ASE_WORKDIR)/../libraries/$lib
	vmap $lib $::env(ASE_WORKDIR)/../libraries/$lib
}

set QUARTUS_INSTALL_DIR "/home/kkara/Tools/altera_pro/16.0/quartus/"

eval  vlog  "$QUARTUS_INSTALL_DIR/eda/sim_lib/altera_primitives.v"                   -work altera_ver       
eval  vlog  "$QUARTUS_INSTALL_DIR/eda/sim_lib/220model.v"                            -work lpm_ver          
eval  vlog  "$QUARTUS_INSTALL_DIR/eda/sim_lib/sgate.v"                               -work sgate_ver        
eval  vlog  "$QUARTUS_INSTALL_DIR/eda/sim_lib/altera_mf.v"                           -work altera_mf_ver    
eval  vlog  "$QUARTUS_INSTALL_DIR/eda/sim_lib/mentor/altera_lnsim_for_vhdl.sv"       -work altera_lnsim_ver 
eval  vlog  "$QUARTUS_INSTALL_DIR/eda/sim_lib/mentor/twentynm_atoms_for_vhdl.v"      -work twentynm_ver     
eval  vlog  "$QUARTUS_INSTALL_DIR/eda/sim_lib/mentor/twentynm_atoms_ncrypt.v"        -work twentynm_ver     
eval  vlog  "$QUARTUS_INSTALL_DIR/eda/sim_lib/mentor/twentynm_hssi_atoms_ncrypt.v"   -work twentynm_hssi_ver
eval  vlog  "$QUARTUS_INSTALL_DIR/eda/sim_lib/mentor/twentynm_hssi_atoms_for_vhdl.v" -work twentynm_hssi_ver
eval  vlog  "$QUARTUS_INSTALL_DIR/eda/sim_lib/mentor/twentynm_hip_atoms_ncrypt.v"    -work twentynm_hip_ver 
eval  vlog  "$QUARTUS_INSTALL_DIR/eda/sim_lib/mentor/twentynm_hip_atoms_for_vhdl.v"  -work twentynm_hip_ver 
eval  vcom  "$QUARTUS_INSTALL_DIR/eda/sim_lib/altera_syn_attributes.vhd"             -work altera           
eval  vcom  "$QUARTUS_INSTALL_DIR/eda/sim_lib/altera_standard_functions.vhd"         -work altera           
eval  vcom  "$QUARTUS_INSTALL_DIR/eda/sim_lib/alt_dspbuilder_package.vhd"            -work altera           
eval  vcom  "$QUARTUS_INSTALL_DIR/eda/sim_lib/altera_europa_support_lib.vhd"         -work altera           
eval  vcom  "$QUARTUS_INSTALL_DIR/eda/sim_lib/altera_primitives_components.vhd"      -work altera           
eval  vcom  "$QUARTUS_INSTALL_DIR/eda/sim_lib/altera_primitives.vhd"                 -work altera           
eval  vcom  "$QUARTUS_INSTALL_DIR/eda/sim_lib/220pack.vhd"                           -work lpm              
eval  vcom  "$QUARTUS_INSTALL_DIR/eda/sim_lib/220model.vhd"                          -work lpm              
eval  vcom  "$QUARTUS_INSTALL_DIR/eda/sim_lib/sgate_pack.vhd"                        -work sgate            
eval  vcom  "$QUARTUS_INSTALL_DIR/eda/sim_lib/sgate.vhd"                             -work sgate            
eval  vcom  "$QUARTUS_INSTALL_DIR/eda/sim_lib/altera_mf_components.vhd"              -work altera_mf        
eval  vcom  "$QUARTUS_INSTALL_DIR/eda/sim_lib/altera_mf.vhd"                         -work altera_mf        
eval  vcom  "$QUARTUS_INSTALL_DIR/eda/sim_lib/altera_lnsim_components.vhd"           -work altera_lnsim     
eval  vcom  "$QUARTUS_INSTALL_DIR/eda/sim_lib/twentynm_atoms.vhd"                    -work twentynm         
eval  vcom  "$QUARTUS_INSTALL_DIR/eda/sim_lib/twentynm_components.vhd"               -work twentynm         
eval  vcom  "$QUARTUS_INSTALL_DIR/eda/sim_lib/twentynm_hssi_components.vhd"          -work twentynm_hssi    
eval  vcom  "$QUARTUS_INSTALL_DIR/eda/sim_lib/twentynm_hssi_atoms.vhd"               -work twentynm_hssi    
eval  vcom  "$QUARTUS_INSTALL_DIR/eda/sim_lib/twentynm_hip_components.vhd"           -work twentynm_hip     
eval  vcom  "$QUARTUS_INSTALL_DIR/eda/sim_lib/twentynm_hip_atoms.vhd"                -work twentynm_hip

eval  vcom  "$::env(DOPPIODB_HOME)/fpga/operators/sgd/IP/fp_adder_arria10/altera_fp_functions_160/sim/dspba_library_package.vhd" -work altera_fp_functions_160
eval  vcom  "$::env(DOPPIODB_HOME)/fpga/operators/sgd/IP/fp_adder_arria10/altera_fp_functions_160/sim/dspba_library.vhd" -work altera_fp_functions_160
eval  vcom  "$::env(DOPPIODB_HOME)/fpga/operators/sgd/IP/fp_adder_arria10/altera_fp_functions_160/sim/fp_adder_arria10_altera_fp_functions_160_3ch56ay.vhd" -work altera_fp_functions_160
eval  vcom  "$::env(DOPPIODB_HOME)/fpga/operators/sgd/IP/fp_converter_arria10/altera_fp_functions_160/sim/fp_converter_arria10_altera_fp_functions_160_ldxmaja.vhd" -work altera_fp_functions_160
eval  vcom  "$::env(DOPPIODB_HOME)/fpga/operators/sgd/IP/fp_mult_arria10/altera_fp_functions_160/sim/fp_mult_arria10_altera_fp_functions_160_djoefaq.vhd" -work altera_fp_functions_160