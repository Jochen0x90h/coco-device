# CoCo Buffer

Transfer buffer module for CoCo. Transfer buffers are used for communication with
peripherals such as SPI, I2C and USB. Number and size of transfer buffers have to
be allocated according to the application, but then yield a very high performance
as no additional copying is necessary. Drivers can directly transfer to/from the
transfer buffers using DMA.

## Import
Add coco-buffer/\<version> to your conanfile where version corresponds to the git tags

## Features
* Wait for completion of transfer
* Support for cancellation
* First bytes or entire buffer can be a command for SPI and I2C peripherals such as memories or displays

## Supported Platforms
All platforms, see README.md of coco base library
