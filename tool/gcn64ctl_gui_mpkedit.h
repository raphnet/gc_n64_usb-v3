#ifndef gcn64ctl_gui_mpkedit_h__
#define gcn64ctl_gui_mpkedit_h__

#include "mempak.h"

struct mpkedit_data;
struct application;

struct mpkedit_data *mpkedit_new(struct application *app);
void mpkedit_free(struct mpkedit_data *mpke);
void mpke_replaceMpk(struct application *app, mempak_structure_t *mpk, char *filename);
mempak_structure_t *mpke_getCurrentMempak(struct application *app);

#endif
