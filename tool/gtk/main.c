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
	struct gcn64_info current_adapter_info;
	GThread *updater_thread;

	const char *update_status;
	const char *updateHexFile;
	int update_percent;
	int update_dialog_response;
};

static void updateGuiFromAdapter(struct application *app);
gboolean rebuild_device_list_store(gpointer data);
void deselect_adapter(struct application *app);

gboolean updateDonefunc(gpointer data)
{
	struct application *app = data;
	GET_UI_ELEMENT(GtkDialog, firmware_update_dialog);

	printf("updateDonefunc\n");
	gtk_dialog_response(firmware_update_dialog, app->update_dialog_response);
	g_thread_join(app->updater_thread);

	rebuild_device_list_store(data);
	updateGuiFromAdapter(app);

	return FALSE;
}

gboolean updateProgress(gpointer data)
{
	struct application *app = data;
	GET_UI_ELEMENT(GtkProgressBar, updateProgress);
	GET_UI_ELEMENT(GtkLabel, updateStatus);

	gtk_progress_bar_set_fraction(updateProgress, app->update_percent / 100.0);
	gtk_label_set_text(updateStatus, app->update_status);

	return FALSE;
}

gboolean closeAdapter(gpointer data)
{
	struct application *app = data;

	deselect_adapter(app);

	return FALSE;
}

gpointer updateThreadFunc(gpointer data)
{
	struct application *app = data;
	int res;
	FILE *dfu_fp;
	char linebuf[256];
	char cmd[256];
	int retries = 10;

	app->update_status = "Starting...";
	app->update_percent = 1;
	gdk_threads_add_idle(updateProgress, data);

	app->update_percent = 10;
	app->update_status = "Enter bootloader...";

	gcn64lib_bootloader(app->current_adapter_handle);
	gdk_threads_add_idle(closeAdapter, data);

	app->update_percent = 19;
	app->update_status = "Erasing chip...";
	do {
		app->update_percent++;
		gdk_threads_add_idle(updateProgress, data);

		dfu_fp = popen("dfu-programmer at90usb1287 erase", "r");
		if (!dfu_fp) {
			app->update_dialog_response = GTK_RESPONSE_REJECT;
			gdk_threads_add_idle(updateDonefunc, data);
			return NULL;
		}

		do {
			fgets(linebuf, sizeof(linebuf), dfu_fp);
		} while(!feof(dfu_fp));

		res = pclose(dfu_fp);
		printf("Pclose: %d\n", res);

		if (res==0) {
			break;
		}
		sleep(1);
	} while (retries--);

	if (!retries) {
		app->update_dialog_response = GTK_RESPONSE_REJECT;
		gdk_threads_add_idle(updateDonefunc, data);
	}

	app->update_status = "Chip erased";
	app->update_percent = 20;
	gdk_threads_add_idle(updateProgress, data);


	app->update_status = "Programming ...";
	app->update_percent = 30;
	gdk_threads_add_idle(updateProgress, data);

	snprintf(cmd, sizeof(cmd), "dfu-programmer at90usb1287 flash %s", app->updateHexFile);
	dfu_fp = popen(cmd, "r");
	if (!dfu_fp) {
		app->update_dialog_response = GTK_RESPONSE_REJECT;
		gdk_threads_add_idle(updateDonefunc, data);
		return NULL;
	}

	do {
		fgets(linebuf, sizeof(linebuf), dfu_fp);
		printf("ln: %s\n\n", linebuf);
		if (strstr(linebuf, "Validating")) {
			app->update_status = "Validating...";
			app->update_percent = 70;
			gdk_threads_add_idle(updateDonefunc, data);
		}
	} while(!feof(dfu_fp));

	res = pclose(dfu_fp);
	printf("Pclose: %d\n", res);

	if (res != 0) {
		app->update_dialog_response = GTK_RESPONSE_REJECT;
		gdk_threads_add_idle(updateDonefunc, data);
		return NULL;
	}

	app->update_status = "Starting firmware...";
	app->update_percent = 80;
	gdk_threads_add_idle(updateProgress, data);

	dfu_fp = popen("dfu-programmer at90usb1287 start", "r");
	if (!dfu_fp) {
		app->update_dialog_response = GTK_RESPONSE_REJECT;
		gdk_threads_add_idle(updateDonefunc, data);
		return NULL;
	}

	res = pclose(dfu_fp);
	printf("Pclose: %d\n", res);

	if (res!=0) {
		app->update_dialog_response = GTK_RESPONSE_REJECT;
		gdk_threads_add_idle(updateDonefunc, data);
		return NULL;
	}


	app->update_status = "Waiting for device...";
	app->update_percent = 90;
	gdk_threads_add_idle(updateProgress, data);

	retries = 6;
	do {
		app->current_adapter_handle = gcn64_openBy(&app->current_adapter_info, GCN64_FLG_OPEN_BY_SERIAL);
		if (app->current_adapter_handle)
			break;
		sleep(1);
		app->update_percent++;
		gdk_threads_add_idle(updateProgress, data);
	} while (retries--);

	if (!app->current_adapter_handle) {
		app->update_dialog_response = GTK_RESPONSE_REJECT;
		gdk_threads_add_idle(updateDonefunc, data);
		return NULL;
	}


	app->update_status = "Done";
	app->update_percent = 100;
	gdk_threads_add_idle(updateProgress, data);

	printf("Update done\n");
	app->update_dialog_response = GTK_RESPONSE_OK;
	gdk_threads_add_idle(updateDonefunc, data);
	return NULL;
}

G_MODULE_EXPORT void updatestart_btn_clicked_cb(GtkWidget *w, gpointer data)
{
	struct application *app = data;
	GET_UI_ELEMENT(GtkButtonBox, update_dialog_btnBox);

	app->updater_thread = g_thread_new("updater", updateThreadFunc, app);
	gtk_widget_set_sensitive(GTK_WIDGET(update_dialog_btnBox), FALSE);
}

void deselect_adapter(struct application *app)
{
	GET_UI_ELEMENT(GtkComboBox, cb_adapter_list);

	printf("deselect adapter\n");

	if (app->current_adapter_handle) {
		gcn64_closeDevice(app->current_adapter_handle);
		app->current_adapter_handle = NULL;
	}

	gtk_combo_box_set_active_iter(cb_adapter_list, NULL);
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
	FILE *dfu_fp;

	/* Test for dfu-programmer presence in path*/
	dfu_fp = popen("dfu-programmer --help", "r");
	if (!dfu_fp) {
		perror("popen");
		return;
	}
	if (pclose(dfu_fp)) {
		errorPopop(app, "dfu-programmmer not found. Cannot perform update.");
		return;
	}

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
		app->updateHexFile = filename;

		if (!check_ihex_for_signature(filename, "9c3ea8b8-753f-11e5-a0dc-001bfca3c593")) {
			errorPopop(app, "Signature not found - This file is invalid or not meant for this adapter");
		}

		/* Prepare the update dialog widgets... */
		gtk_label_set_text(lbl_firmware_filename, basename);
		gtk_widget_set_sensitive(GTK_WIDGET(update_dialog_btnBox), TRUE);
		updateProgress(data);

		/* Run the dialog */
		res = gtk_dialog_run(firmware_update_dialog);
		gtk_widget_hide( GTK_WIDGET(firmware_update_dialog));

		if (res == GTK_RESPONSE_OK) {
			infoPopop(app, "Update succeeded.");
		} else if (res == GTK_RESPONSE_REJECT) {
			errorPopop(app, "Update failed. Suggestion: Do not disconnect the adapter and retry right away!");
		}
		printf("Update dialog done\n");

		g_free(filename);
		g_free(basename);
	}

	gtk_widget_destroy(dialog);
}

static void updateGuiFromAdapter(struct application *app)
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
	struct gcn64_info *info = &app->current_adapter_info;

	if (!app->current_adapter_handle) {
		deselect_adapter(app);
		return;
	}

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

	if (sizeof(wchar_t)==4) {
		gtk_label_set_text(label_product_name, g_ucs4_to_utf8((void*)info->str_prodname, -1, NULL, NULL, NULL));
	} else {
		gtk_label_set_text(label_product_name, g_utf16_to_utf8((void*)info->str_prodname, -1, NULL, NULL, NULL));
	}

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
	if (n != 0) {
		errorPopop(app, "Error setting configuration");
		deselect_adapter(app);
		rebuild_device_list_store(data);
	}
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
	int i, n;
	unsigned char buf;

	for (i=0; configurable_bits[i].chkbtn; i++) {
		buf = gtk_toggle_button_get_active(configurable_bits[i].chkbtn);
		n = gcn64lib_setConfig(app->current_adapter_handle, configurable_bits[i].cfg_param, &buf, 1);
		if (n != 0) {
			errorPopop(app, "Error setting configuration");
			deselect_adapter(app);
			rebuild_device_list_store(app);
			break;
		}
		printf("cfg param %02x now set to %d\n", configurable_bits[i].cfg_param, buf);
	}
}

gboolean rebuild_device_list_store(gpointer data)
{
	struct application *app = data;
	struct gcn64_list_ctx *listctx;
	struct gcn64_info info;
	GtkListStore *list_store;
	GET_UI_ELEMENT(GtkComboBox, cb_adapter_list);

	list_store = GTK_LIST_STORE( gtk_builder_get_object(app->builder, "adaptersList") );

	gtk_list_store_clear(list_store);

	printf("Listing device...\n");
	listctx = gcn64_allocListCtx();
	if (!listctx)
		return FALSE;

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
		if (app->current_adapter_handle) {
			if (!wcscmp(app->current_adapter_info.str_serial, info.str_serial)) {
				gtk_combo_box_set_active_iter(cb_adapter_list, &iter);
			}
		}
	}

	gcn64_freeListCtx(listctx);
	return FALSE;
}

G_MODULE_EXPORT void onMainWindowShow(GtkWidget *win, gpointer data)
{
	int res;
	struct application *app = data;

	res = gcn64_init(1);
	if (res) {
		GtkWidget *d = GTK_WIDGET( gtk_builder_get_object(app->builder, "internalError") );
		gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(d), "gcn64_init failed (returned %d)", res);
		gtk_widget_show(d);
		return;
	}

	rebuild_device_list_store(data);
}

G_MODULE_EXPORT void adapterSelected(GtkComboBox *cb, gpointer data)
{
	struct application *app = data;
	GtkTreeIter iter;
	GtkListStore *list_store = GTK_LIST_STORE( gtk_builder_get_object(app->builder, "adaptersList") );
	GtkWidget *adapter_details = GTK_WIDGET( gtk_builder_get_object(app->builder, "adapterDetails") );
	struct gcn64_info *info;

	if (app->current_adapter_handle) {
		gcn64_closeDevice(app->current_adapter_handle);
		app->current_adapter_handle = NULL;
		gtk_widget_set_sensitive(adapter_details, FALSE);
	}

	if (gtk_combo_box_get_active_iter(cb, &iter)) {
		gtk_tree_model_get(GTK_TREE_MODEL(list_store), &iter, 3, &info, -1);
		printf("%s\n", info->str_path);

		app->current_adapter_handle = gcn64_openDevice(info);
		if (!app->current_adapter_handle) {
			errorPopop(app, "Failed to open adapter");
			deselect_adapter(app);
			return;
		}

		memcpy(&app->current_adapter_info, info, sizeof(struct gcn64_info));

		updateGuiFromAdapter(app);
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

G_MODULE_EXPORT void onFileRescan(GtkWidget *wid, gpointer data)
{
	rebuild_device_list_store(data);
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

