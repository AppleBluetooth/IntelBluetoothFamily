# IntelBluetoothFamily

Supporting Bluetooth chipset on macOS has never been hard: notable projects like [BrcmPatchRAM](https://github.com/acidanthera/BrcmPatchRAM) and [IntelBluetoothFirmware](https://github.com/OpenIntelWireless/IntelBluetoothFirmware) have embarked on this journey and are my major inspirations. But what makes this driver different? Instead of partially implementing the Bluetooth stack, this project directly utilizes Apple's native Bluetooth stack, IOBluetoothFamily, which is made possible by my reverse-engineering efforts. In addition to its integration with the system, the "injector" trick is no longer necessary - we no longer need to "spoof" the Bluetooth controller as Broadcom. Inheriting directly from the IOBluetoothFamily base classes, vendor-specific transports and host controllers can be created, which also solves certain compatibility problems and makes it easier to extend the macOS Bluetooth stack. 

## Supported Devices
- 0x8087, 0x0025
- 0x8087, 0x0026
- 0x8087, 0x0029
- 0x8087, 0x0032
- 0x8087, 0x0033
- 0x8087, 0x07DA (CSR)
- 0x8087, 0x07DA
- 0x8087, 0x0A2A
- 0x8087, 0x0A2B
- 0x8087, 0x0AA7
- 0x8087, 0x0AAA

## Acknowledgements
- Apple for macOS
- Intel engineers for the [Linux](https://github.com/torvalds/linux/tree/master/drivers/bluetooth) Bluetooth driver
- [IntelBluetoothFirmware](https://github.com/OpenIntelWireless/IntelBluetoothFirmware) for inspirations
- @zxystd and @williambj1 for crucial help and support
- cjiang for reversing IOBluetoothFamily[https://github.com/CharlieJiangXXX/MacKernelSDK] and writing this software 
