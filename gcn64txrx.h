#ifndef _gcn64txrx_h__
#define _gcn64txrx_h__

void gcn64_sendBytes0(const unsigned char *data, unsigned char n_bytes);
void gcn64_sendBytes1(const unsigned char *data, unsigned char n_bytes);
void gcn64_sendBytes2(const unsigned char *data, unsigned char n_bytes);
void gcn64_sendBytes3(const unsigned char *data, unsigned char n_bytes);

/**
 * \brief Receive up to \max_bytes bytes
 * \param dstbuf Destination buffer
 * \param max_bytes The maximum number of bytes
 * \return The number of received bytes. 0xFF in case of error, 0xFE in case of overflow (max_bytes too small)
 */
unsigned char gcn64_receiveBytes0(unsigned char *dstbuf, unsigned char max_bytes);
unsigned char gcn64_receiveBytes1(unsigned char *dstbuf, unsigned char max_bytes);
unsigned char gcn64_receiveBytes2(unsigned char *dstbuf, unsigned char max_bytes);
unsigned char gcn64_receiveBytes3(unsigned char *dstbuf, unsigned char max_bytes);

#endif // _gcn64txrx_h__
