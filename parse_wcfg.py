#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0
# Copyright 2024 Frank Zago


# Generate chips.h from upstream wcfg files

import configparser

cf = {}
chips = []

config = configparser.ConfigParser()

for fname in ['chiplist_CH32F.wcfg', 'chiplist_CH32V.wcfg', 'chiplist_CH54x_CH55x.wcfg',
              'chiplist_CH56.wcfg', 'chiplist_CH57x_CH59x.wcfg']:
    config.read(fname)
    del config['WCHCHIP']
    cf |= config._sections.items()

cf = list(cf.items())

for name, c in cf:
    isusbdown = int(c.get('isusbdown', 0))
    if isusbdown == 0:
        continue

    #Note: mcuser appears to be in hex.
    family = 0x10 + int(c["mcuser"], 16)

    flash_size = int(c["maxflashsize"])

    # Fixups for bogus entries

    # CH55x is fam 0x11 but McuSer is 0
    if family == 0x10:
        family = 0x11

    # Apparently incorrect sizes
    if name == "CH567":
        assert flash_size == 1966080
        flash_size = 196608

    if name in (("CH32V208RBT6", "CH32V208CBU6", "CH32V208GBU6", "CH32F208RBT6")):
        assert flash_size == 491520
        flash_size = 131072

    chips.append([name, family, int(c["mcutype"]),
                  flash_size, int(c["maxeepromsize"])])

# Sort the list by family/type
chips = sorted(chips, key=lambda t: (t[1], t[2]))

for chip in chips:
    print("	{")
    print(f'		.name = "{chip[0]}",')
    print(f'		.family = {hex(chip[1])},')
    print(f'		.type = {hex(chip[2])},')
    print(f'		.code_flash_size = {chip[3]},')
    if chip[4] != 0:
        print(f'		.data_flash_size = {chip[4]},')

    family = chip[1]
    if family >= 0x12:
        print('		.mcu_id_len = 8,')
    else:
        print('		.mcu_id_len = 4,')

    if (family == 0x13 and chip[2] == 0x79) or (family == 0x23):
        print('		.clear_cfg_rom_read = true,')

    if family >= 0x14:
        print("		.need_remove_wp = true,")

    if family == 0x12 or family >= 0x14:
        print("		.need_last_write = true,")

    print("	},")
