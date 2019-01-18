set_global_assignment -entity "SCJIO" -library "lib_SCJIO" -name IP_TOOL_NAME "QsysPrimePro"
set_global_assignment -entity "SCJIO" -library "lib_SCJIO" -name IP_TOOL_VERSION "16.0"
set_global_assignment -entity "SCJIO" -library "lib_SCJIO" -name IP_TOOL_ENV "QsysPrimePro"
set_global_assignment -library "lib_SCJIO" -name SPD_FILE [file join $::quartus(sip_path) "SCJIO.spd"]
set_global_assignment -library "lib_SCJIO" -name MISC_FILE [file join $::quartus(sip_path) "../SCJIO.qsys"]

set_global_assignment -library "lib_SCJIO" -name MISC_FILE [file join $::quartus(sip_path) "sim/SCJIO.v"]
set_global_assignment -library "lib_SCJIO" -name MISC_FILE [file join $::quartus(sip_path) "altera_soft_core_jtag_io_160/sim/altera_soft_core_jtag_io.v"]
