set_global_assignment -entity "fp_exp_arria10_nohard" -library "lib_fp_exp_arria10_nohard" -name IP_TOOL_NAME "QsysPrimePro"
set_global_assignment -entity "fp_exp_arria10_nohard" -library "lib_fp_exp_arria10_nohard" -name IP_TOOL_VERSION "16.0"
set_global_assignment -entity "fp_exp_arria10_nohard" -library "lib_fp_exp_arria10_nohard" -name IP_TOOL_ENV "QsysPrimePro"
set_global_assignment -library "lib_fp_exp_arria10_nohard" -name SPD_FILE [file join $::quartus(sip_path) "fp_exp_arria10_nohard.spd"]
set_global_assignment -library "lib_fp_exp_arria10_nohard" -name MISC_FILE [file join $::quartus(sip_path) "../fp_exp_arria10_nohard.qsys"]

set_global_assignment -library "lib_fp_exp_arria10_nohard" -name MISC_FILE [file join $::quartus(sip_path) "sim/fp_exp_arria10_nohard.v"]
set_global_assignment -library "lib_fp_exp_arria10_nohard" -name MISC_FILE [file join $::quartus(sip_path) "altera_fp_functions_160/sim/dspba_library_package.vhd"]
set_global_assignment -library "lib_fp_exp_arria10_nohard" -name MISC_FILE [file join $::quartus(sip_path) "altera_fp_functions_160/sim/dspba_library.vhd"]
set_global_assignment -library "lib_fp_exp_arria10_nohard" -name MISC_FILE [file join $::quartus(sip_path) "altera_fp_functions_160/sim/fp_exp_arria10_nohard_altera_fp_functions_160_724m2py.vhd"]
