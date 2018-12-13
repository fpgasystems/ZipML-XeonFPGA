puts "Original Line: 1 Profiled Line: 2 Time: [clock format [clock seconds] -format {%T}]"
set project_file [ lindex $argv 0 ]
puts "Original Line: 2 Profiled Line: 4 Time: [clock format [clock seconds] -format {%T}]"
set board_ip [ lindex $argv 1 ]
puts "Original Line: 3 Profiled Line: 6 Time: [clock format [clock seconds] -format {%T}]"
set board_port [ lindex $argv 2 ]

puts "Original Line: 5 Profiled Line: 9 Time: [clock format [clock seconds] -format {%T}]"
puts "Loading project $project_file."
puts "Original Line: 6 Profiled Line: 11 Time: [clock format [clock seconds] -format {%T}]"
if { [ catch { set design_node [ design_load $project_file  ] } ] } {
puts "Original Line: 7 Profiled Line: 13 Time: [clock format [clock seconds] -format {%T}]"
  global errorInfo
puts "Original Line: 8 Profiled Line: 15 Time: [clock format [clock seconds] -format {%T}]"
  puts "Failed to load $project_file. $errorInfo."
puts "Original Line: 9 Profiled Line: 17 Time: [clock format [clock seconds] -format {%T}]"
  exit 1
puts "Original Line: 10 Profiled Line: 19 Time: [clock format [clock seconds] -format {%T}]"
}
puts "Original Line: 11 Profiled Line: 21 Time: [clock format [clock seconds] -format {%T}]"
array set design_markers [ marker_get_info $design_node ]

puts "Original Line: 13 Profiled Line: 24 Time: [clock format [clock seconds] -format {%T}]"
set existing_devices [ get_service_paths device ]

puts "Original Line: 15 Profiled Line: 27 Time: [clock format [clock seconds] -format {%T}]"
puts "Connecting to remote device on $board_ip:$board_port."
puts "Original Line: 16 Profiled Line: 29 Time: [clock format [clock seconds] -format {%T}]"
if { [ catch { set connection_node [ add_service tcp remote_system $board_ip $board_port ] } ] } {
puts "Original Line: 17 Profiled Line: 31 Time: [clock format [clock seconds] -format {%T}]"
  global errorInfo
puts "Original Line: 18 Profiled Line: 33 Time: [clock format [clock seconds] -format {%T}]"
  puts "Failed to connect to remote device. $errorInfo."
puts "Original Line: 19 Profiled Line: 35 Time: [clock format [clock seconds] -format {%T}]"
}

puts "Original Line: 21 Profiled Line: 38 Time: [clock format [clock seconds] -format {%T}]"
refresh_connections
puts "Original Line: 22 Profiled Line: 40 Time: [clock format [clock seconds] -format {%T}]"
get_service_paths device
puts "Original Line: 23 Profiled Line: 42 Time: [clock format [clock seconds] -format {%T}]"
if { [ catch { marker_get_info $connection_node } ] } {
puts "Original Line: 24 Profiled Line: 44 Time: [clock format [clock seconds] -format {%T}]"
  global errorInfo
puts "Original Line: 25 Profiled Line: 46 Time: [clock format [clock seconds] -format {%T}]"
  puts "System Console was unable to connect to $board_ip:$board_port successfully.$errorInfo"
puts "Original Line: 26 Profiled Line: 48 Time: [clock format [clock seconds] -format {%T}]"
  exit 1
puts "Original Line: 27 Profiled Line: 50 Time: [clock format [clock seconds] -format {%T}]"
}

puts "Original Line: 29 Profiled Line: 53 Time: [clock format [clock seconds] -format {%T}]"
set new_devices {}
puts "Original Line: 30 Profiled Line: 55 Time: [clock format [clock seconds] -format {%T}]"
foreach device [ get_service_paths device ] {
puts "Original Line: 31 Profiled Line: 57 Time: [clock format [clock seconds] -format {%T}]"
  if { [ lsearch $existing_devices $device ] < 0 } {
puts "Original Line: 32 Profiled Line: 59 Time: [clock format [clock seconds] -format {%T}]"
    set new_devices [ lappend $new_devices $device ]
puts "Original Line: 33 Profiled Line: 61 Time: [clock format [clock seconds] -format {%T}]"
  }
puts "Original Line: 34 Profiled Line: 63 Time: [clock format [clock seconds] -format {%T}]"
}

puts "Original Line: 36 Profiled Line: 66 Time: [clock format [clock seconds] -format {%T}]"
# Add delay here; may affect a hypothetical race condition.
puts "Original Line: 37 Profiled Line: 68 Time: [clock format [clock seconds] -format {%T}]"
after 1000

puts "Original Line: 39 Profiled Line: 71 Time: [clock format [clock seconds] -format {%T}]"
set at_least_one_device_matches_project 0
puts "Original Line: 40 Profiled Line: 73 Time: [clock format [clock seconds] -format {%T}]"
puts "Found new devices:"
puts "Original Line: 41 Profiled Line: 75 Time: [clock format [clock seconds] -format {%T}]"
foreach device $new_devices {
puts "Original Line: 42 Profiled Line: 77 Time: [clock format [clock seconds] -format {%T}]"
  puts "\t$device"
puts "Original Line: 43 Profiled Line: 79 Time: [clock format [clock seconds] -format {%T}]"
  array set device_markers [ marker_get_info $device ]
puts "Original Line: 44 Profiled Line: 81 Time: [clock format [clock seconds] -format {%T}]"
  if { $design_markers(DESIGN_HASH) == $device_markers(DESIGN_HASH) } {
puts "Original Line: 45 Profiled Line: 83 Time: [clock format [clock seconds] -format {%T}]"
    set at_least_one_device_matches_project 1
puts "Original Line: 46 Profiled Line: 85 Time: [clock format [clock seconds] -format {%T}]"
  }
puts "Original Line: 47 Profiled Line: 87 Time: [clock format [clock seconds] -format {%T}]"
}
puts "Original Line: 48 Profiled Line: 89 Time: [clock format [clock seconds] -format {%T}]"
if { !$at_least_one_device_matches_project } {
puts "Original Line: 49 Profiled Line: 91 Time: [clock format [clock seconds] -format {%T}]"
  puts "The project $project_file didn't match any of the newly discovered devices."
puts "Original Line: 50 Profiled Line: 93 Time: [clock format [clock seconds] -format {%T}]"
  exit 1
puts "Original Line: 51 Profiled Line: 95 Time: [clock format [clock seconds] -format {%T}]"
}

puts "Original Line: 53 Profiled Line: 98 Time: [clock format [clock seconds] -format {%T}]"
puts "Remote system ready."
