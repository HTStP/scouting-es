# Remote Firmware Reset Server
# Tcl socket server based on this example: https://wiki.tcl-lang.org/page/The+simplest+possible+socket+demonstration

proc init_hw {} {
    open_hw
    connect_hw_server -url localhost:3121
    current_hw_target [get_hw_targets */xilinx_tcf/Xilinx/1234-tulA]
    set_property PARAM.FREQUENCY 15000000 [get_hw_targets */xilinx_tcf/Xilinx/1234-tulA]
    open_hw_target
    set_property PROGRAM.FILE {/home/scouter/bitfiles/use_with_vivado_2018.2/eight_links_zs_lids_from_protocol/top.bit} [get_hw_devices xcku115_0]
    set_property PROBES.FILE {/home/scouter/bitfiles/use_with_vivado_2018.2/eight_links_zs_lids_from_protocol/top.ltx} [get_hw_devices xcku115_0]
    set_property FULL_PROBES.FILE {/home/scouter/bitfiles/use_with_vivado_2018.2/eight_links_zs_lids_from_protocol/top.ltx} [get_hw_devices xcku115_0]
    current_hw_device [get_hw_devices xcku115_0]
    refresh_hw_device [lindex [get_hw_devices xcku115_0] 0]
}

proc reset {} {
    puts "Resetting the board..."
    set_property OUTPUT_VALUE 1 [get_hw_probes rst_global -of_objects [get_hw_vios -of_objects [get_hw_devices xcku115_0] -filter {CELL_NAME=~"central_vio"}]]
    commit_hw_vio [get_hw_probes {rst_global} -of_objects [get_hw_vios -of_objects [get_hw_devices xcku115_0] -filter {CELL_NAME=~"central_vio"}]]
    after 1000
    set_property OUTPUT_VALUE 0 [get_hw_probes rst_global -of_objects [get_hw_vios -of_objects [get_hw_devices xcku115_0] -filter {CELL_NAME=~"central_vio"}]]
    commit_hw_vio [get_hw_probes {rst_global} -of_objects [get_hw_vios -of_objects [get_hw_devices xcku115_0] -filter {CELL_NAME=~"central_vio"}]]
    puts "Done"
}

proc accept {chan addr port} {           ;# Make a proc to accept connections
    gets $chan input                     ;# Receive a string
    puts "$addr:$port says $input"
    if {$input=="reset"} {
	puts stdout "RESET request detected"
	reset
	puts $chan ok
    } else {
	puts $chan goodbye               ;# Send a string
    }
    close $chan                          ;# Close the socket (automatically flushes)
}   


# main

puts stdout "Connecting to the Board..."
init_hw
puts stdout ""

puts stdout "Starting HW Reset Server..."
socket -server accept 12345              ;# Create a server socket
vwait forever                            ;# Enter the event loop
