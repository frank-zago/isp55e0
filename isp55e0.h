/* Endpoints to use */
#define EP_OUT 0x02
#define EP_IN 0x82

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
	uint8_t _u1[12];
	uint32_t bootloader_version; /* in big endian */
	uint8_t id[7];		/* chip unique ID */
	uint8_t id_checksum;
} __attribute__((__packed__));

struct req_write_config {
	struct req_hdr hdr;
	uint16_t what;		/* bitfield of config types? */
	uint8_t data[12];
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
	uint16_t return_code;
} __attribute__((__packed__));

struct req_erase_flash {
	struct req_hdr hdr;
	uint8_t data[4];
} __attribute__((__packed__));

struct resp_erase_flash {
	struct resp_hdr hdr;
	uint16_t return_code;
} __attribute__((__packed__));

struct req_write_fw {
	struct req_hdr hdr;
	uint32_t offset;	/* ROM offset */
	uint8_t _u1;		/* some checksum? */
	uint8_t data[56];
} __attribute__((__packed__));

struct resp_write_fw {
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
