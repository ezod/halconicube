# HALCON Acquisition Interface for NET iCube (Linux 32-bit)


## Overview

This is an acquisition interface for [MVTec HALCON] [halcon] for the [NET
iCube] [icube] camera series, for 32-bit Linux.


## Compilation and Installation

1. Ensure that the `HALCONROOT` and `HALCONARCH` environment variables are set
   per the HALCON installation instructions.
2. Copy `libNETUSBCAM.so.0.0.0` to `$(HALCONROOT)/lib/$(HALCONARCH)`.
3. Create symlinks `libNETUSBCAM.so.0` and `libNETUSBCAM.so`.
4. Copy `NETUSBCAM_API.h` to `$(HALCONROOT)/include`.
5. Run `make` in the source directory.
6. Copy or symlink `hAcqICube.so` to `$(HALCONROOT)/lib/$(HALCONARCH)`.


[halcon]: http://www.mvtec.com/halcon
[icube]: http://www.net-gmbh.com/en/usb2.0.html
