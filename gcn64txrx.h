#ifndef _gcn64txrx_h__
#define _gcn64txrx_h__

void gcn64_sendBytes(const unsigned char *data, unsigned char n_bytes);

/**
 * \brief Receive up to \max_bytes bytes
 * \param dstbuf Destination buffer
 * \param max_bytes The maximum number of bytes
 * \return The number of received bytes. 0xFF in case of error, 0xFE in case of overflow (max_bytes too small)
 */
unsigned char gcn64_receiveBytes(unsigned char *dstbuf, unsigned char max_bytes);

#endif // _gcn64txrx_h__
