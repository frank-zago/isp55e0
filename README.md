ISP-55e0 - An ISP flashing tool for the WCH CH55x, CH57x and CH32Fx

This tool is meant to flash the WinChipHead CH55x / CH57x / CH32Fx
series, such as the CH551, CH552, CH554, CH559 or CH579, through USB
on Linux.

When set in ISP mode, the chip creates a 4348:55e0 USB device, hence
the name for that project.

A few similar tools exist for the CH55x series, but all appear
unmaintained and / or are without a proper license:

  * https://github.com/rgwan/librech551
  * https://github.com/NgoHungCuong/vnproch551
  * https://github.com/LoveMHz/vnproch55x   (descendant of vnproch551)

So the wheel has to be re-invented.

There is also https://github.com/MarsTechHAN/ch552tool, which I found
out after starting this project.

A drawback of these devices is that the firmware has to be sent
encrypted to the device. This is just annoying. The encrypting
algorithm is not known to me, but it is rather weak since it's
vulnerable to a replay attacks. However it's likely the encryption key
is tied to a device through its unique ID, as it is the case for the
previous bootloaders. There are plenty of other manufacturers who
don't do that stupid shit.

Tested on:
- a CH551 with a bootloader version 2.3.1
- a CH552 with a bootloader version 2.4.0
- a CH554 with a bootloader version 2.4.0
- a CH559 with a bootloader version 2.4.0
- a CH579 with a bootloader version 2.8.0
- a CH32F103C8T6 with a bootloader version 2.3.1


Usage
-----

Query the device:

  ./isp55e0

Flash some firmware:

  ./isp55e0 -f fw.bin

Verify an existing firmware against a flashed firmware:

  ./isp55e0 -c fw.bin

Note that if the verification fails, all subsequent verifications will
fail. The chip has to be power cycled to fix the issue.


Note on the CH32F103C8T6 BluePill clone
---------------------------------------

That chip has 2 USB ports, and only one is exposed. The USB port used
by the bootloader connects to PB6 (D-) and PB7 (D+), while the
breakout board only show the other USB port, which connects to PA11
(D-) and PA12 (D+).


Caveats
-------

Setting options is not implemented.
The program will make no attempt to recover from an error and will
just exit.
