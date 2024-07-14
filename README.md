ISP-55e0 - An ISP flashing tool for the WCH CH55x, CH57x and CH32Fx
===================================================================

This tool is meant to flash the WinChipHead CH55x / CH57x / CH32Fx
series, such as the CH551, CH552, CH554, CH559 or CH579, through USB
or serial port on Linux.

When set in ISP mode, the chip creates a 4348:55e0 USB device, hence
the name for that project.

A few similar tools exist for the CH55x series, but all appear
unmaintained and / or are without a proper license:

  - https://github.com/rgwan/librech551
  - https://github.com/NgoHungCuong/vnproch551
  - https://github.com/LoveMHz/vnproch55x     (descendant of vnproch551)
  - https://github.com/MarsTechHAN/ch552tool  (in python)
  - https://github.com/juliuswwj/wchprog      (in python)
  - https://github.com/SIGINT112/wchprog      (fork of wchprog, w/ serial support)

So the wheel has to be re-invented.

See also `wchisp` (https://github.com/ch32-rs/wchisp) which is a new
supported tool, written in rust, and with a license.

isp55e0 has been tested on:

|Chip        |Bootloader|USB|Serial|
|------------|----------|---|------|
|CH549       |2.4.0     |+  |      |
|CH551       |2.3.1     |+  |      |
|CH552       |2.3.1     |+  |      |
|CH552       |2.4.0     |+  |      |
|CH554       |2.4.0     |+  |      |
|CH559       |2.4.0     |+  |      |
|CH573       |2.8.0     |+  |      |
|CH579       |2.8.0     |+  |      |
|CH582       |2.4.0     |+  |+     |
|CH583       |2.4.0     |+  |      |
|CH32F103C8T6|2.3.1     |+  |      |
|CH32F103C8T6|2.5.0     |+  |      |
|CH32V103C8T6|2.6.0     |+  |      |
|CH32V203C8T6|2.7.0     |+  |      |
|CH32V203G8R6|2.6.0     |+  |      |
|CH32V307VCT6|2.9.0     |+  |      |


Build
-----

It needs the libusb development package. On Debian / Ubuntu, install
the "libusb-1.0-0-dev" package. In rpm world, it is usually called
"libusb1-devel".

Type:

>  make


UDEV configuration
------------------

By default, a linux system will only allow the root user to program
these chips. If you'd like to program these chips as a regular user, a
udev rule must be added. As root, issue the following command:

    cp 99-wch-isp.rules /etc/udev/rules.d/99-wch-isp.rules

Edit this file as desired.


Usage
-----

Help:
```
  ./isp55e0 --help

  ISP programmer for some WinChipHead MCUs
  Options:
    --port, -p          use serial port instead of usb
    --code-flash, -f    firmware to flash
    --code-verify, -c   verify existing firwmare
    --data-flash, -k    data to flash
    --data-verify, -l   verify existing data
    --data-dump, -m     dump the data flash to a file
    --debug, -d         turn debug traces on
    --help, -h          this help
```

Query the device:

>  ./isp55e0

Flash some firmware, which will also read back to verify it:

>  ./isp55e0 -f fw.bin

Verify an existing firmware against a flashed firmware:

>  ./isp55e0 -c fw.bin

Note that if the verification fails, all subsequent verifications will
fail. The chip has to be power cycled to fix the issue.


On flashing iHex files
----------------------

iHex is not supported, but objcopy can be used to convert an iHex file
to a regular raw file:

>  objcopy -I ihex -O binary xxx.hex xxx.bin


Note on the CH32F103C8T6 BluePill clone
---------------------------------------

The CH32F103 chip has 2 USB ports. Some breakout boards have 1 USB
port, while some have both. The USB port used by the bootloader is
connected to PB6 (D-) and PB7 (D+), while the regular port is
connected to PA11 (D-) and PA12 (D+). When only one port is present,
some soldering will be needed to access the port. When the 2 ports are
present, the bottom port is used by the bootloader and the top port is
the regular one, compatible with the STM32F103 chip.


Note on the firmware encryption
-------------------------------

A drawback of these devices is that the firmware has to be sent
encrypted to the device. The host creates the key, which is sent
garbled and hidden amongst garbage bytes to the bootloader. The
bootloader extracts the key from predetermined offsets in the packet
and will use it later to decrypt the data before flashing or
comparing. As it's a simple fixed size xor key, it's rather easy to
figure it out, but this is just plain stupid. Since the last byte of
the key is the sum of the first byte plus the chip type, it is not
possible to create a fake key full of zeroes.

There are plenty of other manufacturers who don't do that stupid shit.

For some reason, the data flash is different. Writing to it has to be
encrypted, but reading from it is in clear.


Caveats
-------

Setting options is not implemented.
The program will make no attempt to recover from an error and will
just exit.
