/* Endpoints to use */
#define EP_OUT 0x02
#define EP_IN 0x82

#define XOR_KEY_LEN 8

/* Profile of a specific CH device */
struct ch_profile {
	const char *name;

	uint8_t type;		/* CH554 -> 0x54, ... */
	uint8_t family;		/* CH55x-> 0x11, CH57x -> 0x13, ...*/

	int mcu_id_len;		/* Number of byte in the unique ID */
	int xor_key_id_len;	/* Number of ID bytes to use for encryption key */

	int code_flash_size;
	int data_flash_size;

	bool need_remove_wp;	/* remove CH32 write protect */
	bool need_last_write;	/* chip needs an empty write */
};

/* Content of either a file or one of the flash section */
struct content {
	char *filename;
	bool encrypted;	/* whether the data in buf has been encrypted */
	size_t len;
	size_t max_flash_size;
	uint8_t *buf;
};

/* Current device */
struct device {
	const struct ch_profile *profile;
	bool debug;
	struct content fw;
	struct content data;	/* read data from */
	struct content data_dump; /* write the data flash into */
	libusb_device_handle *usb_h;
	uint32_t bv;		/* bootloader version */
	uint8_t id[8];
	uint8_t config_data[12];
	uint8_t xor_key[XOR_KEY_LEN];
	bool wait_reboot_resp;	/* wait for reboot command response */
};

/* Enough to erase the flash. */
#define USB_TIMEOUT 5000

#define CMD_CHIP_TYPE        0xa1
#define CMD_REBOOT           0xa2
#define CMD_SET_KEY          0xa3
#define CMD_ERASE_CODE_FLASH 0xa4
#define CMD_WRITE_CODE_FLASH 0xa5
#define CMD_CMP_CODE_FLASH   0xa6
#define CMD_READ_CONFIG      0xa7
#define CMD_WRITE_CONFIG     0xa8
#define CMD_ERASE_DATA_FLASH 0xa9
#define CMD_WRITE_DATA_FLASH 0xaa
#define CMD_READ_DATA_FLASH  0xab

struct req_hdr {
	uint8_t command;
	uint16_t data_len;	/* Number of bytes after the header */
} __attribute__((__packed__));

struct resp_hdr {
	uint8_t command;
	uint8_t _u0;
	uint8_t data_len;	/* Number of bytes after the header */
	uint8_t _u1;		/* possibly part of the length */
} __attribute__((__packed__));

struct req_get_chip_type {
	struct req_hdr hdr;
	uint8_t type;		/* CH554 -> 0x54, ... */
	uint8_t family;		/* CH55x-> 0x11, CH57x -> 0x13, ...*/
	uint8_t string[16];	/* "MCU ISP & WCH.CN" */
} __attribute__((__packed__));

struct resp_chip_type {
	struct resp_hdr hdr;
	uint8_t type;		/* CH554 -> 0x54, ... */
	uint8_t family;		/* CH55x-> 0x11, CH57x -> 0x13, ...*/
} __attribute__((__packed__));

struct req_read_config {
	struct req_hdr hdr;
	uint16_t what;		/* bitfield of config types? */
} __attribute__((__packed__));

struct resp_read_config {
	struct resp_hdr hdr;
	uint16_t what;		/* same as command */
	uint8_t config_data[12];	/* various configuration bits */
	uint32_t bootloader_version; /* in big endian */
	uint8_t id[7];		/* chip unique ID */
	uint8_t id_checksum;
} __attribute__((__packed__));

struct req_write_config {
	struct req_hdr hdr;
	uint16_t what;		/* bitfield of config types? */
	uint8_t config_data[12];
} __attribute__((__packed__));

struct resp_write_config {
	struct resp_hdr hdr;
	uint16_t return_code;
} __attribute__((__packed__));

struct req_set_key {
	struct req_hdr hdr;
	uint8_t data[64];	/* obfuscated crap */
} __attribute__((__packed__));

struct resp_set_key {
	struct resp_hdr hdr;
	uint16_t key_checksum;
} __attribute__((__packed__));

struct req_erase_flash {
	struct req_hdr hdr;
	uint16_t length;	/* size in KiB, possibly just 1 byte to code it. */
	uint16_t _u1;
} __attribute__((__packed__));

struct resp_erase_flash {
	struct resp_hdr hdr;
	uint16_t return_code;
} __attribute__((__packed__));

struct req_flash_rw {
	struct req_hdr hdr;
	uint32_t offset;	/* ROM offset */
	uint8_t _u1;		/* some checksum? */
	uint8_t data[56];
} __attribute__((__packed__));

struct resp_flash_rw {
	struct resp_hdr hdr;
	uint16_t return_code;
} __attribute__((__packed__));

struct req_reboot {
	struct req_hdr hdr;
	uint8_t option;
} __attribute__((__packed__));

struct resp_reboot {
	struct resp_hdr hdr;
	uint16_t return_code;
} __attribute__((__packed__));

struct req_erase_data_flash {
	struct req_hdr hdr;
	uint32_t _u1;
	uint8_t len;		/* length in KB? */
} __attribute__((__packed__));

struct resp_erase_data_flash {
	struct resp_hdr hdr;
	uint16_t return_code;
} __attribute__((__packed__));

struct req_read_data_flash {
	struct req_hdr hdr;
	uint32_t offset;	/* Data offset */
	uint16_t len;
} __attribute__((__packed__));

struct resp_read_data_flash {
	struct resp_hdr hdr;
	uint16_t return_code;
	uint8_t data[58];
} __attribute__((__packed__));
