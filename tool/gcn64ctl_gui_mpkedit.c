#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include "gcn64ctl_gui.h"
#include "mempak.h"

void mpke_syncModel(struct application *app);

struct mpkedit_data {
	struct mempak_structure *mpk;
	char *filename;
	int modified;
};

struct mpkedit_data *mpkedit_new(struct application *app)
{
	struct mpkedit_data *mpke;

	mpke = calloc(sizeof(struct mpkedit_data), 1);
	if (!mpke) {
		perror("calloc");
		return NULL;
	}

	mpke->mpk = mempak_new();
	if (!mpke->mpk) {
		free(mpke);
		return NULL;
	}

	return mpke;
}

void mpkedit_free(struct mpkedit_data *mpke)
{
	if (mpke) {
		free(mpke);
	}
}

int mpke_getSelection(struct application *app)
{
	GET_UI_ELEMENT(GtkTreeView, n64_notes_treeview);
	GtkTreeSelection *sel = gtk_tree_view_get_selection(n64_notes_treeview);
	GList *selected;
	int sel_id = -1;

	if (!sel) {
		return -1;
	}

	selected = gtk_tree_selection_get_selected_rows(sel, NULL);
	if (selected && selected->data) {
		GtkTreePath *path;
		gint *indices;

		path = (GtkTreePath*)selected->data;
		indices = gtk_tree_path_get_indices(path);
		sel_id = indices[0];

	} else {
		return -1;
	}

	if (selected)
		g_list_free_full(selected, (GDestroyNotify)gtk_tree_path_free);

	printf("Current selection: %d\n", sel_id);
	return sel_id;
}

void mpke_syncTitle(struct application *app)
{
	GET_UI_ELEMENT(GtkWindow, win_mempak_edit);
	char titlebuf[64];

	if (app->mpke->filename) {
		char *bn = g_path_get_basename(app->mpke->filename);

		snprintf(titlebuf, sizeof(titlebuf), "N64 Mempak editor - %s%s",
			bn,
			app->mpke->modified ? " [MODIFIED]":""
			);
		g_free(bn);
		printf("New title: %s\n", titlebuf);
		gtk_window_set_title(win_mempak_edit, titlebuf);
	} else {
		snprintf(titlebuf, sizeof(titlebuf), "N64 Mempak editor%s",
			app->mpke->modified ? " [NOT SAVED]" : "");
	}
}

void mpke_updateFilename(struct application *app, char *filename)
{
	if (app->mpke->filename) {
		// The filename always comes from gtk_file_chooser_get_filename
		g_free(app->mpke->filename);
	}

	app->mpke->filename = filename;
	mpke_syncTitle(app);
}

void mpke_replaceMpk(struct application *app, mempak_structure_t *mpk, char *filename)
{

	if (app->mpke->mpk) {
		mempak_free(app->mpke->mpk);
	}

	app->mpke->mpk = mpk;

	mpke_syncModel(app);
	mpke_updateFilename(app, filename);
}

void mpke_syncModel(struct application *app)
{
	GET_UI_ELEMENT(GtkListStore, n64_notes);
	GET_UI_ELEMENT(GtkTreeView, n64_notes_treeview);
	GET_UI_ELEMENT(GtkStatusbar, mempak_status_bar);
	int i, res;
	char statusbuf[64];

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
				gtk_list_store_set(n64_notes, &iter, 0, i, 1, note_data.name, 2, note_data.blocks, 3, app->mpke->mpk->note_comments[i], -1);
			} else {
				gtk_list_store_set(n64_notes, &iter, 0, i, -1);
			}
		}
	}

	gtk_tree_view_set_model(n64_notes_treeview, GTK_TREE_MODEL(n64_notes));

	snprintf(statusbuf, sizeof(statusbuf), "Blocks used: %d / %d", 123-get_mempak_free_space(app->mpke->mpk), 123);
	gtk_statusbar_push(mempak_status_bar, gtk_statusbar_get_context_id(mempak_status_bar, "free blocks"), statusbuf);

}

G_MODULE_EXPORT void mpke_export_note(GtkWidget *win, gpointer data)
{
	struct application *app = data;
	int selection;
	GtkWidget *dialog;
	GtkFileChooser *chooser;
	GET_UI_ELEMENT(GtkWindow, win_mempak_edit);
	GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_SAVE;
	GET_UI_ELEMENT(GtkFileFilter, n64_note_filter);
	int res;

	selection = mpke_getSelection(app);

	if (selection <0) {
		printf("No selection");
		return;
	}

	if (app->mpke->mpk) {
		entry_structure_t entry;
		if (0==get_mempak_entry(app->mpke->mpk, selection, &entry)) {
			char namebuf[64];
			if (!entry.valid) {
				errorPopup(app, "Please select a non-empty note");
				return;
			}

			dialog = gtk_file_chooser_dialog_new("Save File",
										win_mempak_edit,
										action,
										"_Cancel",
										GTK_RESPONSE_CANCEL,
										"_Save",
										GTK_RESPONSE_ACCEPT,
										NULL);
			chooser = GTK_FILE_CHOOSER(dialog);
			snprintf(namebuf, sizeof(namebuf), "%s.note", entry.name);
			gtk_file_chooser_set_current_name(chooser, namebuf);
			gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(dialog), n64_note_filter);

			res = gtk_dialog_run (GTK_DIALOG(dialog));
			if (res == GTK_RESPONSE_ACCEPT) {
				char *filename;

				filename = gtk_file_chooser_get_filename(chooser);
				if (mempak_exportNote(app->mpke->mpk, selection, filename)) {
					errorPopup(app, "Could not export note");
				} else {
					printf("Note saved to %s\n", filename);
				}
			}

			gtk_widget_destroy(dialog);
		}
	}
}

G_MODULE_EXPORT void mpke_insert_note(GtkWidget *win, gpointer data)
{
	struct application *app = data;

	GtkWidget *dialog;
	GET_UI_ELEMENT(GtkWindow, win_mempak_edit);
	GET_UI_ELEMENT(GtkFileFilter, n64_note_filter);

	GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
	int res;

	dialog = gtk_file_chooser_dialog_new("Load N64 mempak image",
										win_mempak_edit,
										action,
										"_Cancel",
										GTK_RESPONSE_CANCEL,
										"_Open",
										GTK_RESPONSE_ACCEPT,
										NULL);

	gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(dialog), n64_note_filter);
	res = gtk_dialog_run (GTK_DIALOG(dialog));

	if (res == GTK_RESPONSE_ACCEPT) {
		char *filename;
		GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
		entry_structure_t entry;
		int used_note_id;
		int dst_id;

		filename = gtk_file_chooser_get_filename(chooser);

		dst_id = mpke_getSelection(app);

		if (0 == get_mempak_entry(app->mpke->mpk, dst_id, &entry)) {
			if (entry.valid) {
				// Ask confirmation
				printf("Ask confirmation\n");
			}
		}

		res = mempak_importNote(app->mpke->mpk, filename, dst_id, &used_note_id);
		if (res) {
			switch(res)
			{
				default:
				case -1: errorPopup(app, "Error loading file or inserting note\n"); break;
				case -2: errorPopup(app, "Not enough free blocks to insert note\n"); break;
			}
		} else {
			// Success
			app->mpke->modified =1;
			mpke_syncModel(app);
			mpke_syncTitle(app);
		}
	}

	gtk_widget_destroy(dialog);

}

G_MODULE_EXPORT void mpke_new(GtkWidget *win, gpointer data)
{
	struct application *app = data;

	app->mpke->modified = 0;
	mpke_replaceMpk(app, mempak_new(), NULL);
}

G_MODULE_EXPORT void mpke_open(GtkWidget *win, gpointer data)
{
	struct application *app = data;
	GtkWidget *dialog;
	GET_UI_ELEMENT(GtkWindow, win_mempak_edit);
	GET_UI_ELEMENT(GtkFileFilter, n64_mempak_filter);
	GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
	int res;

	dialog = gtk_file_chooser_dialog_new("Load N64 mempak image",
										win_mempak_edit,
										action,
										"_Cancel",
										GTK_RESPONSE_CANCEL,
										"_Open",
										GTK_RESPONSE_ACCEPT,
										NULL);

	gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(dialog), n64_mempak_filter);
	res = gtk_dialog_run (GTK_DIALOG(dialog));
	if (res == GTK_RESPONSE_ACCEPT) {
		char *filename;
		GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
		mempak_structure_t *mpk;

		filename = gtk_file_chooser_get_filename(chooser);
		mpk = mempak_loadFromFile(filename);
		if (mpk) {
			app->mpke->modified = 0;
			mpke_replaceMpk(app, mpk, filename);
		} else {
			errorPopup(app, "Failed to load mempak");
		}
	}

	gtk_widget_destroy(dialog);
}

G_MODULE_EXPORT void mpke_saveas(GtkWidget *win, gpointer data)
{
	struct application *app = data;
	GtkWidget *dialog;
	GtkFileChooser *chooser;
	GET_UI_ELEMENT(GtkWindow, win_mempak_edit);
	GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_SAVE;
	GET_UI_ELEMENT(GtkFileFilter, n64_mempak_filter);
	int res;

	dialog = gtk_file_chooser_dialog_new("Save File",
										win_mempak_edit,
										action,
										"_Cancel",
										GTK_RESPONSE_CANCEL,
										"_Save",
										GTK_RESPONSE_ACCEPT,
										NULL);
	chooser = GTK_FILE_CHOOSER(dialog);

	gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(dialog), n64_mempak_filter);
	if (app->mpke->filename) {
		gchar *bs = g_path_get_basename(app->mpke->filename);
		gchar *dn = g_path_get_dirname(app->mpke->filename);
		gtk_file_chooser_set_current_name(chooser, bs);
		gtk_file_chooser_set_current_folder(chooser, dn);
		g_free(bs);
		g_free(dn);
	} else {
		gtk_file_chooser_set_current_name(chooser, "mempak.n64");
	}

	res = gtk_dialog_run (GTK_DIALOG(dialog));
	if (res == GTK_RESPONSE_ACCEPT) {
		char *filename;
		int fmt;

		filename = gtk_file_chooser_get_filename(chooser);
		fmt = mempak_getFilenameFormat(filename);
		if (fmt!= MPK_FORMAT_INVALID) {
			mempak_saveToFile(app->mpke->mpk, filename, fmt);
			printf("Saved to %s\n", filename);
			app->mpke->modified = 0;
			mpke_updateFilename(app,filename);
		} else {
			errorPopup(app, "Unknown file format specified");
		}
	}

	gtk_widget_destroy(dialog);
}

G_MODULE_EXPORT void mpke_save(GtkWidget *win, gpointer data)
{
	struct application *app = data;

	if (!app->mpke->mpk)
		return;

	if (!app->mpke->filename) {
		mpke_saveas(win, data);
	} else {
		mempak_saveToFile(app->mpke->mpk, app->mpke->filename, app->mpke->mpk->file_format);
		app->mpke->modified = 0;
		mpke_syncTitle(app);
	}
}

G_MODULE_EXPORT void mpke_delete(GtkWidget *win, gpointer data)
{
	struct application *app = data;
	int selection;

	selection = mpke_getSelection(app);

	if (selection <0) {
		printf("No selection");
		return;
	}

	if (app->mpke->mpk) {
		entry_structure_t entry;
		if (0==get_mempak_entry(app->mpke->mpk, selection, &entry)) {
			delete_mempak_entry(app->mpke->mpk, &entry);
			mpke_syncModel(app);
			app->mpke->modified = 1;
			mpke_syncTitle(app);
		}
	}
}

G_MODULE_EXPORT void onMempakWindowShow(GtkWidget *win, gpointer data)
{
	struct application *app = data;
	mpke_syncModel(app);
}

mempak_structure_t *mpke_getCurrentMempak(struct application *app)
{
	return app->mpke->mpk;
}

