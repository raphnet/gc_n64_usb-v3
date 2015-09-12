#include <stdio.h>
#include <glib.h>
#include <gtk/gtk.h>
#include "../gcn64.h"
#include "../gcn64lib.h"
#include "../../requests.h"

#define GET_UI_ELEMENT(TYPE, ELEMENT)   TYPE *ELEMENT = (TYPE *) \
                                            gtk_builder_get_object(app->builder, #ELEMENT);

struct application {
	GtkBuilder *builder;
	gcn64_hdl_t current_adapter_handle;
};

static void updateGuiFromAdapter(struct application *app)
{
	unsigned char buf[32];
	int n;

	GtkSpinButton *pollInterval0 = GTK_SPIN_BUTTON( gtk_builder_get_object(app->builder, "pollInterval0") );

	n = gcn64lib_getConfig(app->current_adapter_handle, CFG_PARAM_POLL_INTERVAL0, buf, sizeof(buf));
	if (n == 1) {
		printf("poll interval: %d\n", buf[0]);
		gtk_spin_button_set_value(pollInterval0, (gdouble)buf[0]);
	}
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
//		gtk_list_store_set(list_store, &iter, 1, "asfasdfasdfasf", -1);
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

int
main( int    argc,
      char **argv )
{
    GtkWidget  *window;
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
    window = GTK_WIDGET( gtk_builder_get_object( app.builder, "mainWindow" ) );

    /* Connect signals */
    gtk_builder_connect_signals( app.builder, &app );

    /* Destroy builder, since we don't need it anymore */
//    g_object_unref( G_OBJECT( builder ) );

    /* Show window. All other widgets are automatically shown by GtkBuilder */
    gtk_widget_show( window );

    /* Start main loop */
    gtk_main();

    return( 0 );
}

