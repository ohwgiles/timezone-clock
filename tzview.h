#ifndef TZVIEW_H_
#define TZVIEW_H_

#include <gtk/gtk.h>

#define TZC_TYPE_TZVIEW (tzview_get_type())
G_DECLARE_FINAL_TYPE(TzView, tzview, TZC, TZVIEW, GtkDrawingArea)

GtkWidget* tzview_new(void);

void tzview_set_timezones(TzView* tzv, const char*const* timezone_list);

#endif // TZVIEW_H_
