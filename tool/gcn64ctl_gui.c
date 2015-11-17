#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../requests.h"
#include "ihex.h"
#include "mempak.h"
#include "gcn64ctl_gui.h"
#include "mempak_gcn64usb.h"
#include "gcn64lib.h"

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
	app->inhibit_periodic_updates = 0;

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

	app->inhibit_periodic_updates = 1;

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

		if (app->at90usb1287) {
			dfu_fp = popen("dfu-programmer at90usb1287 erase", "r");
		} else {
			dfu_fp = popen("dfu-programmer atmega32u2 erase", "r");
		}
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

	snprintf(cmd, sizeof(cmd), "dfu-programmer %s flash %s",
				app->at90usb1287 ? "at90usb1287" : "atmega32u2",
				app->updateHexFile);
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

void infoPopup(struct application *app, const char *message)
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


void errorPopup(struct application *app, const char *message)
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
	GtkWidget *dialog = NULL;
	GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
	GET_UI_ELEMENT(GtkFileFilter, hexfilter);
	GET_UI_ELEMENT(GtkDialog, firmware_update_dialog);
	GET_UI_ELEMENT(GtkLabel, lbl_firmware_filename);
	GET_UI_ELEMENT(GtkButtonBox, update_dialog_btnBox);
	FILE *dfu_fp;
	char *filename = NULL, *basename = NULL;
	char adap_sig[64];
#ifndef WINDOWS
	const char *notfound = "dfu-programmer not found. Cannot perform update.";
#else
	const char *notfound = "dfu-programmer.exe not found. Cannot perform update.";
#endif

	if (gcn64lib_getSignature(app->current_adapter_handle, adap_sig, sizeof(adap_sig))) {
		errorPopup(app, "Could not read adapter signature - This file may not be meant for it (Bricking hazard!)");
		goto done;
	}

	/* Test for dfu-programmer presence in path*/
	dfu_fp = popen("dfu-programmer --version", "r");
	//dfu_fp = popen("dfu2-programmer --version", "r");
	if (!dfu_fp) {
		perror("popen");
		errorPopup(app, notfound);
		return;
	}
	res = pclose(dfu_fp);
#ifdef WINDOWS
	// Under Mingw, 0 is returned when dfu-programmmer was found 
	// and executed. Otherwise 1.
	if (res != 0) {
#else
	// Under Unix, the usual is available.
	if (!WIFEXITED(res) || (WEXITSTATUS(res)!=1)) {
#endif
		if (res) {
			errorPopup(app, notfound);
			return;
		}
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
		GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);

		filename = gtk_file_chooser_get_filename(chooser);
		basename = g_path_get_basename(filename);

		gtk_widget_destroy(dialog);
		dialog = NULL;

		printf("File selected: %s\n", filename);
		app->updateHexFile = filename;

		if (!check_ihex_for_signature(filename, adap_sig)) {
			errorPopup(app, "Signature not found - This file is invalid or not meant for this adapter");
			goto done;
		}

		// For my development board based on at90usb1287. Need to pass the correct
		// MCU argument to dfu-programmer
		if (0 == strcmp("e106420a-7c54-11e5-ae9a-001bfca3c593", adap_sig)) {
			app->at90usb1287 = 1;
		} else {
			app->at90usb1287 = 0;
		}

		/* Prepare the update dialog widgets... */
		gtk_label_set_text(lbl_firmware_filename, basename);
		gtk_widget_set_sensitive(GTK_WIDGET(update_dialog_btnBox), TRUE);
		app->update_percent = 0;
		app->update_status = "Ready";
		updateProgress(data);

		/* Run the dialog */
		res = gtk_dialog_run(firmware_update_dialog);
		gtk_widget_hide( GTK_WIDGET(firmware_update_dialog));

		if (res == GTK_RESPONSE_OK) {
			infoPopup(app, "Update succeeded.");
		} else if (res == GTK_RESPONSE_REJECT) {
			errorPopup(app, "Update failed. Suggestion: Do not disconnect the adapter and retry right away!");
		}
		printf("Update dialog done\n");

	}

done:
	if (filename)
		g_free(filename);
	if (basename)
		g_free(basename);
	if (dialog)
		gtk_widget_destroy(dialog);
}

static gboolean periodic_updater(gpointer data)
{
	struct application *app = data;
	GET_UI_ELEMENT(GtkLabel, label_controller_type);
	GET_UI_ELEMENT(GtkButton, btn_read_mempak);
	GET_UI_ELEMENT(GtkButton, btn_write_mempak);
	GET_UI_ELEMENT(GtkButton, btn_rumble_test);

	if (app->current_adapter_handle && !app->inhibit_periodic_updates) {
		app->controller_type = gcn64lib_getControllerType(app->current_adapter_handle, 0);
		gtk_label_set_text(label_controller_type, gcn64lib_controllerName(app->controller_type));

		switch (app->controller_type)
		{
			case CTL_TYPE_N64:
				gtk_widget_set_sensitive(GTK_WIDGET(btn_read_mempak), TRUE);
				gtk_widget_set_sensitive(GTK_WIDGET(btn_write_mempak), TRUE);
				gtk_widget_set_sensitive(GTK_WIDGET(btn_rumble_test), TRUE);
				break;
			case CTL_TYPE_GC:
				gtk_widget_set_sensitive(GTK_WIDGET(btn_rumble_test), TRUE);
				gtk_widget_set_sensitive(GTK_WIDGET(btn_read_mempak), FALSE);
				gtk_widget_set_sensitive(GTK_WIDGET(btn_write_mempak), FALSE);
				break;

			default:
			case CTL_TYPE_NONE:
				gtk_widget_set_sensitive(GTK_WIDGET(btn_read_mempak), FALSE);
				gtk_widget_set_sensitive(GTK_WIDGET(btn_write_mempak), FALSE);
				gtk_widget_set_sensitive(GTK_WIDGET(btn_rumble_test), FALSE);
				break;
		}

	}
	return TRUE;
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
	GET_UI_ELEMENT(GtkSpinButton, pollInterval0);
	int i;
	struct gcn64_info *info = &app->current_adapter_info;
	char adap_sig[64];

	if (!app->current_adapter_handle) {
		deselect_adapter(app);
		return;
	}

	if (gcn64lib_getSignature(app->current_adapter_handle, adap_sig, sizeof(adap_sig))) {
	} else {
		printf("Adapter signature: %s\n", adap_sig);
	}

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
		sscanf((char*)buf, "%d.%d.%d", &app->firmware_maj, &app->firmware_min, &app->firmware_build);
		gtk_label_set_text(label_firmware_version, (char*)buf);

	}

	snprintf((char*)buf, sizeof(buf), "%04x:%04x", info->usb_vid, info->usb_pid);
	gtk_label_set_text(label_usb_id, (char*)buf);

	gtk_label_set_text(label_device_path, info->str_path);

	periodic_updater(app);
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
		errorPopup(app, "Error setting configuration");
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
			errorPopup(app, "Error setting configuration");
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
			errorPopup(app, "Failed to open adapter");
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

static int mempak_io_progress_cb(int progress, void *ctx)
{
	struct application *app = ctx;
	GET_UI_ELEMENT(GtkProgressBar, mempak_io_progress);

	gtk_progress_bar_set_fraction(mempak_io_progress, progress/((gdouble)MEMPAK_MEM_SIZE));
	while (gtk_events_pending()) {
		gtk_main_iteration_do(FALSE);
	}

	return app->stop_mempak_io;
}

G_MODULE_EXPORT void mempak_io_stop(GtkWidget *wid, gpointer data)
{
	struct application *app = data;
	app->stop_mempak_io = 1;
}

G_MODULE_EXPORT void write_n64_pak(GtkWidget *wid, gpointer data)
{
	struct application *app = data;
	GET_UI_ELEMENT(GtkWindow, win_mempak_edit);
	GET_UI_ELEMENT(GtkDialog, mempak_io_dialog);
	GET_UI_ELEMENT(GtkLabel, mempak_op_label);
	GtkWidget *confirm_dialog;
	gint res;

	gtk_widget_show(GTK_WIDGET(win_mempak_edit));


	confirm_dialog = gtk_message_dialog_new(GTK_WINDOW(win_mempak_edit), GTK_DIALOG_MODAL,
									GTK_MESSAGE_QUESTION, 0, "Your memory card will be completely overwritten by the content of the memory pack editor.\n\nAre you sure?");

	gtk_dialog_add_buttons(GTK_DIALOG(confirm_dialog), "Cancel", 1, "Continue", 2, NULL);

	res = gtk_dialog_run(GTK_DIALOG(confirm_dialog));
	gtk_widget_destroy(confirm_dialog);

	switch(res)
	{
		case 2:
			printf("Confirmed write N64 mempak.\n");

			app->stop_mempak_io = 0;
			gtk_label_set_text(mempak_op_label, "Writing memory pack...");
			gtk_widget_show(GTK_WIDGET(mempak_io_dialog));

			res = gcn64lib_mempak_upload(app->current_adapter_handle, 0, mpke_getCurrentMempak(app), mempak_io_progress_cb, app);
			gtk_widget_hide(GTK_WIDGET(mempak_io_dialog));

			if (res != 0) {
				switch(res)
				{
					case -1: errorPopup(app, "No mempak detected"); break;
					case -2: errorPopup(app, "I/O error writing to mempak"); break;
					case -4: errorPopup(app, "Write aborted"); break;
					default:
						errorPopup(app, "Error writing to mempak"); break;
				}
			}

			break;
		case 1:
		default:
			printf("Write N64 mempak cancelled\n");
	}

}

G_MODULE_EXPORT void read_n64_pak(GtkWidget *wid, gpointer data)
{
	struct application *app = data;
	GET_UI_ELEMENT(GtkWindow, win_mempak_edit);
	GET_UI_ELEMENT(GtkDialog, mempak_io_dialog);
	GET_UI_ELEMENT(GtkLabel, mempak_op_label);
	mempak_structure_t *mpk;
	int res;

	printf("N64 read mempak\n");
	if (!app->current_adapter_handle)
		return;

	gtk_widget_show(GTK_WIDGET(mempak_io_dialog));
	gtk_label_set_text(mempak_op_label, "Reading memory pack...");

	app->stop_mempak_io = 0;
	res = gcn64lib_mempak_download(app->current_adapter_handle, 0, &mpk, mempak_io_progress_cb, app);

	gtk_widget_hide(GTK_WIDGET(mempak_io_dialog));
	if (res != 0) {
		switch(res)
		{
			case -1: errorPopup(app, "No mempak detected"); break;
			case -2: errorPopup(app, "I/O error reading mempak"); break;
			case -4: errorPopup(app, "Read aborted"); break;
			default:
			case -3: errorPopup(app, "Error reading mempak"); break;
		}
	}
	else {
		mpke_replaceMpk(app, mpk, NULL);
		gtk_widget_show(GTK_WIDGET(win_mempak_edit));
	}
}

G_MODULE_EXPORT void testVibration(GtkWidget *wid, gpointer data)
{
	struct application *app = data;
	GtkWidget *dialog;

	if ((app->firmware_maj < 3) || (app->firmware_min < 1)) {
		errorPopup(app, "Firmware 3.1 or later required");
	} else {
		if (0 > gcn64lib_forceVibration(app->current_adapter_handle, 0, 1)) {
			errorPopup(app, "Error setting vibration");
		} else {
			dialog = gtk_dialog_new_with_buttons("Vibration test", app->mainwindow, GTK_DIALOG_MODAL, "Stop vibration", GTK_RESPONSE_ACCEPT, NULL);
			gtk_dialog_run(GTK_DIALOG(dialog));
			gtk_widget_destroy(dialog);

			gcn64lib_forceVibration(app->current_adapter_handle, 0, 0);
		}
	}
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

	app.mpke = mpkedit_new(&app);

    /* Get main window pointer from UI */
    window = GTK_WINDOW( gtk_builder_get_object( app.builder, "mainWindow" ) );
	app.mainwindow = window;

    /* Connect signals */
    gtk_builder_connect_signals( app.builder, &app );

	g_timeout_add_seconds(1, periodic_updater, &app);

    /* Show window. All other widgets are automatically shown by GtkBuilder */
    gtk_widget_show( GTK_WIDGET(window) );

    /* Start main loop */
    gtk_main();

	mpkedit_free(app.mpke);

    return( 0 );
}
