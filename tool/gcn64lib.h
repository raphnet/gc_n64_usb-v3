#ifndef _gcn64_lib_h__
#define _gcn64_lib_h__

#include "gcn64.h"

int gcn64lib_suspendPolling(gcn64_hdl_t hdl, unsigned char suspend);
int gcn64lib_setConfig(gcn64_hdl_t hdl, unsigned char param, unsigned char *data, unsigned char len);
int gcn64lib_getConfig(gcn64_hdl_t hdl, unsigned char param, unsigned char *rx, unsigned char rx_max);
int gcn64lib_rawSiCommand(gcn64_hdl_t hdl, unsigned char channel, unsigned char *tx, unsigned char tx_len, unsigned char *rx, unsigned char max_rx);
int gcn64lib_getVersion(gcn64_hdl_t hdl, char *dst, int dstmax);

int gcn64lib_8bit_scan(gcn64_hdl_t hdl, unsigned char min, unsigned char max);
int gcn64lib_16bit_scan(gcn64_hdl_t hdl, unsigned short min, unsigned short max);

#endif // _gcn64_lib_h__
