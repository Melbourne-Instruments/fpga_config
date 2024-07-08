# FPGA Config #

FPGA Config app for the Raspberry Pi (64-bit).

### Building the FPGA Config app ###

The FPGA Config app uses CMake as its build system. A generate script is also provided for convenient setup. Simply running ./generate with no arguments in the root of FPGA Config will setup a build folder containing a Release configuration and a Debug configuration. CMake arguments can be passed through the generate script using the --cmake-args flag. Those arguments will then be added to both configurations.

To configure and build the app, run from the command line:

$ ./generate -b

To cross compile for the Raspberry Pi, the ELK SDK *must* be used. It can be downloaded from here:

https://github.com/elk-audio/elkpi-sdk

It is recommended to install the SDK in the /opt folder on your Ubuntu PC.

Once this has been done, source the environment script to set-up the build environment, for example:

$ source /opt/elk/1.0/environment-setup-aarch64-elk-linux

---
Copyright 2021-2024 Melbourne Instruments, Australia.
