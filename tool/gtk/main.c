#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <gtk/gtk.h>
#include "../gcn64.h"
#include "../gcn64lib.h"
#include "../../requests.h"
#include "../ihex.h"

#define GET_ELEMENT(TYPE, ELEMENT)	(TYPE *)gtk_builder_get_object(app->builder, #ELEMENT)
#define GET_UI_ELEMENT(TYPE, ELEMENT)   TYPE *ELEMENT = GET_ELEMENT(TYPE, ELEMENT)

struct application {
	GtkBuilder *builder;
	GtkWindow *mainwindow;
	gcn64_hdl_t current_adapter_handle;
};

G_MODULE_EXPORT void updatestart_btn_clicked_cb(GtkWidget *w, gpointer data)
{
	struct application *app = data;
	GET_UI_ELEMENT(GtkProgressBar, updateProgress);
	GET_UI_ELEMENT(GtkDialog, firmware_update_dialog);
	GET_UI_ELEMENT(GtkButtonBox, update_dialog_btnBox);
	int i;

	gtk_widget_set_sensitive(GTK_WIDGET(update_dialog_btnBox), FALSE);

	for (i=0; i<100; i++) {
		gtk_progress_bar_set_fraction(updateProgress, i/100.0);
		usleep(50000);
		gtk_main_iteration_do(FALSE);
	}

	gtk_dialog_response(firmware_update_dialog, GTK_RESPONSE_OK);
}

void infoPopop(struct application *app, const char *message)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new(app->mainwindow,
									GTK_DIALOG_DESTROY_WITH_PARENT,
									GTK_MESSAGE_INFO,
									GTK_BUTTONS_CLOSE,
									message);

	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}


void errorPopop(struct application *app, const char *message)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new(app->mainwindow,
									GTK_DIALOG_DESTROY_WITH_PARENT,
									GTK_MESSAGE_ERROR,
									GTK_BUTTONS_CLOSE,
									message);

	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}

#define IHEX_MAX_FILE_SIZE	0x20000
char check_ihex_for_signature(const char *filename, const char *signature)
{
	unsigned char *buf;
	int max_address;

	buf = malloc(IHEX_MAX_FILE_SIZE);
	if (!buf) {
		perror("malloc");
		return 0;
	}

	max_address= load_ihex(filename, buf, IHEX_MAX_FILE_SIZE);

	if (max_address > 0) {
		if (!memmem(buf, max_address + 1, signature, strlen(signature))) {
			return 0;
		}
		return 1;
	}

	return 0;
}

G_MODULE_EXPORT void update_usbadapter_firmware(GtkWidget *w, gpointer data)
{
	struct application *app = data;
	gint res;
	GtkWidget *dialog;
	GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
	GET_UI_ELEMENT(GtkFileFilter, hexfilter);
	GET_UI_ELEMENT(GtkDialog, firmware_update_dialog);
	GET_UI_ELEMENT(GtkLabel, lbl_firmware_filename);
	GET_UI_ELEMENT(GtkButtonBox, update_dialog_btnBox);

	dialog = gtk_file_chooser_dialog_new("Open .hex file",
										app->mainwindow,
										action,
										"_Cancel",
										GTK_RESPONSE_CANCEL,
										"_Open",
										GTK_RESPONSE_ACCEPT,
										NULL);

	gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(dialog), hexfilter);

	res = gtk_dialog_run (GTK_DIALOG(dialog));
	if (res == GTK_RESPONSE_ACCEPT) {
		char *filename, *basename;
		GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);

		filename = gtk_file_chooser_get_filename(chooser);
		basename = g_path_get_basename(filename);

		printf("File selected: %s\n", filename);

		if (!check_ihex_for_signature(filename, "9c3ea8b8-753f-11e5-a0dc-001bfca3c593")) {
			errorPopop(app, "Signature not found - This file is invalid or not meant for this adapter");
		}

		/* Prepare the update dialog widgets... */
		gtk_label_set_text(lbl_firmware_filename, basename);
		gtk_widget_set_sensitive(GTK_WIDGET(update_dialog_btnBox), TRUE);

		/* Run the dialog */
		res = gtk_dialog_run(firmware_update_dialog);
		gtk_widget_hide( GTK_WIDGET(firmware_update_dialog));

		if (res == GTK_RESPONSE_OK) {
			infoPopop(app, "Update succeeded.");
		}

		g_free(filename);
		g_free(basename);
	}

	gtk_widget_destroy(dialog);
}

static void updateGuiFromAdapter(struct application *app, struct gcn64_info *info)
{
	unsigned char buf[32];
	int n;
	struct {
		unsigned char cfg_param;
		GtkToggleButton *chkbtn;
	} configurable_bits[] = {
//		{ CFG_PARAM_N64_SQUARE, GET_ELEMENT(GtkCheckButton, chkbtn_n64_square) },
//		{ CFG_PARAM_GC_MAIN_SQUARE, GET_ELEMENT(GtkCheckButton, chkbtn_gc_main_square) },
//		{ CFG_PARAM_GC_CSTICK_SQUARE, GET_ELEMENT(GtkCheckButton, chkbtn_gc_cstick_square) },
		{ CFG_PARAM_FULL_SLIDERS, GET_ELEMENT(GtkToggleButton, chkbtn_gc_full_sliders) },
		{ CFG_PARAM_INVERT_TRIG, GET_ELEMENT(GtkToggleButton, chkbtn_gc_invert_trig) },
		{ },
	};
	GET_UI_ELEMENT(GtkLabel, label_product_name);
	GET_UI_ELEMENT(GtkLabel, label_firmware_version);
	GET_UI_ELEMENT(GtkLabel, label_usb_id);
	GET_UI_ELEMENT(GtkLabel, label_device_path);
	int i;

	GtkSpinButton *pollInterval0 = GTK_SPIN_BUTTON( gtk_builder_get_object(app->builder, "pollInterval0") );

	n = gcn64lib_getConfig(app->current_adapter_handle, CFG_PARAM_POLL_INTERVAL0, buf, sizeof(buf));
	if (n == 1) {
		printf("poll interval: %d\n", buf[0]);
		gtk_spin_button_set_value(pollInterval0, (gdouble)buf[0]);
	}

	for (i=0; configurable_bits[i].chkbtn; i++) {
		gcn64lib_getConfig(app->current_adapter_handle, configurable_bits[i].cfg_param, buf, sizeof(buf));
		printf("config param %02x is %d\n",  configurable_bits[i].cfg_param, buf[0]);
		gtk_toggle_button_set_active(configurable_bits[i].chkbtn, buf[0]);
	}

	gtk_label_set_text(label_product_name, g_ucs4_to_utf8((void*)info->str_prodname, -1, NULL, NULL, NULL));

	if (0 == gcn64lib_getVersion(app->current_adapter_handle, (char*)buf, sizeof(buf))) {
		gtk_label_set_text(label_firmware_version, (char*)buf);

	}

	snprintf((char*)buf, sizeof(buf), "%04x:%04x", info->usb_vid, info->usb_pid);
	gtk_label_set_text(label_usb_id, (char*)buf);

	gtk_label_set_text(label_device_path, info->str_path);
}

G_MODULE_EXPORT void pollIntervalChanged(GtkWidget *win, gpointer data)
{
	struct application *app = data;
	GET_UI_ELEMENT(GtkSpinButton, pollInterval0);
	gdouble value;
	int n;
	unsigned char buf;

	value = gtk_spin_button_get_value(pollInterval0);
	printf("Value: %d\n", (int)value);
	buf = (int)value;

	n = gcn64lib_setConfig(app->current_adapter_handle, CFG_PARAM_POLL_INTERVAL0, &buf, 1);
}


G_MODULE_EXPORT void config_checkbox_changed(GtkWidget *win, gpointer data)
{
	struct application *app = data;
	struct {
		unsigned char cfg_param;
		GtkToggleButton *chkbtn;
	} configurable_bits[] = {
//		{ CFG_PARAM_N64_SQUARE, GET_ELEMENT(GtkCheckButton, chkbtn_n64_square) },
//		{ CFG_PARAM_GC_MAIN_SQUARE, GET_ELEMENT(GtkCheckButton, chkbtn_gc_main_square) },
//		{ CFG_PARAM_GC_CSTICK_SQUARE, GET_ELEMENT(GtkCheckButton, chkbtn_gc_cstick_square) },
		{ CFG_PARAM_FULL_SLIDERS, GET_ELEMENT(GtkToggleButton, chkbtn_gc_full_sliders) },
		{ CFG_PARAM_INVERT_TRIG, GET_ELEMENT(GtkToggleButton, chkbtn_gc_invert_trig) },
		{ },
	};
	int i;
	unsigned char buf;

	for (i=0; configurable_bits[i].chkbtn; i++) {
		buf = gtk_toggle_button_get_active(configurable_bits[i].chkbtn);
		gcn64lib_setConfig(app->current_adapter_handle, configurable_bits[i].cfg_param, &buf, 1);
		printf("cfg param %02x now set to %d\n", configurable_bits[i].cfg_param, buf);
	}
}

G_MODULE_EXPORT void onMainWindowShow(GtkWidget *win, gpointer data)
{
	int res;
	struct gcn64_list_ctx *listctx;
	struct gcn64_info info;
	struct application *app = data;
	GtkListStore *list_store;

	res = gcn64_init(1);
	if (res) {
		GtkWidget *d = GTK_WIDGET( gtk_builder_get_object(app->builder, "internalError") );
		gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(d), "gcn64_init failed (returned %d)", res);
		gtk_widget_show(d);
		return;
	}

	list_store = GTK_LIST_STORE( gtk_builder_get_object(app->builder, "adaptersList") );

	printf("Listing device...\n");
	listctx = gcn64_allocListCtx();

	while (gcn64_listDevices(&info, listctx)) {
		GtkTreeIter iter;
		printf("Device '%ls'\n", info.str_prodname);
		gtk_list_store_append(list_store, &iter);
		if (sizeof(wchar_t)==4) {
			gtk_list_store_set(list_store, &iter,
							0, g_ucs4_to_utf8((void*)info.str_serial, -1, NULL, NULL, NULL),
							1, g_ucs4_to_utf8((void*)info.str_prodname, -1, NULL, NULL, NULL),
							3, g_memdup(&info, sizeof(struct gcn64_info)),
								-1);
		} else {
			gtk_list_store_set(list_store, &iter,
							0, g_utf16_to_utf8((void*)info.str_serial, -1, NULL, NULL, NULL),
							1, g_utf16_to_utf8((void*)info.str_prodname, -1, NULL, NULL, NULL),
							3, g_memdup(&info, sizeof(struct gcn64_info)),
								-1);
		}
	}

	gcn64_freeListCtx(listctx);
}

G_MODULE_EXPORT void adapterSelected(GtkComboBox *cb, gpointer data)
{
	struct application *app = data;
	GtkTreeIter iter;
	GtkListStore *list_store = GTK_LIST_STORE( gtk_builder_get_object(app->builder, "adaptersList") );
	GtkWidget *adapter_details = GTK_WIDGET( gtk_builder_get_object(app->builder, "adapterDetails") );
	struct gcn64_info *info;

	if (!app->current_adapter_handle) {
		gcn64_closeDevice(app->current_adapter_handle);
		app->current_adapter_handle = NULL;
		gtk_widget_set_sensitive(adapter_details, FALSE);
	}

	if (gtk_combo_box_get_active_iter(cb, &iter)) {
		gtk_tree_model_get(GTK_TREE_MODEL(list_store), &iter, 3, &info, -1);
		printf("%s\n", info->str_path);

		app->current_adapter_handle = gcn64_openDevice(info);
		if (!app->current_adapter_handle) {
			printf("Access error!\n");
		}

		updateGuiFromAdapter(app, info);
		gtk_widget_set_sensitive(adapter_details, TRUE);
	}
}

G_MODULE_EXPORT void onMainWindowHide(GtkWidget *win, gpointer data)
{
	gcn64_shutdown();
}

G_MODULE_EXPORT void suspend_polling(GtkButton *button, gpointer data)
{
	struct application *app = data;

	gcn64lib_suspendPolling(app->current_adapter_handle, 1);
}

G_MODULE_EXPORT void resume_polling(GtkButton *button, gpointer data)
{
	struct application *app = data;

	gcn64lib_suspendPolling(app->current_adapter_handle, 0);
}



int
main( int    argc,
      char **argv )
{
    GtkWindow  *window;
    GError     *error = NULL;
	struct application app = { };

    /* Init GTK+ */
    gtk_init( &argc, &argv );

    /* Create new GtkBuilder object */
    app.builder = gtk_builder_new();
    /* Load UI from file. If error occurs, report it and quit application.
     * Replace "tut.glade" with your saved project. */
    if( ! gtk_builder_add_from_file( app.builder, "gcn64cfg.glade", &error ) )
    {
        g_warning( "%s", error->message );
        g_free( error );
        return( 1 );
    }

    /* Get main window pointer from UI */
    window = GTK_WINDOW( gtk_builder_get_object( app.builder, "mainWindow" ) );
	app.mainwindow = window;

    /* Connect signals */
    gtk_builder_connect_signals( app.builder, &app );

    /* Destroy builder, since we don't need it anymore */
//    g_object_unref( G_OBJECT( builder ) );

    /* Show window. All other widgets are automatically shown by GtkBuilder */
    gtk_widget_show( GTK_WIDGET(window) );

    /* Start main loop */
    gtk_main();

    return( 0 );
}

