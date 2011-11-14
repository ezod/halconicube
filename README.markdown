# HALCON Acquisition Interface for NET iCube (Linux 32-bit)


## Overview

This is an acquisition interface for [MVTec HALCON] [halcon] for the [NET
iCube] [icube] camera series, for 32-bit Linux.


## Compilation

The supplied makefile relies on the `HALCONROOT` and `HALCONARCH` environment
variables, which are normally used by HALCON in Linux. It is recommended to add
`libNETUSBCAM.so.*` provided by the NET iCube Linux driver to the directory
`$(HALCONROOT)/lib/$(HALCONARCH)`. The compiled `hAcqICube.so` should then also
be installed there, which will make the `ICube` acquisition interface available
in HALCON.


## iCube SDK License Information

Excerpted from the iCube SDK API Manual, version 2.0.0.7 (February 2010), p. 5:

> Evaluation version of the NET GmbH Camera API (iCube SDK Library) is only
> compliant with cameras manufactured by NET GmbH and my not be operable with
> other cameras.

> User may purchase the license by contacting our sales department or your local
> distributor for unlimited use of the API and it’s function. Please refer to
> the standard EULA documents for details concerned with API License.

> NET GmbH Camera API (iCube SDK Library) only supports NET GmbH hardware and
> strictly forbidden to use or build Application for cameras or hardware from
> other venders with this API. The EVALUATION VERSION SOFTWARE is provided to
> you "AS IS" without warranty. The entire risk of the quality and performance
> of the software is with its users. We would appreciate feedback bug report of
> any kind, however, we can not guarantee satisfactory response.

> By installing, copying or otherwise using the SOFTWARE, you agree to be bound
> by the terms of the End User License Agreements (EULA). The SOFTWARE includes
> NET GmbH and NET GmbH suppliers’ intellectual property.

> Please read NET GmbH and NET GmbH suppliers’ EULA before installing the
> SOFTWARE. If you do not accept the terms of the license agreements, please do
> not install copy or use this SOFTWARE.


[halcon]: http://www.mvtec.com/halcon
[icube]: http://www.net-gmbh.com/en/usb2.0.html
