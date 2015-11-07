#include <glib.h>
#include <gtk/gtk.h>

#include "gcn64.h"
#include "gcn64lib.h"
#include "gcn64ctl_gui_mpkedit.h"

#define GET_ELEMENT(TYPE, ELEMENT)	(TYPE *)gtk_builder_get_object(app->builder, #ELEMENT)
#define GET_UI_ELEMENT(TYPE, ELEMENT)   TYPE *ELEMENT = GET_ELEMENT(TYPE, ELEMENT)

void errorPopup(struct application *app, const char *message);

struct application {
	GtkBuilder *builder;
	GtkWindow *mainwindow;

	gcn64_hdl_t current_adapter_handle;
	struct gcn64_info current_adapter_info;
	GThread *updater_thread;

	const char *update_status;
	const char *updateHexFile;
	int update_percent;
	int update_dialog_response;

	struct mpkedit_data *mpke;
	int stop_reading_mempak;
	int inhibit_periodic_updates;
	int controller_type;
	int firmware_maj, firmware_min, firmware_build;
	int at90usb1287;
};


