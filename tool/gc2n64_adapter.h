#ifndef _gc2n64_adapter_h__
#define _gc2n64_adapter_h__

#include "gcn64.h"

struct gc2n64_adapter_info_app {
	unsigned char default_mapping_id;
	unsigned char deadzone_enabled;
	unsigned char old_v1_5_conversion;
	char version[16];
};

struct gc2n64_adapter_info_bootloader {
	char version[16];
	unsigned char mcu_page_size;
	unsigned short bootloader_start_address;
};

struct gc2n64_adapter_info {
	int in_bootloader;
	union {
		struct gc2n64_adapter_info_app app;
		struct gc2n64_adapter_info_bootloader bootldr;
	};
};

int gc2n64_adapter_echotest(gcn64_hdl_t hdl, int verbosee);
int gc2n64_adapter_getInfo(gcn64_hdl_t hdl, struct gc2n64_adapter_info *inf);
void gc2n64_adapter_printInfo(struct gc2n64_adapter_info *inf);

int gc2n64_adapter_boot_eraseAll(gcn64_hdl_t hdl);
int gc2n64_adapter_boot_readBlock(gcn64_hdl_t hdl, unsigned int block_id, unsigned char dst[32]);
int gc2n64_adapter_dumpFlash(gcn64_hdl_t hdl);
int gc2n64_adapter_updateFirmware(gcn64_hdl_t hdl, const char *hexfile);
int gc2n64_adapter_enterBootloader(gcn64_hdl_t hdl);
int gc2n64_adapter_bootApplication(gcn64_hdl_t hdl);
int gc2n64_adapter_sendFirmwareBlocks(gcn64_hdl_t hdl, unsigned char *firmware, int len);
int gc2n64_adapter_verifyFirmware(gcn64_hdl_t hdl, unsigned char *firmware, int len);
int gc2n64_adapter_waitForBootloader(gcn64_hdl_t hdl, int timeout_s);

#endif // _gc2n64_adapter_h__

