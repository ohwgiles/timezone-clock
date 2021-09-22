#include "tzview.h"

struct _TzView {
	GtkDrawingArea drawing_area;
	char const* const* timezones;
	double scroll_pos;
};

static struct {
	GdkRGBA bg;
	GdkRGBA bg_night;
	GdkRGBA bg_day;
	GdkRGBA header_bg;
	GdkRGBA header_label_fg;
	GdkRGBA hour_label_fg;
	GdkRGBA day_label_fg;
	GdkRGBA column_separator;
	GdkRGBA row_separator;
	GdkRGBA dst_transition_bg;
	GdkRGBA dst_transition_label_fg;
} COLORS;

G_DEFINE_TYPE(TzView, tzview, GTK_TYPE_DRAWING_AREA);

#define HOUR_HEIGHT 32
#define HEADER_HEIGHT 40

static void do_draw(GtkDrawingArea *widget, cairo_t *cr, int width, int height, gpointer user_data)
{
	TzView* tzv = TZC_TZVIEW(widget);

	int n_timezones = 0;
	while(tzv->timezones && tzv->timezones[n_timezones])
		n_timezones++;

	if(n_timezones == 0)
		return;

	const int col_width = width / n_timezones;

	time_t begin_at = time(NULL) - (3600 * 24 * 2) + 60 * tzv->scroll_pos;

	// when a DST transition in Pacific/Auckland is visible
	//time_t begin_at = 1617414871 + 60 * (int) tzv->scroll_pos;

	// when DST transitions in America/Asuncion and Europe are visible
	//time_t begin_at = 1616846400 + 60 * (int) tzv->scroll_pos;

	cairo_set_line_width(cr, 1.0);

	// hour label font
	cairo_select_font_face(cr, "sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, 14);

	// column label layout and font
	PangoLayout* layout = pango_cairo_create_layout(cr);
	pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
	pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
	pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
	PangoFontDescription* font_desc = pango_font_description_from_string("sans 12");
	pango_layout_set_font_description(layout, font_desc);
	pango_font_description_free(font_desc);

	for(int col = 0; col < n_timezones; ++col) {
		time_t now = begin_at;

		setenv("TZ", tzv->timezones[col], 1);
		struct tm* st = localtime(&now);

		int hour = st->tm_hour;

		// horizontal hour lines
		for (int row = 0;; ++row) {
			double y = 0.5 + HEADER_HEIGHT + HOUR_HEIGHT * (row - (now % 3600)/3600.0);
			if (y > height)
				break;

			// background
			const GdkRGBA* hour_bg = (st->tm_hour >= 22 || st->tm_hour <= 5)
			                         ? &COLORS.bg_night
			                         : (st->tm_hour >= 8 && st->tm_hour <= 18)
			                         ? &COLORS.bg_day
			                         : &COLORS.bg;
			gdk_cairo_set_source_rgba(cr, hour_bg);
			cairo_rectangle(cr, col_width * col, y, col_width, HOUR_HEIGHT + 1);
			cairo_fill(cr);

			if (st->tm_hour != hour % 24) {
				// DST transition
				gdk_cairo_set_source_rgba(cr, &COLORS.dst_transition_bg);
				cairo_rectangle(cr, 0.5 + col_width * col + 50, y - HOUR_HEIGHT/2, col_width, HOUR_HEIGHT);
				//cairo_stroke(cr);
				cairo_fill(cr);

				char time_diff[6];
				snprintf(time_diff, 6, "%+d hr", st->tm_hour - hour);
				cairo_move_to(cr, 0.5 + col_width * (col + 0.5) + 6, y + 4);
				gdk_cairo_set_source_rgba(cr, &COLORS.dst_transition_label_fg);
				cairo_show_text(cr, time_diff);

				hour = st->tm_hour;
			}

			if (hour == 24) {
				// New day
				char date[16];
				strftime(date, 16, "%a %d %b", st);
				cairo_move_to(cr, 0.5 + col_width * (col + 0.5) - 14, y + 4);
				gdk_cairo_set_source_rgba(cr, &COLORS.day_label_fg);
				cairo_show_text(cr, date);
				hour = 0;
			}

			// draw hour labels
			gdk_cairo_set_source_rgba(cr, &COLORS.hour_label_fg);
			char hour_label[8];
			cairo_move_to(cr, 0.5 + col_width * col + 5, y + 4);
			sprintf(hour_label, "%02d:00", st->tm_hour);
			cairo_show_text(cr, hour_label);

			// next hour
			now += 3600;
			hour++;
			// inefficient to call this every iteration, but easiest way to catch DST transitions
			st = localtime(&now);
		}
		unsetenv("TZ");
	}

	// header bg
	gdk_cairo_set_source_rgba(cr, &COLORS.header_bg);
	cairo_rectangle(cr, 0, 0, width, HEADER_HEIGHT);
	cairo_fill(cr);

	// vertical separators between zones
	gdk_cairo_set_source_rgba(cr, &COLORS.column_separator);
	for (int col = 0; col < n_timezones; ++col) {
		cairo_move_to(cr, 0.5 + col_width * (col + 1), 0);
		cairo_rel_line_to(cr, 0, height);
		cairo_stroke(cr);
	}

	// column labels
	gdk_cairo_set_source_rgba(cr, &COLORS.header_label_fg);
	for (int col = 0; col < n_timezones; ++col) {
		pango_layout_set_width(layout, PANGO_SCALE * (col_width - 8));
		pango_layout_set_height(layout, PANGO_SCALE * (HEADER_HEIGHT - 2));
		pango_layout_set_text(layout, tzv->timezones[col], -1);

		cairo_move_to(cr, 0.5 + col_width * col, 0);
		pango_cairo_show_layout(cr, layout);
	}
	cairo_move_to(cr, 0, 0.5 + HEADER_HEIGHT);
	cairo_rel_line_to(cr, width, 0);
	cairo_stroke(cr);
}

static void tzview_class_init(TzViewClass* klass)
{
	gdk_rgba_parse(&COLORS.bg, "#e4e4df");
	gdk_rgba_parse(&COLORS.bg_day, "#fbfbfb");
	gdk_rgba_parse(&COLORS.bg_night, "#c3c3ce");
	gdk_rgba_parse(&COLORS.header_bg, "#f0f0f0");
	gdk_rgba_parse(&COLORS.header_label_fg, "#222222");
	gdk_rgba_parse(&COLORS.hour_label_fg, "#555555");
	gdk_rgba_parse(&COLORS.day_label_fg, "#222222");
	gdk_rgba_parse(&COLORS.column_separator, "#dfdfdf");
	gdk_rgba_parse(&COLORS.row_separator, "#dfdfdf");
	gdk_rgba_parse(&COLORS.dst_transition_bg, "#e1a1a1");
	gdk_rgba_parse(&COLORS.dst_transition_label_fg, "#f11111");
}

gboolean scroll (GtkEventControllerScroll* self,  gdouble dx, gdouble dy, gpointer user_data )
{
	TzView* tzv = TZC_TZVIEW(user_data);

	tzv->scroll_pos += 25 * dy;
	gtk_widget_queue_draw(GTK_WIDGET(tzv));
	return TRUE;
}

static void tzview_init(TzView* tzv)
{
	GtkEventController* evc = gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
	g_signal_connect(evc, "scroll", G_CALLBACK(scroll), tzv);

	gtk_widget_add_controller(GTK_WIDGET(tzv), evc);
	gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(tzv), do_draw, NULL, NULL);
}

void tzview_set_timezones(TzView* tzv, const char*const* timezone_list)
{
	tzv->timezones = timezone_list;
	gtk_widget_queue_draw(GTK_WIDGET(tzv));
}

GtkWidget* tzview_new(void)
{
	TzView* tzv = g_object_new(TZC_TYPE_TZVIEW, NULL);
	return (GtkWidget*) tzv;
}
