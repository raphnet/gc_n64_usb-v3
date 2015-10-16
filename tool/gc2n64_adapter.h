#ifndef _gc2n64_adapter_h__
#define _gc2n64_adapter_h__

#include "gcn64.h"

#define GC2N64_MAX_MAPPING_PAIRS	32
#define GC2N64_NUM_MAPPINGS			5

struct gc2n64_adapter_mapping_pair {
	int gc, n64;
};

struct gc2n64_adapter_mapping {
	int n_pairs;
	struct gc2n64_adapter_mapping_pair pairs[GC2N64_MAX_MAPPING_PAIRS];
};

struct gc2n64_adapter_info_app {
	unsigned char default_mapping_id;
	unsigned char deadzone_enabled;
	unsigned char old_v1_5_conversion;
	unsigned char upgradeable;
	char version[16];
	struct gc2n64_adapter_mapping mappings[GC2N64_NUM_MAPPINGS];
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

int gc2n64_adapter_echotest(gcn64_hdl_t hdl, int channel, int verbosee);
int gc2n64_adapter_getInfo(gcn64_hdl_t hdl,  int channel, struct gc2n64_adapter_info *inf);
void gc2n64_adapter_printInfo(struct gc2n64_adapter_info *inf);
void gc2n64_adapter_printMapping(struct gc2n64_adapter_mapping *map);

#define MAPPING_SLOT_BUILTIN_CURRENT	0
#define MAPPING_SLOT_DPAD_UP			1
#define MAPPING_SLOT_DPAD_DOWN			2
#define MAPPING_SLOT_DPAD_LEFT			3
#define MAPPING_SLOT_DPAD_RIGHT			4
const char *gc2n64_adapter_getMappingSlotName(unsigned char id, int default_context);

int gc2n64_adapter_getMapping(gcn64_hdl_t hdl, int channel, int mapping_id, struct gc2n64_adapter_mapping *dst_mapping);
int gc2n64_adapter_setMapping(gcn64_hdl_t hdl, int channel, struct gc2n64_adapter_mapping *mapping);
int gc2n64_adapter_storeCurrentMapping(gcn64_hdl_t hdl, int channel, int dst_slot);

int gc2n64_adapter_saveMapping(struct gc2n64_adapter_mapping *map, const char *dstfile);
struct gc2n64_adapter_mapping *gc2n64_adapter_loadMapping(const char *srcfile);

int gc2n64_adapter_waitNotBusy(gcn64_hdl_t hdl, int channel, int verbose);
int gc2n64_adapter_boot_isBusy(gcn64_hdl_t hdl, int channel);

int gc2n64_adapter_boot_eraseAll(gcn64_hdl_t hdl, int channel);
int gc2n64_adapter_boot_readBlock(gcn64_hdl_t hdl, int channel, unsigned int block_id, unsigned char dst[32]);
int gc2n64_adapter_dumpFlash(gcn64_hdl_t hdl, int channel);
int gc2n64_adapter_updateFirmware(gcn64_hdl_t hdl, int channel, const char *hexfile);
int gc2n64_adapter_enterBootloader(gcn64_hdl_t hdl, int channel);
int gc2n64_adapter_bootApplication(gcn64_hdl_t hdl, int channel);
int gc2n64_adapter_sendFirmwareBlocks(gcn64_hdl_t hdl, int channel, unsigned char *firmware, int len);
int gc2n64_adapter_verifyFirmware(gcn64_hdl_t hdl, int channel, unsigned char *firmware, int len);
int gc2n64_adapter_waitForBootloader(gcn64_hdl_t hdl, int channel, int timeout_s);

#endif // _gc2n64_adapter_h__

