# CoCo Device

Device module for CoCo. In contrast to classical operating systems, a CoCo device is based on transfer buffers
instead of read() and write() methods. Buffers are used for communication with peripherals such as SPI, I2C and USB.
Number and size of buffers have to be allocated upfront according to the application, but then yield a very
high performance as no additional copying is necessary. Drivers can directly transfer to/from the buffers using DMA.
Buffers support a header that can be used to store out-of-band data such as the address inside an SPI flash.
When reading from SPI, the SPI driver will first send the header containing the address and then read the data into
the buffer behind the header.

## Import
Add coco-device/\<version> to your conanfile where version corresponds to the git tags

## Features
* Device state with awaitable notification for state changes
* Buffer state with awaitable notification for state changes
* Optional buffer header for register index or memory address
* Awaitable completion of transfer
* Support for cancellation

## Supported Platforms
All platforms, see README.md of coco base library
