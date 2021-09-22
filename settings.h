#ifndef SETTINGS_H_
#define SETTINGS_H_

#include <gtk/gtk.h>

typedef void (*SettingsUpdatedCallback)(gpointer user_data);

const char* const* settings_get_timezones(void);

void settings_dialog_run(GtkWindow* parent, SettingsUpdatedCallback callback, gpointer user_data);

void settings_cleanup(void);

#endif //SETTINGS_H_
