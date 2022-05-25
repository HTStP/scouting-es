open_hw
connect_hw_server -url localhost:3121
current_hw_target [get_hw_targets */xilinx_tcf/Xilinx/1234-tulA]
set_property PARAM.FREQUENCY 15000000 [get_hw_targets */xilinx_tcf/Xilinx/1234-tulA]
open_hw_target
set_property PROGRAM.FILE {/home/scouter/bitfiles/eight_links_zs/top.bit} [get_hw_devices xcku115_0]
set_property PROBES.FILE {/home/scouter/bitfiles/eight_links_zs/top.ltx} [get_hw_devices xcku115_0]
set_property FULL_PROBES.FILE {/home/scouter/bitfiles/eight_links_zs/top.ltx} [get_hw_devices xcku115_0]
current_hw_device [get_hw_devices xcku115_0]
refresh_hw_device [lindex [get_hw_devices xcku115_0] 0]

set_property OUTPUT_VALUE 1 [get_hw_probes rst_global -of_objects [get_hw_vios -of_objects [get_hw_devices xcku115_0] -filter {CELL_NAME=~"central_vio"}]]
commit_hw_vio [get_hw_probes {rst_global} -of_objects [get_hw_vios -of_objects [get_hw_devices xcku115_0] -filter {CELL_NAME=~"central_vio"}]]
after 1000
set_property OUTPUT_VALUE 0 [get_hw_probes rst_global -of_objects [get_hw_vios -of_objects [get_hw_devices xcku115_0] -filter {CELL_NAME=~"central_vio"}]]
commit_hw_vio [get_hw_probes {rst_global} -of_objects [get_hw_vios -of_objects [get_hw_devices xcku115_0] -filter {CELL_NAME=~"central_vio"}]]


