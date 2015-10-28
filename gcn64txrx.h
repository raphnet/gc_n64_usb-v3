#ifndef _gcn64txrx_h__
#define _gcn64txrx_h__

void gcn64_sendBytes(unsigned char *data, unsigned char n_bytes);
unsigned int gcn64_receiveBits(unsigned char *dstbuf, unsigned char max_bits);

#endif // _gcn64txrx_h__
