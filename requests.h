#ifndef _gcn64_requests_h__
#define _gcn64_requests_h__

/* Commands */
#define RQ_GCN64_SET_CONFIG_PARAM		0x01
#define RQ_GCN64_GET_CONFIG_PARAM		0x02
#define RQ_GCN64_SUSPEND_POLLING		0x03
#define RQ_GCN64_RAW_SI_COMMAND			0x80
#define RQ_GCN64_JUMP_TO_BOOTLOADER		0xFF

/* Configuration parameters and constants */
#define CFG_PARAM_MODE			0x00
#define CFG_MODE_STANDARD   	0x00

#define CFG_PARAM_SERIAL		0x01


#endif
