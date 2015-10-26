#ifndef gcn64ctl_gui_mpkedit_h__
#define gcn64ctl_gui_mpkedit_h__

struct mpkedit_data;
struct application;

struct mpkedit_data *mpkedit_new(struct application *app);
void mpkedit_free(struct mpkedit_data *mpke);

#endif
