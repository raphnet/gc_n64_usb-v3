
// TO BE REMOVED

int mempak_dump(gcn64_hdl_t hdl);
int mempak_readBlock(gcn64_hdl_t hdl, unsigned short addr, unsigned char dst[32]);
int mempak_dumpToFile(gcn64_hdl_t hdl, const char *out_filename);
int mempak_writeFromFile(gcn64_hdl_t hdl, const char *in_filename);

