# scouting-es
Simple set of classes and scripts to collect data from scouting prototype and save to files.

## Build

~~**Compile with**~~ 

~~g++ -std=c++0x -g -ltbb -lboost_thread -lcurl \*.cc~~

**Use supplied Makefile:**

```
$ cd src
$ make
```

## Setup (at least for the KCU1500-based systems)

On the `scoutdaq` machine:
1. Start the hw_server with: `/opt/Xilinx/Vivado_Lab/2018.3/bin/unwrapped/lnx64.o/hw_server -d -I20 -s TCP:127.0.0.1:3121` (to be moved into a service)
  * You may need to install the Xilinx cable drivers from `/opt/Xilinx/Vivado_Lab/2018.3/data/xicom/cable_drivers/lin64/install_script/install_drivers`
1. Place the `scouting-es` folder (i.e. the one containing this README) into `/home/scouter`
1. Move `systemd/disableFatalErrorReporting.service`, `systemd/runSCdaq.service`, `systemd/scoutboardResetServer.service` to `/usr/lib/systemd/system/`
1. Call `sudo systemctl daemon-reload` to make systemd aware of the new unit files
1. Call `sudo systemctl enable disableFatalErrorReporting.service runSCdaq.service scoutboardResetServer.service` to run the services at startup
1. Create a `scouter` group (if it doesn't exist already) and add any user who needs access to the driver to it
1. Copy `udev/99-xdma-permissions.rules` to `/etc/udev/rules.d/99-xdma-permissions.rules`
1. Make the driver with `make && sudo make install` in `/home/scouter/2018-10_Xilinx_DMA/wz_driver/driver` (the driver needs to be obtained from elsewhere, e.g. `scoutdaq-s1d12-39-01`)
   * You may need to also install the appropriate version of `kernel-devel` to compile
1. Copy `modules/kcu1500_xdma.conf` to `/etc/modules-load.d/kcu1500_xdma.conf`
1. Set the configuration in `src/scdaq.conf` according to the machine setup (see below for an example)

On the `scoutBU` machine:
1. Place the `scouting-es` folder (i.e. the one containing this README) into `/home/scouter`
1. Copy `systemd/runFileMover.service` to `/usr/lib/systemd/system/runFileMover.service`

## Run

1. Start the file mover on `scoutBU`: `sudo systemctl start runFileMover.service`
1. Start the machinery on `scoutdaq`: `sudo systemctl start scoutboardResetServer.service runSCdaq.service`

## Configuration

#### example conf in scdaq.conf:

```
input_file:/dev/shm/testdata.bin  
output_filename_base:/tmp/scdaq  
max_file_size:1073741824  
threads:8  
input_buffers:20  
blocks_buffer:1000  
port:8000  
elastic_url:no   
pt_cut:7  
```

See `src/scdaq.conf` for an up-to-date reasonable configuration.
