# IntelBluetoothFamily

Supporting Bluetooth chipsets on macOS has never been hard: notable projects like [BrcmPatchRAM](https://github.com/acidanthera/BrcmPatchRAM) and [IntelBluetoothFirmware](https://github.com/OpenIntelWireless/IntelBluetoothFirmware) have embarked on this journey and are my major inspirations. But what makes this driver different? Instead of partially implementing the Bluetooth stack, this project directly utilizes Apple's native Bluetooth stack, IOBluetoothFamily, which is made possible by my reverse-engineering efforts. Apart from its integration with the system, the "injector" trick is no longer necessary - we don't need to "spoof" the Bluetooth controller as Broadcom. Inheriting directly from the IOBluetoothFamily base classes instead, vendor-specific transports and host controllers can be created, which solves certain compatibility problems and makes it easier to extend the macOS Bluetooth stack in addition.

### DON'T USE ON MONTEREY!!!

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

## Usage
1. Download OpenFirmwareManager, IOBluetoothFixup, and IntelBluetoothFamily from the AppleBluetooth organization.
2. Drag the transports out of the PlugIns folder in IntelBluetoothFamily. This has to be done for now as the name is too long.
3. Place the kexts in the EFI folder and add them to the config.plist. Make sure the order is as follows.
4. Remove all other Bluetooth kexts such as IntelBluetoothFirmware.
5. Reboot and enjoy!
<details>
  <summary>Load Order</summary>

  ```xml
    <dict>
      <key>Arch</key>
      <string>Any</string>
      <key>BundlePath</key>
      <string>OpenFirmwareManager.kext</string>
      <key>Comment</key>
      <string></string>
      <key>Enabled</key>
      <true/>
      <key>ExecutablePath</key>
      <string>Contents/MacOS/OpenFirmwareManager</string>
      <key>MaxKernel</key>
      <string></string>
      <key>MinKernel</key>
      <string></string>
      <key>PlistPath</key>
      <string>Contents/Info.plist</string>
    </dict>
    <dict>
      <key>Arch</key>
      <string>Any</string>
      <key>BundlePath</key>
      <string>IOBluetoothFixup.kext</string>
      <key>Comment</key>
      <string></string>
      <key>Enabled</key>
      <true/>
      <key>ExecutablePath</key>
      <string>Contents/MacOS/IOBluetoothFixup</string>
      <key>MaxKernel</key>
      <string></string>
      <key>MinKernel</key>
      <string></string>
      <key>PlistPath</key>
      <string>Contents/Info.plist</string>
    </dict>
    <dict>
      <key>Arch</key>
      <string>Any</string>
      <key>BundlePath</key>
      <string>IntelBluetoothFamily.kext</string>
      <key>Comment</key>
      <string></string>
      <key>Enabled</key>
      <true/>
      <key>ExecutablePath</key>
      <string>Contents/MacOS/IntelBluetoothFamily</string>
      <key>MaxKernel</key>
      <string></string>
      <key>MinKernel</key>
      <string></string>
      <key>PlistPath</key>
      <string>Contents/Info.plist</string>
    </dict>
    <dict>
      <key>Arch</key>
      <string>Any</string>
      <key>BundlePath</key>
      <string>IntelBluetoothHostControllerUSBTransport.kext</string>
      <key>Comment</key>
      <string></string>
      <key>Enabled</key>
      <true/>
      <key>ExecutablePath</key>
      <string>Contents/MacOS/IntelBluetoothHostControllerUSBTransport</string>
      <key>MaxKernel</key>
      <string></string>
      <key>MinKernel</key>
      <string></string>
      <key>PlistPath</key>
      <string>Contents/Info.plist</string>
    </dict>
    <dict>
      <key>Arch</key>
      <string>Any</string>
      <key>BundlePath</key>
      <string>IntelGenXBluetoothHostControllerUSBTransport.kext</string>
      <key>Comment</key>
      <string></string>
      <key>Enabled</key>
      <true/>
      <key>ExecutablePath</key>
      <string>Contents/MacOS/IntelGenXBluetoothHostControllerUSBTransport</string>
      <key>MaxKernel</key>
      <string></string>
      <key>MinKernel</key>
      <string></string>
      <key>PlistPath</key>
      <string>Contents/Info.plist</string>
    </dict>
  ```
</details>

## Acknowledgements
- Apple for macOS
- Intel engineers for the [Linux](https://github.com/torvalds/linux/tree/master/drivers/bluetooth) Bluetooth driver
- [IntelBluetoothFirmware](https://github.com/OpenIntelWireless/IntelBluetoothFirmware) for inspirations
- @zxystd and @williambj1 for crucial help and support
- cjiang for reversing [IOBluetoothFamily](https://github.com/CharlieJiangXXX/MacKernelSDK) and writing this software 
