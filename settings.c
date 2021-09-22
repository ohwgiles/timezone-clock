#define _GNU_SOURCE
#include "settings.h"

#include "olsen-names.h"

struct Settings {
	char **timezones;
};

static struct Settings* settings = NULL;

static void settings_load(void)
{
	if (settings)
		return;

	settings = g_new(struct Settings, 1);

	char *path_dir = g_strdup_printf("%s/timezone-clock", g_get_user_config_dir());
	g_mkdir_with_parents(path_dir, 0755);
	char *path = g_strdup_printf("%s/preferences.conf", path_dir);
	g_free(path_dir);

	GKeyFile *kf = g_key_file_new();
	GError *err = NULL;
	g_key_file_load_from_file(kf, path, G_KEY_FILE_KEEP_COMMENTS, NULL);

	char *cfg_tzs = g_key_file_get_string(kf, "general", "timezones", &err);
	if (!cfg_tzs)
		cfg_tzs = g_strdup("UTC");
	settings->timezones = g_strsplit(cfg_tzs, ",", -1);
	g_free(cfg_tzs);

	g_key_file_free(kf);
	g_free(path);
}

static void settings_save(void)
{
	char *path = g_strdup_printf("%s/timezone-clock/preferences.conf", g_get_user_config_dir());

	GKeyFile *kf = g_key_file_new();
	GError *err = NULL;
	g_key_file_load_from_file(kf, path, G_KEY_FILE_KEEP_COMMENTS, NULL);

	char *tznames = g_strjoinv(",", settings->timezones);
	g_key_file_set_string(kf, "general", "timezones", tznames);
	free(tznames);

	g_key_file_save_to_file(kf, path, &err);
	g_free(path);
	g_key_file_free(kf);
}

typedef struct {
	GtkWidget *dialog;
	GtkWidget *timezone_list;
	GtkWidget *new_tz_combo;
	SettingsUpdatedCallback callback;
	gpointer user_data;
} SettingsDialog;

static void on_clicked_timezone_delete(SettingsDialog *sd)
{
	GtkListBoxRow *selected = gtk_list_box_get_selected_row(GTK_LIST_BOX(sd->timezone_list));
	if (selected) {
		gtk_list_box_remove(GTK_LIST_BOX(sd->timezone_list), GTK_WIDGET(selected));
	}
}

static void on_settings_dialog_response(GtkDialog* self, gint response_id, gpointer user_data)
{
	SettingsDialog *sd = (SettingsDialog *) user_data;
	if (response_id == GTK_RESPONSE_OK) {
		char *tznames = g_strdup("");
		GtkListBoxRow *row;

		for (int i = 0; (row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(sd->timezone_list), i)); i++) {
			const char *txt = gtk_label_get_text(GTK_LABEL(gtk_list_box_row_get_child(row)));

			char *tmp = tznames;
			tznames = g_strdup_printf("%s%s%s", tmp, *tmp ? "," : "", txt);
			g_free(tmp);
		}
		char **tmp = settings->timezones;
		settings->timezones = g_strsplit(tznames, ",", -1);
		g_strfreev(tmp);
		settings_save();
	}
	gtk_window_destroy(GTK_WINDOW(sd->dialog));
	(*sd->callback)(sd->user_data);
	g_free(sd);
}

void do_add_timezone(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	SettingsDialog *sd = (SettingsDialog *) user_data;
	gtk_list_box_append(GTK_LIST_BOX(sd->timezone_list), gtk_label_new(g_action_get_name(G_ACTION(action))));
}

static GMenu* olsen_zones_to_gmenu(const char* stem, int stem_len, int *olsen_list_index)
{
	GMenu *menu = g_menu_new();
	for (; *olsen_list_index < OLSEN_NAMES_N && strncmp(stem, OLSEN_NAMES[*olsen_list_index], stem_len) == 0;) {
		const char *entry = OLSEN_NAMES[*olsen_list_index];
		const char *slash = strchr(entry + stem_len, '/');
		if (slash) {
			char *name = g_strdup_printf("%.*s", (int) (slash - entry - stem_len), entry + stem_len);
			GMenu *submenu = olsen_zones_to_gmenu(entry, slash - entry + 1, olsen_list_index);
			g_menu_append_submenu(menu, name, G_MENU_MODEL(submenu));
			g_free(name);
		} else {
			char *action = g_strdup_printf("AddTimezone.%s", entry);
			g_menu_append(menu, entry + stem_len, action);
			g_free(action);
			(*olsen_list_index)++;
		}
	}
	return menu;
}

void settings_dialog_run(GtkWindow*parent, SettingsUpdatedCallback callback, gpointer user_data)
{
	SettingsDialog *sd = g_new0(SettingsDialog, 1);
	sd->callback = callback;
	sd->user_data = user_data;

	sd->dialog = gtk_message_dialog_new(parent, GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_USE_HEADER_BAR, GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL, "Preferences");

	gtk_window_set_modal(GTK_WINDOW(sd->dialog), TRUE);

	GtkWidget *header = gtk_header_bar_new();
	gtk_header_bar_set_show_title_buttons(GTK_HEADER_BAR(header), TRUE);

	GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(sd->dialog));
	g_object_set(content_area, "margin", 10, NULL);

	GtkWidget *win = gtk_scrolled_window_new();
	gtk_scrolled_window_set_min_content_width(GTK_SCROLLED_WINDOW(win), 250);
	gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(win), 150);
	gtk_widget_set_vexpand(win, TRUE);
	sd->timezone_list = gtk_list_box_new();

	for (const char *const *t = settings_get_timezones(); *t; t++) {
		gtk_list_box_append(GTK_LIST_BOX(sd->timezone_list), gtk_label_new(*t));
	}
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(win), sd->timezone_list);

	GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_style_context_add_class(gtk_widget_get_style_context(toolbar), "toolbar");

	GtkWidget *btn = gtk_menu_button_new();
	gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(btn), "list-add-symbolic");

	int idx = 0;
	GMenu *menu = olsen_zones_to_gmenu("", 0, &idx);
	GSimpleActionGroup *sag = g_simple_action_group_new();
	GActionEntry *action_entries = g_new0(GActionEntry, OLSEN_NAMES_N);
	for (int i = 0; i < OLSEN_NAMES_N; ++i) {
		action_entries[i].name = OLSEN_NAMES[i];
		action_entries[i].activate = do_add_timezone;
	}
	g_action_map_add_action_entries(G_ACTION_MAP(sag), action_entries, OLSEN_NAMES_N, sd);
	g_free(action_entries);
	gtk_widget_insert_action_group(btn, "AddTimezone", G_ACTION_GROUP(sag));
	gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(btn), G_MENU_MODEL(menu));
	gtk_box_append(GTK_BOX(toolbar), btn);

	btn = gtk_button_new_from_icon_name("list-remove-symbolic");
	g_signal_connect_swapped(btn, "clicked", (GCallback) on_clicked_timezone_delete, sd);
	gtk_box_append(GTK_BOX(toolbar), btn);

	gtk_box_append(GTK_BOX(content_area), win);
	gtk_box_append(GTK_BOX(content_area), toolbar);

	gtk_widget_show(sd->dialog);
	g_signal_connect(sd->dialog, "response", G_CALLBACK(on_settings_dialog_response), sd);
}

const char*const* settings_get_timezones()
{
	settings_load();

	return (const char *const *) settings->timezones;
}

void settings_cleanup()
{
	g_strfreev(settings->timezones);
	g_free(settings);
	settings = NULL;
}
