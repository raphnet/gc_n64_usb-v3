#include <stdio.h>
#include <stdlib.h>
#include "gcn64ctl_gui.h"
#include "mempak.h"

void mpke_syncModel(struct application *app);

struct mpkedit_data {
	struct mempak_structure *mpk;
};

struct mpkedit_data *mpkedit_new(struct application *app)
{
	struct mpkedit_data *mpke;

	mpke = calloc(sizeof(struct mpkedit_data), 1);
	if (!mpke) {
		perror("calloc");
		return NULL;
	}

//	mpke->mpk = mempak_new();
	mpke->mpk = mempak_loadFromFile("/home/raph/0Dream/programming/multiuse_pcbX/gc_n64_usb-v3/tmp/perfect_dark_b.N64");


	return mpke;
}

void mpkedit_free(struct mpkedit_data *mpke)
{
	if (mpke) {
		free(mpke);
	}
}

void mpke_syncModel(struct application *app)
{
	GET_UI_ELEMENT(GtkListStore, n64_notes);
	GET_UI_ELEMENT(GtkTreeView, n64_notes_treeview);
	int i, res;

	gtk_list_store_clear(n64_notes);
	if (!app->mpke->mpk) {
		return;
	}

	for (i=0; i<16; i++) {
		GtkTreeIter iter;
		entry_structure_t note_data;

		gtk_list_store_append(n64_notes, &iter);

		res = get_mempak_entry(app->mpke->mpk, i, &note_data);
		if (res) {
			gtk_list_store_set(n64_notes, &iter, 0, i, 1, "!!ERROR!!", 2, 0, -1);
		} else {
			if (note_data.valid) {
				gtk_list_store_set(n64_notes, &iter, 0, i, 1, note_data.name, 2, note_data.blocks, -1);
			} else {
				gtk_list_store_set(n64_notes, &iter, 0, i, -1);
			}
		}
	}

	gtk_tree_view_set_model(n64_notes_treeview, GTK_TREE_MODEL(n64_notes));

}

G_MODULE_EXPORT void mpke_export_note(GtkWidget *win, gpointer data)
{
}

G_MODULE_EXPORT void mpke_insert_note(GtkWidget *win, gpointer data)
{
}

G_MODULE_EXPORT void mpke_new(GtkWidget *win, gpointer data)
{
	struct application *app = data;

	mempak_free(app->mpke->mpk);
	app->mpke->mpk = mempak_new();

	mpke_syncModel(app);
}

G_MODULE_EXPORT void mpke_open(GtkWidget *win, gpointer data)
{
}

G_MODULE_EXPORT void mpke_saveas(GtkWidget *win, gpointer data)
{
}

G_MODULE_EXPORT void mpke_save(GtkWidget *win, gpointer data)
{
}

G_MODULE_EXPORT void mpke_delete(GtkWidget *win, gpointer data)
{
}

G_MODULE_EXPORT void onMempakWindowShow(GtkWidget *win, gpointer data)
{
	struct application *app = data;
	mpke_syncModel(app);
}
