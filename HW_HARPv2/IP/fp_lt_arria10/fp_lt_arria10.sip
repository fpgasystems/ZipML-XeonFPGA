set_global_assignment -entity "fp_lt_arria10" -library "lib_fp_lt_arria10" -name IP_TOOL_NAME "QsysPrimePro"
set_global_assignment -entity "fp_lt_arria10" -library "lib_fp_lt_arria10" -name IP_TOOL_VERSION "16.0"
set_global_assignment -entity "fp_lt_arria10" -library "lib_fp_lt_arria10" -name IP_TOOL_ENV "QsysPrimePro"
set_global_assignment -library "lib_fp_lt_arria10" -name SPD_FILE [file join $::quartus(sip_path) "fp_lt_arria10.spd"]
set_global_assignment -library "lib_fp_lt_arria10" -name MISC_FILE [file join $::quartus(sip_path) "../fp_lt_arria10.qsys"]

set_global_assignment -library "lib_fp_lt_arria10" -name MISC_FILE [file join $::quartus(sip_path) "sim/fp_lt_arria10.v"]
set_global_assignment -library "lib_fp_lt_arria10" -name MISC_FILE [file join $::quartus(sip_path) "altera_fp_functions_160/sim/dspba_library_package.vhd"]
set_global_assignment -library "lib_fp_lt_arria10" -name MISC_FILE [file join $::quartus(sip_path) "altera_fp_functions_160/sim/dspba_library.vhd"]
set_global_assignment -library "lib_fp_lt_arria10" -name MISC_FILE [file join $::quartus(sip_path) "altera_fp_functions_160/sim/fp_lt_arria10_altera_fp_functions_160_wduzk3i.vhd"]
