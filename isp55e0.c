/*
 * ISP-55E0 - an ISP programmer for some WinChipHead MCU families
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

/* Profile of supported chips */
static const struct ch_profile profiles[] = {
	{
		.name = "CH551",
		.family = 0x11,
		.type = 0x51,
		.code_flash_size = 10240,
		.data_flash_size = 128,
		.mcu_id_len = 4,
		.xor_key_id_len = 4,
	},
	{
		.name = "CH552",
		.family = 0x11,
		.type = 0x54,
		.code_flash_size = 14336,
		.data_flash_size = 128,
		.mcu_id_len = 4,
		.xor_key_id_len = 4,
	},
	{
		.name = "CH554",
		.family = 0x11,
		.type = 0x54,
		.code_flash_size = 14336,
		.data_flash_size = 128,
		.mcu_id_len = 4,
		.xor_key_id_len = 4,
	},
	{
		.name = "CH559",
		.family = 0x11,
		.type = 0x54,
		.code_flash_size = 61440,
		.data_flash_size = 1024,
		.mcu_id_len = 4,
		.xor_key_id_len = 4,
	},
	{
		.name = "CH579",
		.family = 0x13,
		.type = 0x79,
		.code_flash_size = 256000,
		.data_flash_size = 2048,
		.mcu_id_len = 7,
		.xor_key_id_len = 8, /* ID plus checksum byte */
	},
	{
		.name = "CH32F103",
		.family = 0x14,
		.type = 0x3f,
		.code_flash_size = 65536,
		.mcu_id_len = 8,
		.xor_key_id_len = 8,
		.need_remove_wp = true,
		.need_last_write = true,
	},
	{}
};

static const struct option long_options[] = {
	{ "code-verify", required_argument, 0, 'c' },
	{ "debug", no_argument, 0,  'd' },
	{ "code-flash", required_argument, 0,  'f' },
	{ "help", no_argument, 0,  'h' },
	{ "data-flash", required_argument, 0,  'k' },
	{ "data-verify", required_argument, 0,  'l' },
	{ "data-dump", required_argument, 0,  'm' },
	{ 0, 0, 0, 0 }
};

static void usage(void)
{
	printf("ISP programmer for some WinChipHead MCUs\n");
	printf("Options:\n");
	printf("  --code-flash, -f    firmware to flash\n");
	printf("  --code-verify, -c   verify existing firwmare\n");
	printf("  --data-flash, -k    data to flash\n");
	printf("  --data-verify, -l   verify existing data\n");
	printf("  --data-dump, -m     dump the data flash to a file\n");
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

static void set_chip_profile(struct device *dev, uint8_t family, uint8_t type)
{
	const struct ch_profile *profile = profiles;

	while (profile->name) {
		if (profile->family == family && profile->type == type) {
			dev->profile = profile;
			dev->fw.max_flash_size = profile->code_flash_size;
			dev->data.max_flash_size = profile->data_flash_size;
			dev->data_dump.max_flash_size = profile->data_flash_size;
			return;
		}

		profile++;
	}

	errx(EXIT_FAILURE, "Device family %02x type %02x is not supported\n",
	     family, type);
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

	if (resp.family == 0)
		errx(EXIT_FAILURE, "Chip is hosed. Reset or power cycle it.");

	set_chip_profile(dev, resp.family, resp.type);
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
	memcpy(dev->id, resp.id, dev->profile->xor_key_id_len);
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

	if (dev->profile->need_remove_wp && req.config_data[0] == 0xff)
		req.config_data[0] = 0xa5;

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
	};
	struct resp_erase_flash resp;
	int length;
	int ret;

	/* Erase length is in KiB blocks, with a minimum of 8KiB */
	length = ((dev->fw.len + 1023) & ~1023) / 1024;
	if (length < 8)
		length = 8;

	req.length = length;

	ret = transfer(dev, &req, sizeof(req), &resp, sizeof(resp));
	if (ret)
		errx(EXIT_FAILURE, "Can't erase the code flash");

	if (resp.return_code != 0x00)
		errx(EXIT_FAILURE, "The device refused to erase the code flash");
}

static void load_file(struct device *dev, struct content *info)
{
	struct stat statbuf;
	int ret;
	int fd;

	fd = open(info->filename, O_RDONLY);
	if (fd == -1)
		err(EXIT_FAILURE, "Can't open the firmware file");

	ret = fstat(fd, &statbuf);
	if (ret == -1)
		err(EXIT_FAILURE, "Can't get firmware file size");

	/* Round up to 8 bytes boundary as upload protocol requires
	 * it. Extra bytes are zeroes. */
	info->len = (statbuf.st_size + 7) & ~7;

	if (info->len > info->max_flash_size)
		errx(EXIT_FAILURE, "Firmware cannot fit in flash");

	info->buf = calloc(1, info->len);
	if (info->buf == NULL)
	    errx(EXIT_FAILURE, "Can't allocate %zd bytes for the firmware",
		 info->len);

	/* TODO: loop until all read, or use mmap instead. */
	ret = read(fd, info->buf, statbuf.st_size);
	if (ret != statbuf.st_size)
		err(EXIT_FAILURE, "Can't read firmware file");

	close(fd);
}

/* Encrypt (or decrypt) some data */
static void encrypt(const struct device *dev, struct content *info)
{
	uint8_t *p;
	int i;

	p = info->buf;
	for (i = 0; i < info->len; i++) {
		*p++ ^= dev->xor_key[i % XOR_KEY_LEN];
	}

	info->encrypted = !info->encrypted;
}

/* Create the local key to encrypt the data to send */
static void create_key(struct device *dev)
{
	uint8_t sum;
	int i;

	sum = 0;
	for (i = 0; i < dev->profile->xor_key_id_len; i++)
		sum += dev->id[i];

	for (i = 0; i < XOR_KEY_LEN; i++)
		dev->xor_key[i] = sum;
	dev->xor_key[7] += dev->profile->type;
}

/* Send the encryption key */
static void send_key(struct device *dev)
{
	static struct req_set_key req = {
		.hdr.command = CMD_SET_KEY,
		.hdr.data_len = 0x1e,
	};
	struct resp_set_key resp;
	int ret;
	uint8_t sum;
	int i;

	sum = 0;
	for (i = 0; i < XOR_KEY_LEN; i++)
		sum += dev->xor_key[i];

	ret = transfer(dev, &req, sizeof(struct req_hdr) + req.hdr.data_len,
		       &resp, sizeof(resp));
	if (ret)
		errx(EXIT_FAILURE, "Can't set the key");

	if (resp.key_checksum != sum)
		errx(EXIT_FAILURE, "The device refused the key");
}

/* read or write code flash, or write data flash */
static int flash_rw(struct device *dev, int cmd, struct content *info,
		    int *offset_out)
{
	struct req_flash_rw req = {
		.hdr.command = cmd,
	};
	struct resp_flash_rw resp;
	int offset;
	int to_send;
	int len;
	int ret;

	/* Send the firmware in 56 bytes chunks */
	offset = 0;
	to_send = info->len;
	while (to_send) {
		req.offset = offset;

		len = sizeof(req.data);
		if (len > to_send)
			len = to_send;

		req.hdr.data_len = len + 5;

		memcpy(&req.data, &info->buf[offset], len);

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

	if (cmd == CMD_WRITE_CODE_FLASH && dev->profile->need_last_write) {
		/* The CH32Fx need a last empty write. */
		req.offset = info->len;
		req.hdr.data_len = 5;

		ret = transfer(dev, &req, sizeof(struct req_hdr) + req.hdr.data_len,
			       &resp, sizeof(resp));
		if (ret)
			errx(EXIT_FAILURE, "Write failure at offset %d", offset);

		if (resp.return_code != 0) {
			*offset_out = offset;
			return resp.return_code;
		}
	}

	return 0;
}

static void write_code_flash(struct device *dev)
{
	int offset;
	int ret;

	ret = flash_rw(dev, CMD_WRITE_CODE_FLASH, &dev->fw, &offset);
	if (ret)
		errx(EXIT_FAILURE, "Write code flash failure at offset %d",
		     offset);
}

static void verify_code_flash(struct device *dev)
{
	int offset;
	int ret;

	ret = flash_rw(dev, CMD_CMP_CODE_FLASH, &dev->fw, &offset);
	if (ret)
		errx(EXIT_FAILURE, "Check code flash failure at offset %d", offset);
}

static void erase_data_flash(struct device *dev)
{
	struct req_erase_data_flash req = {
		.hdr.command = CMD_ERASE_DATA_FLASH,
	};
	struct resp_erase_data_flash resp;
	size_t length;
	int ret;

	/* Erase length is in KiB blocks, with a minimum of 1KiB */
	length = ((dev->profile->data_flash_size + 1023) & ~1023) / 1024;
	if (length < 1)
		length = 1;

	req.len = length;

	ret = transfer(dev, &req, sizeof(req), &resp, sizeof(resp));
	if (ret)
		errx(EXIT_FAILURE, "Can't erase the data flash");

	if (resp.return_code != 0x00)
		errx(EXIT_FAILURE, "The device refused to erase the data flash");
}

static void write_data_flash(struct device *dev)
{
	int ret;
	int offset;

	ret = flash_rw(dev, CMD_WRITE_DATA_FLASH, &dev->data, &offset);
	if (ret)
		errx(EXIT_FAILURE, "Write data flash failure at offset %d",
		     offset);
}

static void read_data_flash(struct device *dev)
{
	struct req_read_data_flash req = {
		.hdr.command = CMD_READ_DATA_FLASH,
	};
	struct resp_read_data_flash resp;
	int to_read;
	int offset;
	int len;
	int ret;

	to_read = dev->data_dump.max_flash_size;

	dev->data_dump.len = to_read;
	dev->data_dump.buf = calloc(1, to_read);
	if (!dev->data_dump.buf)
		errx(EXIT_FAILURE, "Can't allocate %u bytes for the data flash",
		     to_read);

	offset = 0;

	while (to_read) {
		req.offset = offset;

		len = sizeof(resp.data);
		if (len > to_read)
			len = to_read;

		req.len = len;

		ret = transfer(dev, &req, sizeof(req), &resp, sizeof(resp));
		if (ret)
			errx(EXIT_FAILURE, "Data read failure at offset %d", offset);

		if (resp.return_code != 0)
			errx(EXIT_FAILURE, "Data read failure at offset %d", offset);

		memcpy(&dev->data_dump.buf[offset], resp.data, len);

		to_read -= len;
		offset += len;
	}
}

static void verify_data_flash(struct device *dev)
{
	if (dev->data.encrypted) {
		/* The data was just written previously to the flash,
		 * and encrypted. It needs to been decrypted now so
		 * the comparison can happen. */
		encrypt(dev, &dev->data);
	}

	if (memcmp(dev->data.buf, dev->data_dump.buf, dev->data.len) != 0)
		errx(EXIT_FAILURE, "Data flash doesn't match");
}

static void dump_data_flash(struct device *dev)
{
	int fd;
	int ret;

	fd = creat(dev->data_dump.filename, 0600);
	if (fd == -1)
		err(EXIT_FAILURE, "Can't create the file to dump the data flash");

	ret = write(fd, dev->data_dump.buf, dev->data.max_flash_size);
	if (ret == -1)
		err(EXIT_FAILURE, "Can't dump the data flash");

	if (ret != dev->profile->data_flash_size)
		err(EXIT_FAILURE, "Can't dump all the data flash to file");

	close(fd);
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
	bool do_data_flash = false;
	bool do_data_verify = false;
	bool do_data_dump = false;
	int c;
	int i;

	while (1) {
		int option_index = 0;

		c = getopt_long(argc, argv, "c:df:hk:l:m:",
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
			dev.fw.filename = optarg;
			do_code_verify = true;
			break;
		case 'd':
			dev.debug = true;
			break;
		case 'f':
			dev.fw.filename = optarg;
			do_code_flash = true;
			do_code_verify = true; /* always verify after flashing */
			break;
		case 'k':
			dev.data.filename = optarg;
			do_data_flash = true;
			do_data_verify = true;
			break;
		case 'l':
			dev.data.filename = optarg;
			do_data_verify = true;
			break;
		case 'm':
			dev.data_dump.filename = optarg;
			do_data_dump = true;
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
	printf("Found device %s\n", dev.profile->name);

	read_config(&dev);

	printf("Bootloader version %d.%d.%d\n",
	       (dev.bv >> 16) & 0xff, (dev.bv >> 8) & 0xff, dev.bv & 0xff);

	printf("Unique chip ID ");
	for (i = 0; i < dev.profile->mcu_id_len; i++) {
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

	create_key(&dev);

	if (do_code_flash || do_code_verify) {
		load_file(&dev, &dev.fw);
		encrypt(&dev, &dev.fw);
	}

	if (do_data_flash || do_data_verify)
		load_file(&dev, &dev.data);

	if (do_code_flash || do_code_verify || do_data_flash)
		send_key(&dev);

	/* Code flash */

	if (do_code_flash) {
		write_config(&dev);

		erase_code_flash(&dev);
		write_code_flash(&dev);

		printf("Code flashing successful\n");
	}

	if (do_code_verify) {
		verify_code_flash(&dev);

		printf("Firmware is good\n");
	}

	/* Data flash */

	if (do_data_flash) {
		encrypt(&dev, &dev.data);
		erase_data_flash(&dev);
		write_data_flash(&dev);

		printf("Data flashing successful\n");
	}

	if (do_data_verify || do_data_dump)
		read_data_flash(&dev);

	if (do_data_verify) {
		verify_data_flash(&dev);

		printf("Data flash is good\n");
	}

	if (do_data_dump) {
		dump_data_flash(&dev);

		printf("Dumped data flash to file\n");
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
