#ifndef _gcn64_priv_h__
#define _gcn64_priv_h__

#include "hidapi.h"

struct gcn64_list_ctx {
	struct hid_device_info *devs, *cur_dev;
};

#endif
