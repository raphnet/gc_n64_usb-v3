#ifndef _mempak_gcn64usb_h__
#define _mempak_gcn64usb_h__

#include "mempak.h"

int gcn64lib_mempak_detect(gcn64_hdl_t hdl);
int gcn64lib_mempak_readBlock(gcn64_hdl_t hdl, unsigned short addr, unsigned char dst[32]);
int gcn64lib_mempak_writeBlock(gcn64_hdl_t hdl, unsigned short addr, unsigned char data[32]);

int gcn64lib_mempak_download(gcn64_hdl_t hdl, int channel, mempak_structure_t **mempak, void (*progressCb)(int cur_addr, void *ctx), void *ctx);
int gcn64lib_mempak_upload(gcn64_hdl_t hdl, int channel, mempak_structure_t *pak, void (*progressCb)(int cur_addr, void *ctx), void *ctx);

#endif // _mempak_gcn64usb_h__
