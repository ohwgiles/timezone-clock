#include <gtk/gtk.h>

#include "settings.h"
#include "tzview.h"

#define TZC_TYPE_APP (tzc_app_get_type())
G_DECLARE_FINAL_TYPE(TzcApp, tzc_app, TZC, APP, GtkApplication)

struct _TzcApp {
	GtkApplication parent;
	GtkWidget *mainWindow;
	GtkWidget *headerBar;
	GtkWidget *tzv;
};

G_DEFINE_TYPE(TzcApp, tzc_app, GTK_TYPE_APPLICATION)

static void on_settings_changed(gpointer user_data)
{
	TzcApp *tzc = TZC_APP(user_data);
	tzview_set_timezones(TZC_TZVIEW(tzc->tzv), settings_get_timezones());
}

static void open_settings_dialog(GSimpleAction *simple, GVariant *parameter, gpointer user_data)
{
	TzcApp *tzc = TZC_APP(user_data);
	settings_dialog_run(GTK_WINDOW(tzc->mainWindow), on_settings_changed, tzc);
}

static void tzc_startup(GApplication *app)
{
	TzcApp *tzc = TZC_APP(app);
	tzc->mainWindow = gtk_application_window_new(GTK_APPLICATION(app));

	/* Header bar */
	tzc->headerBar = gtk_header_bar_new();
	gtk_header_bar_set_show_title_buttons(GTK_HEADER_BAR(tzc->headerBar), TRUE);

	GtkWidget *btn_menu = gtk_menu_button_new();
	gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(btn_menu), "open-menu-symbolic");
	GMenu *menu_main = g_menu_new();
	g_menu_append(menu_main, "Preferences", "win.settings");

	const GActionEntry menu_entries[] = {
		{"settings", open_settings_dialog}};
	g_action_map_add_action_entries(G_ACTION_MAP(tzc->mainWindow), menu_entries, G_N_ELEMENTS(menu_entries), tzc);

	GtkWidget *popup_menu = gtk_popover_menu_new_from_model(G_MENU_MODEL(menu_main));
	gtk_menu_button_set_popover(GTK_MENU_BUTTON(btn_menu), popup_menu);

	gtk_header_bar_pack_end(GTK_HEADER_BAR(tzc->headerBar), btn_menu);
	gtk_window_set_titlebar(GTK_WINDOW(tzc->mainWindow), tzc->headerBar);

	/* Main Timezone View Widget */
	tzc->tzv = tzview_new();
	tzview_set_timezones(TZC_TZVIEW(tzc->tzv), settings_get_timezones());
	gtk_window_set_child(GTK_WINDOW(tzc->mainWindow), tzc->tzv);

	gtk_window_set_title(GTK_WINDOW(tzc->mainWindow), "Timezone Clock");
	gtk_window_set_default_size(GTK_WINDOW(tzc->mainWindow), 780, 630);

	gtk_widget_show(tzc->mainWindow);
}

static void tzc_app_class_init(TzcAppClass *klass)
{
}

static void tzc_app_init(TzcApp *tzc)
{
}

int main(int argc, char **argv)
{
	int status;

	GtkApplication *app = g_object_new(TZC_TYPE_APP, "application-id", "net.ohwg.tzc", NULL);
	g_signal_connect(app, "activate", G_CALLBACK(tzc_startup), NULL);
	g_application_register(G_APPLICATION(app), NULL, NULL);

	status = g_application_run(G_APPLICATION(app), argc, argv);
	g_object_unref(app);

	settings_cleanup();

	return status;
}
