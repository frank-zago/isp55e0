/*
 * ISP-55E0 - a basic ISP programmer for the WinChipHead CH579
 * Copyright 2021 Frank Zago
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <err.h>
#include <libusb-1.0/libusb.h>

#include "isp55e0.h"

struct device {
	bool debug;
	char *fw_filename;
	size_t fw_len;
	uint8_t *fw_data;
	bool fw_encrypted;	/* whether the firmware was already encrypted */
	libusb_device_handle *usb_h;
	uint8_t type;		/* CH554 -> 0x54, ... */
	uint8_t family;		/* CH55x-> 0x11, CH57x -> 0x13, ...*/
	uint32_t bv;		/* bootloader version */
	int mcu_id_len;		/* Number of byte in the unique ID */
	int xor_key_id_len;	/* Number of ID bytes to use for encryption key */
	uint8_t id[8];
	uint8_t config_data[12];
	bool wait_reboot_resp;	/* wait for reboot command response */
};

#define XOR_KEY_LEN 8

static const struct option long_options[] = {
	{ "code-verify", required_argument, 0, 'c' },
	{ "debug", no_argument, 0,  'd' },
	{ "code-flash", required_argument, 0,  'f' },
	{ "help", no_argument, 0,  'h' },
	{ 0, 0, 0, 0 }
};

static void usage(void)
{
	printf("ISP programmer for WinChipHead CH579\n");
	printf("Options:\n");
	printf("  --code-flash, -f    firmware to flash\n");
	printf("  --code-verify, -c   verify existing firwmare\n");
	printf("  --debug, -d         turn debug traces on\n");
	printf("  --help, -h          this help\n");
}

static void hexdump(const char *name, const void *data, int len)
{
	const uint8_t *p = data;
	int i;

	printf("Dump - %s\n", name);
	for (i = 0; i < len; i++) {
		if (i && (i % 16) == 0)
			printf("\n");

		printf("%02x ", *p++);
	}
	printf("\n");
}

/* Open and claim the USB device */
static void open_usb_device(struct device *dev)
{
	int ret;

	ret = libusb_init(NULL);
	if (ret)
		errx(EXIT_FAILURE, "Can't initialize USB");

	dev->usb_h = libusb_open_device_with_vid_pid(NULL, 0x4348, 0x55e0);
	if (dev->usb_h == NULL)
		errx(EXIT_FAILURE, "No CH5xx devices found in ISP mode");

	ret = libusb_set_auto_detach_kernel_driver(dev->usb_h, 1);
	if (ret)
		errx(EXIT_FAILURE, "Can't detach the device from the kernel");

	ret = libusb_claim_interface(dev->usb_h, 0);
	if (ret)
		errx(EXIT_FAILURE, "Can't claim the USB device\n");
}

/* Send a request, get a reply */
static int transfer(struct device *dev, void *req, int req_len,
		    void *resp, int resp_len)
{
	int len;
	int ret;

	ret = libusb_bulk_transfer(dev->usb_h, EP_OUT, req, req_len,
				   &len, USB_TIMEOUT);
	if (ret)
		return -EIO;

	if (dev->debug)
		hexdump("request", req, len);

	ret = libusb_bulk_transfer(dev->usb_h, EP_IN, resp, resp_len,
				   &len, USB_TIMEOUT);
	if (ret)
		return -EIO;

	if (dev->debug)
		hexdump("response", resp, len);

	return 0;
}

static void read_chip_type(struct device *dev)
{
	struct req_get_chip_type req = {
		.hdr.command = CMD_CHIP_TYPE,
		.hdr.data_len = sizeof(req) - sizeof(req.hdr),
		.string = "MCU ISP & WCH.CN",
	};
	struct resp_chip_type resp;
	int ret;

	ret = transfer(dev, &req, sizeof(req), &resp, sizeof(resp));
	if (ret)
		errx(EXIT_FAILURE, "Can't get the device type");

	dev->type = resp.type;
	dev->family = resp.family;
}

static void read_config(struct device *dev)
{
	struct req_read_config req = {
		.hdr.command = CMD_READ_CONFIG,
		.hdr.data_len = sizeof(req) - sizeof(req.hdr),
		.what = 0x1f,
	};
	struct resp_read_config resp;
	int ret;

	ret = transfer(dev, &req, sizeof(req), &resp, sizeof(resp));
	if (ret)
		errx(EXIT_FAILURE, "Can't get the device configuration");

	dev->bv = be32toh(resp.bootloader_version);
	memcpy(dev->id, resp.id, dev->xor_key_id_len);
	memcpy(dev->config_data, resp.config_data, sizeof(dev->config_data));
}

/* Write some configuration. Hardcoded for now. */
static void write_config(struct device *dev)
{
	struct req_write_config req = {
		.hdr.command = CMD_WRITE_CONFIG,
		.hdr.data_len = sizeof(req) - sizeof(req.hdr),
		.what = 0x07,
	};
	struct resp_write_config resp;
	int ret;

	memcpy(req.config_data, dev->config_data, sizeof(req.config_data));

	ret = transfer(dev, &req, sizeof(req), &resp, sizeof(resp));
	if (ret)
		errx(EXIT_FAILURE, "Can't write the new configuration");
}

/* Erase the flash */
static void erase_code_flash(struct device *dev)
{
	struct req_erase_flash req = {
		.hdr.command = CMD_ERASE_CODE_FLASH,
		.hdr.data_len = sizeof(req) - sizeof(req.hdr),
		.data = { 0x08, 0x00, 0x00, 0x00 },
	};
	struct resp_erase_flash resp;
	int ret;

	ret = transfer(dev, &req, sizeof(req), &resp, sizeof(resp));
	if (ret)
		errx(EXIT_FAILURE, "Can't erase the code flash");

	if (resp.return_code != 0x00)
		errx(EXIT_FAILURE, "The device refused to erase the code flash");
}

static void load_firmware(struct device *dev)
{
	struct stat statbuf;
	int ret;
	int fd;

	fd = open(dev->fw_filename, O_RDONLY);
	if (fd == -1)
		err(EXIT_FAILURE, "Can't open the firmware file");

	ret = fstat(fd, &statbuf);
	if (ret == -1)
		err(EXIT_FAILURE, "Can't get firmware file size");

	dev->fw_len = statbuf.st_size;

	dev->fw_data = malloc(dev->fw_len);
	if (dev->fw_data == NULL)
	    errx(EXIT_FAILURE, "Can't allocate %zd bytes for the firmware",
		 dev->fw_len);

	/* TODO: loop until all read, or use mmap instead. */
	ret = read(fd, dev->fw_data, dev->fw_len);
	if (ret != dev->fw_len)
		err(EXIT_FAILURE, "Can't read firmware file");

	close(fd);
}

/* Send the encryption key and encrypt the firmware */
static void set_key(struct device *dev)
{
	static struct req_set_key req = {
		.hdr.command = CMD_SET_KEY,
		.hdr.data_len = 0x1e,
	};
	struct resp_set_key resp;
	uint8_t xor_key[XOR_KEY_LEN] = {};
	uint8_t *p;
	int ret;
	uint8_t sum;
	int i;

	sum = 0;
	for (i = 0; i < dev->xor_key_id_len; i++)
		sum += dev->id[i];

	for (i = 0; i < XOR_KEY_LEN; ++i)
		xor_key[i] = sum;
	xor_key[7] += dev->type;

	sum = 0;
	for (i = 0; i < XOR_KEY_LEN; ++i)
		sum += xor_key[i];

	ret = transfer(dev, &req, sizeof(struct req_hdr) + req.hdr.data_len,
		       &resp, sizeof(resp));
	if (ret)
		errx(EXIT_FAILURE, "Can't set the key");

	if (resp.key_checksum != sum)
		errx(EXIT_FAILURE, "The device refused the key");

	/* Encrypt the firmware */
	if (!dev->fw_encrypted) {
		p = dev->fw_data;
		for (i = 0; i < dev->fw_len; i++) {
			*p++ ^= xor_key[i % XOR_KEY_LEN];
		}

		dev->fw_encrypted = true;
	}
}

static int code_flash_access(struct device *dev, int cmd, int *offset_out)
{
	struct req_write_fw req = {
		.hdr.command = cmd,
	};
	struct resp_write_fw resp;
	int offset;
	int to_send;
	int len;
	int ret;

	/* Send the firmware in 56 bytes chunks */
	offset = 0;
	to_send = dev->fw_len;
	while (to_send) {
		req.offset = offset;

		len = sizeof(req.data);
		if (len > to_send)
			len = to_send;

		req.hdr.data_len = len + 5;

		memcpy(&req.data, &dev->fw_data[offset], len);

		ret = transfer(dev, &req, sizeof(struct req_hdr) + req.hdr.data_len,
			       &resp, sizeof(resp));
		if (ret)
			errx(EXIT_FAILURE, "Write failure at offset %d", offset);

		if (resp.return_code != 0) {
			*offset_out = offset;
			return resp.return_code;
		}

		to_send -= len;
		offset += len;
	}

	return 0;
}

static void code_flash(struct device *dev)
{
	int offset;
	int ret;

	ret = code_flash_access(dev, CMD_WRITE_CODE_FLASH, &offset);
	if (ret)
		errx(EXIT_FAILURE, "Write code flash failure at offset %d",
		     offset);
}

static void verify_code_flash(struct device *dev)
{
	int offset;
	int ret;

	ret = code_flash_access(dev, CMD_CMP_CODE_FLASH, &offset);
	if (ret)
		errx(EXIT_FAILURE, "Check code flash failure at offset %d", offset);
}

/* Reboot the device */
static void reboot_device(struct device *dev)
{
	struct req_reboot req = {
		.hdr.command = CMD_REBOOT,
		.hdr.data_len = sizeof(req) - sizeof(req.hdr),
		.option = 0x01,
	};
	struct resp_reboot resp;
	int ret;

	ret = transfer(dev, &req, sizeof(req), &resp, sizeof(resp));

	/* 2.4.0 bootloaders do not respond. 2.8.0 does. */
	if (!dev->wait_reboot_resp)
		return;

	if (ret)
		errx(EXIT_FAILURE, "Can't reboot the device");

	if (resp.return_code != 0x00)
		errx(EXIT_FAILURE, "The device refused to reboot");
}

int main(int argc, char *argv[])
{
	struct device dev = {};
	bool do_code_flash = false;
	bool do_code_verify = false;
	int c;
	int i;

	while (1) {
		int option_index = 0;

		c = getopt_long(argc, argv, "c:df:h",
				long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case 0:
			printf("option %s", long_options[option_index].name);
			if (optarg)
				printf(" with arg %s", optarg);
			printf("\n");
			break;
		case 'c':
			dev.fw_filename = optarg;
			do_code_verify = true;
			break;
		case 'd':
			dev.debug = true;
			break;
		case 'f':
			dev.fw_filename = optarg;
			do_code_flash = true;
			do_code_verify = true; /* always verify after flashing */
			break;
		case 'h':
			usage();
			return EXIT_SUCCESS;
		default:
		       return EXIT_FAILURE;
		}
	}

	if (optind < argc)
		errx(EXIT_FAILURE, "Extra argument: %s", argv[optind]);

	open_usb_device(&dev);
	read_chip_type(&dev);

	printf("Found device CH5%02x\n", dev.type);

	switch (dev.family) {
	case 0x11:
		/* CH55x */
		dev.mcu_id_len = 4;
		dev.xor_key_id_len = 4;
		break;

	case 0x13:
		/* CH57x */
		dev.mcu_id_len = 7;
		dev.xor_key_id_len = 8; /* ID plus checksum byte */
		break;

	default:
		printf("Device family %02x is not supported\n", dev.family);
		return 1;
	}

	read_config(&dev);

	printf("Bootloader version %d.%d.%d\n",
	       (dev.bv >> 16) & 0xff, (dev.bv >> 8) & 0xff, dev.bv & 0xff);

	printf("Unique chip ID ");
	for (i = 0; i < dev.mcu_id_len; i++) {
		if (i > 0)
			printf("-");
		printf("%02x", dev.id[i]);
	}
	printf("\n");

	/* check bootloader version */
	switch (dev.bv) {
	case 0x020301:
	case 0x020400:
		dev.wait_reboot_resp = false;
		break;

	case 0x020800:
		dev.wait_reboot_resp = true;
		break;

	default:
		errx(EXIT_FAILURE, "This bootloader version is not supported");
	}

	if (dev.fw_filename)
		load_firmware(&dev);

	if (do_code_flash) {
		write_config(&dev);
		set_key(&dev);

		erase_code_flash(&dev);
		code_flash(&dev);

		printf("Flashing successful\n");
	}

	if (do_code_verify) {
		set_key(&dev);

		verify_code_flash(&dev);

		printf("Firmware is good\n");
	}

	if (do_code_flash)
		reboot_device(&dev);

	return 0;
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "linux"
 * indent-tabs-mode: t
 * tab-width: 8
 * End:
 */
