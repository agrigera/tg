/*
    tg
    Copyright (C) 2015 Marcello Mamino

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2 as
    published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "tg.h"

/* Colour semantics, the same in every panel:
 *   tic = grey, toc = gold, amplitude = magenta, rate/reference = azure,
 *   axes and grids = white (solid = major, dotted = minor), error = red,
 *   status OK (watch icon, calibration done) = green.
 * A stale reading keeps its colour but is drawn faded. */
cairo_pattern_t *black,*white,*red,*green;

struct color { double r, g, b; };
static const struct color COL_TIC  = {.722, .722, .722};
static const struct color COL_TOC  = {.980, .761, .020};
static const struct color COL_AMP  = {1, .310, .816}; // magenta #FF4FD0
static const struct color COL_RATE = {.302, .651, 1};
static const struct color COL_WAVE = {.902, .902, .902}; // neutral, for both beats
static const struct color COL_AXIS = {1, 1, 1};

#define ALPHA_STALE       0.4
#define ALPHA_AXIS_MAJOR  0.55
#define ALPHA_AXIS_MINOR  0.25
#define ALPHA_BAND        0.18	// tic / toc windows in the period panel

static void define_color(cairo_pattern_t **gc,double r,double g,double b)
{
	*gc = cairo_pattern_create_rgb(r,g,b);
}

void initialize_palette()
{
	define_color(&black,0,0,0);
	define_color(&white,1,1,1);
	define_color(&red,1,.231,.188);
	define_color(&green,0,0.8,0);
}

static void set_color(cairo_t *c, struct color col, double alpha)
{
	cairo_set_source_rgba(c, col.r, col.g, col.b, alpha);
}

/* Colour of measured data: faded when the reading is stale */
static void set_signal_color(cairo_t *c, struct color col, int stale)
{
	set_color(c, col, stale ? ALPHA_STALE : 1);
}

static void set_dotted(cairo_t *c, int dotted)
{
	static const double dots[2] = {2, 4};
	if(dotted)
		cairo_set_dash(c, dots, 2, 0);
	else
		cairo_set_dash(c, NULL, 0, 0);
}

/* Line style of an axis or grid: major = solid, minor = dotted and fainter.
 * Call set_dotted(c, 0) after stroking to go back to solid lines. */
static void set_axis_style(cairo_t *c, struct color col, int major)
{
	set_color(c, col, major ? ALPHA_AXIS_MAJOR : ALPHA_AXIS_MINOR);
	set_dotted(c, !major);
}

static void draw_graph(double a, double b, cairo_t *c, struct processing_buffers *p, GtkWidget *da)
{
	GtkAllocation temp;
	gtk_widget_get_allocation (da, &temp);
	int width = temp.width;
	int height = temp.height;

	int n;

	int first = 1;
	for(n=0; n<2*width; n++) {
		int i = n < width ? n : 2*width - 1 - n;
		double x = fmod(a + i * (b-a) / width, p->period);
		if(x < 0) x += p->period;
		int j = floor(x);
		double y;

		if(p->waveform[j] <= 0) y = 0;
		else y = p->waveform[j] * 0.4 / p->waveform_max;

		int k = round(y*height);
		if(n < width) k = -k;
		if(first) {
			cairo_move_to(c,i+.5,height/2+k+.5);
			first = 0;
		} else
			cairo_line_to(c,i+.5,height/2+k+.5);
	}
}
#ifdef DEBUG
static void draw_debug_graph(double a, double b, cairo_t *c, struct processing_buffers *p, GtkWidget *da)
{
	if(!p->debug) return;

	GtkAllocation temp;
	gtk_widget_get_allocation (da, &temp);
	int width = temp.width;
	int height = temp.height;

	int i;
	float max = 0;

	int ai = round(a);
	int bi = 1+round(b);
	if(ai < 0) ai = 0;
	if(bi > p->sample_count) bi = p->sample_count;
	for(i=ai; i<bi; i++)
		if(p->debug[i] > max)
			max = p->debug[i];

	int first = 1;
	for(i=0; i<width; i++) {
		if( round(a + i*(b-a)/width) != round(a + (i+1)*(b-a)/width) ) {
			int j = round(a + i*(b-a)/width);
			if(j < 0) j = 0;
			if(j >= p->sample_count) j = p->sample_count-1;

			int k = round((0.1+p->debug[j]/max)*0.8*height);

			if(first) {
				cairo_move_to(c,i+.5,height-k-.5);
				first = 0;
			} else
				cairo_line_to(c,i+.5,height-k-.5);
		}
	}
}
#endif

static double amplitude_to_time(double lift_angle, double amp)
{
	return asin(lift_angle / (2 * amp)) / M_PI;
}

static double draw_watch_icon(cairo_t *c, int signal, int happy, int light)
{
	happy = !!happy;
	cairo_set_line_width(c,3);
	cairo_set_source(c,happy?green:red);
	cairo_move_to(c, OUTPUT_WINDOW_HEIGHT * 0.5, OUTPUT_WINDOW_HEIGHT * 0.5);
	cairo_line_to(c, OUTPUT_WINDOW_HEIGHT * 0.75, OUTPUT_WINDOW_HEIGHT * (0.75 - 0.5*happy));
	cairo_move_to(c, OUTPUT_WINDOW_HEIGHT * 0.5, OUTPUT_WINDOW_HEIGHT * 0.5);
	cairo_line_to(c, OUTPUT_WINDOW_HEIGHT * 0.35, OUTPUT_WINDOW_HEIGHT * (0.65 - 0.3*happy));
	cairo_stroke(c);
	cairo_arc(c, OUTPUT_WINDOW_HEIGHT * 0.5, OUTPUT_WINDOW_HEIGHT * 0.5, OUTPUT_WINDOW_HEIGHT * 0.4, 0, 2*M_PI);
	cairo_stroke(c);
	int l = OUTPUT_WINDOW_HEIGHT * 0.8 / (2*NSTEPS - 1);
	int i;
	cairo_set_line_width(c,1);
	for(i = 0; i < signal; i++) {
		cairo_move_to(c, OUTPUT_WINDOW_HEIGHT + 0.5*l, OUTPUT_WINDOW_HEIGHT * 0.9 - 2*i*l);
		cairo_line_to(c, OUTPUT_WINDOW_HEIGHT + 1.5*l, OUTPUT_WINDOW_HEIGHT * 0.9 - 2*i*l);
		cairo_line_to(c, OUTPUT_WINDOW_HEIGHT + 1.5*l, OUTPUT_WINDOW_HEIGHT * 0.9 - (2*i+1)*l);
		cairo_line_to(c, OUTPUT_WINDOW_HEIGHT + 0.5*l, OUTPUT_WINDOW_HEIGHT * 0.9 - (2*i+1)*l);
		cairo_line_to(c, OUTPUT_WINDOW_HEIGHT + 0.5*l, OUTPUT_WINDOW_HEIGHT * 0.9 - 2*i*l);
		cairo_stroke_preserve(c);
		cairo_fill(c);
	}
	if(light) {
		int l = OUTPUT_WINDOW_HEIGHT * 0.15;
		cairo_set_line_width(c,2);
		cairo_move_to(c, OUTPUT_WINDOW_HEIGHT * 0.5 - 0.5*l, OUTPUT_WINDOW_HEIGHT * 0.2);
		cairo_line_to(c, OUTPUT_WINDOW_HEIGHT * 0.5 - 0.5*l, OUTPUT_WINDOW_HEIGHT * 0.2 + l);
		cairo_line_to(c, OUTPUT_WINDOW_HEIGHT * 0.5        , OUTPUT_WINDOW_HEIGHT * 0.2 + l);
		cairo_move_to(c, OUTPUT_WINDOW_HEIGHT * 0.5 - 0.2*l, OUTPUT_WINDOW_HEIGHT * 0.2 + 1);
		cairo_line_to(c, OUTPUT_WINDOW_HEIGHT * 0.5 + 0.8*l, OUTPUT_WINDOW_HEIGHT * 0.2 + 1);
		cairo_move_to(c, OUTPUT_WINDOW_HEIGHT * 0.5 + 0.3*l, OUTPUT_WINDOW_HEIGHT * 0.2 + 1);
		cairo_line_to(c, OUTPUT_WINDOW_HEIGHT * 0.5 + 0.3*l, OUTPUT_WINDOW_HEIGHT * 0.2 + l + 1);
		cairo_stroke(c);
	}
	return OUTPUT_WINDOW_HEIGHT + 3*l;
}

static enum audio_input_status get_input_status(struct snapshot *snst)
{
	if(snst->pb && !snst->is_old)
		return AUDIO_STATUS_OK;
	return snst->bph ? AUDIO_STATUS_BPH_UNCONFIRMED : AUDIO_STATUS_BPH_UNKNOWN;
}

static void update_audio_presence(struct output_panel *op)
{
	gint64 now = g_get_monotonic_time();
	if(op->audio_meter_time && now - op->audio_meter_time < 500000)
		return;
	op->audio_meter_time = now;
	op->audio_meter_level = 0.6f * op->audio_meter_level + 0.4f * get_audio_level();

	if(!op->audio_meter_active) {
		if(op->audio_meter_level >= 0.008f)
			op->audio_meter_transition_count++;
		else
			op->audio_meter_transition_count = 0;
		if(op->audio_meter_transition_count >= 2) {
			op->audio_meter_active = 1;
			op->audio_meter_transition_count = 0;
		}
	} else {
		if(op->audio_meter_level <= 0.004f)
			op->audio_meter_transition_count++;
		else
			op->audio_meter_transition_count = 0;
		if(op->audio_meter_transition_count >= 3) {
			op->audio_meter_active = 0;
			op->audio_meter_transition_count = 0;
		}
	}
}

static double draw_input_status(cairo_t *c, double x, enum audio_input_status status)
{
	static const struct color status_colors[] = {
		{.55, .55, .55},
		{1, .72, .12},
		{1, .31, .25},
		{.15, .85, .35}
	};
	static const char *status_marks[] = {"0", "?", "!", ""};
	set_color(c, status_colors[status], 1);
	cairo_set_font_size(c, OUTPUT_WINDOW_HEIGHT * 0.28);
	cairo_move_to(c, x, OUTPUT_WINDOW_HEIGHT * 0.76);
	cairo_show_text(c, status_marks[status]);
	return x + 24;
}

static void cairo_init(cairo_t *c)
{
	cairo_set_line_width(c,1);

	cairo_set_source(c,black);
	cairo_paint(c);
}

static double print_s(cairo_t *c, double x, double y, char *s)
{
	cairo_text_extents_t extents;
	cairo_move_to(c,x,y);
	cairo_show_text(c,s);
	cairo_text_extents(c,s,&extents);
	x += extents.x_advance;
	return x;
}

static double print_number(cairo_t *c, double x, double y, char *s)
{
	cairo_text_extents_t extents;
	cairo_text_extents(c,"0",&extents);
	double z = extents.x_advance;
	char t[2];
	t[1] = 0;
	while((t[0] = *s++)) {
		cairo_text_extents(c,t,&extents);
		cairo_move_to(c, x + (z - extents.x_advance) / 2, y);
		cairo_show_text(c,t);
		x += z;
	}
	return x;
}

/* Two-row results table, like a hardware timegrapher: column titles on top,
 * values below. The columns share the width to the right of the watch icon. */
#define RESULT_COLUMNS 5
#define RESULT_TITLE_ROW  0.22	// vertical centre of each row, as a fraction of the height
#define RESULT_LINE       0.44
#define RESULT_VALUE_ROW  0.72

static void draw_results_table(cairo_t *c, double x0, double width, double height,
		struct snapshot *snst, struct processing_buffers *p, int show_candidate_bph)
{
	static const char *titles[RESULT_COLUMNS] = {"RATE", "AMP.", "B.E.", "L.A.", "BEAT"};
	char num[RESULT_COLUMNS][16];
	const char *unit[RESULT_COLUMNS];
	cairo_text_extents_t e;
	int i;

	if(width <= 0) return;

	// Digits and unit are kept apart: digits go in fixed-width cells so that
	// the values do not jitter, the unit follows them.
	if(p) {
		int rate = round(snst->rate);
		snprintf(num[0], sizeof(num[0]), "%s%d", rate > 0 ? "+" : rate < 0 ? "-" : "", abs(rate));
		unit[0] = "s/d";
		if(snst->amp > 0) {
			snprintf(num[1], sizeof(num[1]), "%.0f", snst->amp);
			unit[1] = "\xC2\xB0";
		} else {
			strcpy(num[1], "---");
			unit[1] = "";
		}
		snprintf(num[2], sizeof(num[2]), "%.1f", snst->be);
		unit[2] = "ms";
	} else {
		for(i = 0; i < 3; i++) {
			strcpy(num[i], "---");
			unit[i] = "";
		}
	}
	snprintf(num[3], sizeof(num[3]), "%.0f", snst->la); // whole degrees only
	unit[3] = "\xC2\xB0";
	if(show_candidate_bph && snst->candidate_bph && snst->bph && snst->is_old)
		snprintf(num[4], sizeof(num[4]), "~%d", snst->candidate_bph);
	else
		snprintf(num[4], sizeof(num[4]), "%d", snst->guessed_bph);
	unit[4] = "";

	// One font size for all values, reduced when the window is too narrow
	double col_w = width / RESULT_COLUMNS;
	double fv = OUTPUT_FONT * 0.85;
	double cell, widest = 0;
	cairo_set_font_size(c, fv);
	cairo_text_extents(c, "0", &e);
	cell = e.x_advance;
	for(i = 0; i < RESULT_COLUMNS; i++) {
		cairo_text_extents(c, unit[i], &e);
		double wd = cell * strlen(num[i]) + e.x_advance;
		if(wd > widest) widest = wd;
	}
	if(widest > col_w * 0.92) {
		fv *= col_w * 0.92 / widest;
		cairo_set_font_size(c, fv);
		cairo_text_extents(c, "0", &e);
		cell = e.x_advance;
	}

	// Column titles
	cairo_save(c);
	cairo_select_font_face(c, "sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size(c, fv * 0.5);
	cairo_text_extents(c, "RATE", &e);
	double y_title = RESULT_TITLE_ROW * height - e.y_bearing - e.height / 2;
	cairo_set_source(c, white);
	for(i = 0; i < RESULT_COLUMNS; i++) {
		cairo_text_extents(c, titles[i], &e);
		cairo_move_to(c, x0 + col_w * (i + 0.5) - e.x_advance / 2, y_title);
		cairo_show_text(c, titles[i]);
	}
	cairo_restore(c);

	// Line between titles and values
	cairo_set_source_rgb(c, .55, .55, .55);
	cairo_set_line_width(c, 1);
	cairo_move_to(c, x0, round(RESULT_LINE * height) + .5);
	cairo_line_to(c, x0 + width, round(RESULT_LINE * height) + .5);
	cairo_stroke(c);

	// Values: rate in the rate colour, amplitude in the amplitude colour, the
	// rest white. Measured values are faded while the reading is stale.
	cairo_set_font_size(c, fv);
	cairo_text_extents(c, "0", &e);
	double y_value = RESULT_VALUE_ROW * height - e.y_bearing - e.height / 2;
	for(i = 0; i < RESULT_COLUMNS; i++) {
		if(i < 3 && !p)
			set_color(c, COL_AXIS, ALPHA_STALE);
		else if(i == 0)
			set_signal_color(c, COL_RATE, snst->is_old);
		else if(i == 1)
			set_signal_color(c, COL_AMP, snst->is_old);
		else if(i == 2)
			set_signal_color(c, COL_AXIS, snst->is_old);
		else
			set_color(c, COL_AXIS, 1);
		cairo_text_extents(c, unit[i], &e);
		double digits = cell * strlen(num[i]);
		double x = x0 + col_w * (i + 0.5) - (digits + e.x_advance) / 2;
		x = print_number(c, x, y_value, num[i]);
		print_s(c, x, y_value, (char *)unit[i]);
	}
}

static gboolean output_draw_event(GtkWidget *widget, cairo_t *c, struct output_panel *op)
{
	cairo_init(c);

	struct snapshot *snst = op->snst;
	struct processing_buffers *p = snst->pb;
	int width = gtk_widget_get_allocated_width(widget);
	int height = gtk_widget_get_allocated_height(widget);

	double x = draw_watch_icon(c,snst->signal,snst->calibrate ? snst->signal==NSTEPS : snst->signal, snst->is_light);
	int show_audio_status = 0;
	enum audio_input_status status = AUDIO_STATUS_UNAVAILABLE;
	if(op->computer && !snst->calibrate) {
		update_audio_presence(op);
		status = op->audio_meter_active ? get_input_status(snst) : AUDIO_STATUS_NO_INPUT;
		snst->audio_status = status;
		show_audio_status = 1;
	} else if(!op->computer && snst->audio_status >= AUDIO_STATUS_NO_INPUT &&
		snst->audio_status <= AUDIO_STATUS_OK) {
		status = snst->audio_status;
		show_audio_status = 1;
	}
	if(show_audio_status) {
		const char *message[] = {
			"0: no audio input. The four squares by the clock show analysis windows with a valid period.",
			"?: audio is present, but no beat rate has been recognized yet. The four squares show analysis windows with a valid period.",
			"!: no valid rhythm at the selected beat rate. A free estimate, when available, is marked with ~ in the BEAT column. The four squares show analysis windows with a valid period.",
			"Audio input and a valid beat period are detected. The four squares show analysis windows with a valid period."
		};
		x = draw_input_status(c, x + 3, status);
		if(op->audio_status != status) {
			gtk_widget_set_tooltip_text(widget, message[status]);
			op->audio_status = status;
		}
	}

	cairo_text_extents_t extents;

	cairo_set_font_size(c, OUTPUT_FONT);
	cairo_text_extents(c,"0",&extents);
	double y = (double)OUTPUT_WINDOW_HEIGHT/2 - extents.y_bearing - extents.height/2;

	if(snst->calibrate) {
		cairo_set_source(c, white);
		x = print_s(c,x,y,"cal");
		cairo_set_font_size(c, OUTPUT_FONT*2/3);
		x = print_s(c,x,y," (");
		cairo_move_to(c,x,y);
		{
			double a = 0;
			char *s[] = {"wait", "acq.", "done", "fail", NULL}, **t = s;
			for(;*t;t++) {
				cairo_text_extents(c,*t,&extents);
				if(a < extents.x_advance) a = extents.x_advance;
			}
			x += a;
		}
		switch(snst->cal_state) {
			case 1:
				cairo_set_source(c,green);
				cairo_show_text(c,"done");
				break;
			case 0:
				if(snst->signal == NSTEPS)
					cairo_set_source(c, white);
				else
					set_color(c, COL_AXIS, ALPHA_STALE);
				cairo_show_text(c, snst->signal == NSTEPS ? "acq." : "wait");
				break;
			case -1:
				cairo_set_source(c,red);
				cairo_show_text(c,"fail");
				break;
		}
		cairo_set_source(c, white);
		x = print_s(c,x,y,")");
		cairo_set_font_size(c, OUTPUT_FONT);
		char s[20];
		switch(snst->cal_state) {
			case 1:
				sprintf(s, " %s%d.%d",
						snst->cal_result < 0 ? "-" : "+",
						abs(snst->cal_result) / 10,
						abs(snst->cal_result) % 10 );
				x = print_s(c,x,y,s);
				cairo_set_font_size(c, OUTPUT_FONT*2/3);
				x = print_s(c,x,y," s/d");
				break;
			case 0:
				sprintf(s, " %d", snst->cal_percent);
				x = print_number(c,x,y,s);
				x = print_s(c,x,y," %");
				break;
		}
	} else {
		draw_results_table(c, x + 10, width - x - 20, height, snst, p,
			snst->audio_status == AUDIO_STATUS_BPH_UNCONFIRMED);
	}
#ifdef DEBUG
	{
		// Frame rate, small and out of the way in the top right corner
		static GTimer *timer = NULL;
		if (!timer) timer = g_timer_new();
		else {
			char s[100];
			sprintf(s,"%.2f fps",1./g_timer_elapsed(timer, NULL));
			cairo_set_source(c, white);
			cairo_set_font_size(c, 12);
			print_s(c, width - 90, 14, s);
			g_timer_reset(timer);
		}
	}
#endif

	return FALSE;
}

static void expose_waveform(
			struct output_panel *op,
			GtkWidget *da,
			cairo_t *c,
			struct color wave_color,
			int (*get_offset)(struct processing_buffers*),
			double (*get_pulse)(struct processing_buffers*))
{
	cairo_init(c);

	GtkAllocation temp;
	gtk_widget_get_allocation(da, &temp);

	int width = temp.width;
	int height = temp.height;

	gtk_widget_get_allocation(gtk_widget_get_toplevel(da), &temp);
	int font = temp.width / 90;
	if(font < 12)
		font = 12;
	int i;

	cairo_set_font_size(c,font);

	// Time axis (bottom half, white): solid every 5 ms, dotted every ms
	for(i = 1-NEGATIVE_SPAN; i < POSITIVE_SPAN; i++) {
		int x = (NEGATIVE_SPAN + i) * width / (POSITIVE_SPAN + NEGATIVE_SPAN);
		cairo_move_to(c, x + .5, height / 2 + .5);
		cairo_line_to(c, x + .5, height - .5);
		set_axis_style(c, COL_AXIS, !(i%5));
		cairo_stroke(c);
	}
	set_dotted(c, 0);
	cairo_set_source(c,white);
	for(i = 1-NEGATIVE_SPAN; i < POSITIVE_SPAN; i++) {
		if(!(i%5)) {
			int x = (NEGATIVE_SPAN + i) * width / (POSITIVE_SPAN + NEGATIVE_SPAN);
			char s[10];
			sprintf(s,"%d",i);
			cairo_move_to(c,x+font/4,height-font/2);
			cairo_show_text(c,s);
		}
	}

	cairo_text_extents_t extents;

	cairo_text_extents(c,"ms",&extents);
	cairo_move_to(c,width - extents.x_advance - font/4,height-font/2);
	cairo_show_text(c,"ms");

	struct snapshot *snst = op->snst;
	struct processing_buffers *p = snst->pb;
	int old = snst->is_old;
	double period = p ? p->period / snst->sample_rate : 7200. / snst->guessed_bph;

	// Amplitude scale (top half, green): solid every 50 deg, dotted every 10 deg
	for(i = 10; i < 360; i+=10) {
		if(2*i < snst->la) continue;
		double t = period*amplitude_to_time(snst->la,i);
		if(t > .001 * NEGATIVE_SPAN) continue;
		int x = round(width * (NEGATIVE_SPAN - 1000*t) / (NEGATIVE_SPAN + POSITIVE_SPAN));
		cairo_move_to(c, x+.5, .5);
		cairo_line_to(c, x+.5, height / 2 + .5);
		set_axis_style(c, COL_AMP, !(i % 50));
		cairo_stroke(c);
	}
	set_dotted(c, 0);

	double last_x = 0;
	set_color(c, COL_AMP, 1);
	for(i = 50; i < 360; i+=50) {
		double t = period*amplitude_to_time(snst->la,i);
		if(t > .001 * NEGATIVE_SPAN) continue;
		int x = round(width * (NEGATIVE_SPAN - 1000*t) / (NEGATIVE_SPAN + POSITIVE_SPAN));
		if(x > last_x) {
			char s[10];

			sprintf(s,"%d",abs(i));
			cairo_move_to(c, x + font/4, font * 3 / 2);
			cairo_show_text(c,s);
			cairo_text_extents(c,s,&extents);
			last_x = x + font/4 + extents.x_advance;
		}
	}

	cairo_text_extents(c,"deg",&extents);
	cairo_move_to(c,width - extents.x_advance - font/4,font * 3 / 2);
	cairo_show_text(c,"deg");

	if(p) {
		double span = 0.001 * snst->sample_rate;
		int offset = get_offset(p);

		double a = offset - span * NEGATIVE_SPAN;
		double b = offset + span * POSITIVE_SPAN;

		draw_graph(a,b,c,p,da);

		set_signal_color(c, wave_color, old);
		cairo_stroke_preserve(c);
		cairo_fill(c);

		// Detected pulse position: reference colour
		double pulse = get_pulse(p);
		if(pulse > 0) {
			int x = round((NEGATIVE_SPAN - pulse / span) * width / (POSITIVE_SPAN + NEGATIVE_SPAN));
			cairo_move_to(c, x, 1);
			cairo_line_to(c, x, height - 1);
			set_signal_color(c, COL_RATE, old);
			cairo_set_line_width(c,2);
			cairo_stroke(c);
		}
	} else {
		cairo_move_to(c, .5, height / 2 + .5);
		cairo_line_to(c, width - .5, height / 2 + .5);
		set_color(c, COL_AXIS, ALPHA_STALE);
		cairo_stroke(c);
	}
}

static int get_tic(struct processing_buffers *p)
{
	return p->tic;
}

static int get_toc(struct processing_buffers *p)
{
	return p->toc;
}

static double get_tic_pulse(struct processing_buffers *p)
{
	return p->tic_pulse;
}

static double get_toc_pulse(struct processing_buffers *p)
{
	return p->toc_pulse;
}

static gboolean tic_draw_event(GtkWidget *widget, cairo_t *c, struct output_panel *op)
{
	UNUSED(widget);
	expose_waveform(op, op->tic_drawing_area, c, COL_TIC, get_tic, get_tic_pulse);
	return FALSE;
}

static gboolean toc_draw_event(GtkWidget *widget, cairo_t *c, struct output_panel *op)
{
	UNUSED(widget);
	expose_waveform(op, op->toc_drawing_area, c, COL_TOC, get_toc, get_toc_pulse);
	return FALSE;
}

/* Vertical grid with ms labels for a window starting at t_left ms and span_ms wide.
 * Major (solid, labelled) lines use a round step giving at most ~10 labels; minor
 * (dotted) lines divide each major step in 5. */
static void draw_ms_axis(cairo_t *c, int width, int height, int font, double t_left, double span_ms)
{
	static const int majors[] = {5, 10, 20, 25, 50, 100, 200, 250, 500, 0};
	int major = 1000;
	int i;
	if(span_ms <= 0 || width <= 0) return;
	for(i = 0; majors[i]; i++) {
		if(span_ms / majors[i] <= 10) {
			major = majors[i];
			break;
		}
	}
	double minor = major / 5.0;
	long k0 = (long)ceil(t_left / minor);
	long k1 = (long)floor((t_left + span_ms) / minor);
	long k;

	for(k = k0; k <= k1; k++) {
		double x = (k * minor - t_left) * width / span_ms;
		if(x <= 0 || x >= width) continue;
		cairo_move_to(c, round(x) + .5, .5);
		cairo_line_to(c, round(x) + .5, height - .5);
		set_axis_style(c, COL_AXIS, !(k % 5));
		cairo_stroke(c);
	}
	set_dotted(c, 0);

	cairo_set_font_size(c, font);
	cairo_text_extents_t extents;
	cairo_text_extents(c, "ms", &extents);
	double unit_x = width - extents.x_advance - font/4;

	cairo_set_source(c, white);
	for(k = k0; k <= k1; k++) {
		if(k % 5) continue;
		double x = (k * minor - t_left) * width / span_ms;
		if(x <= 0 || x >= width) continue;
		char s[16];
		snprintf(s, sizeof(s), "%ld", lround(k * minor));
		cairo_text_extents(c, s, &extents);
		// keep labels clear of the "ms" unit at the right edge
		if(round(x) + font/4 + extents.x_advance > unit_x - font/2) continue;
		cairo_move_to(c, round(x) + font/4, height - font/2);
		cairo_show_text(c, s);
	}

	cairo_move_to(c, unit_x, height - font/2);
	cairo_show_text(c, "ms");
}

static gboolean period_draw_event(GtkWidget *widget, cairo_t *c, struct output_panel *op)
{
	UNUSED(widget);
	cairo_init(c);

	GtkAllocation temp;
	gtk_widget_get_allocation (op->period_drawing_area, &temp);

	int width = temp.width;
	int height = temp.height;

	struct snapshot *snst = op->snst;
	struct processing_buffers *p = snst->pb;
	int old = snst->is_old;

	double toc,a=0,b=0;

	if(p) {
		toc = p->tic < p->toc ? p->toc : p->toc + p->period;
		a = ((double)p->tic + toc)/2 - p->period/2;
		b = ((double)p->tic + toc)/2 + p->period/2;

		cairo_move_to(c, (p->tic - a - NEGATIVE_SPAN*.001*snst->sample_rate) * width/p->period, 0);
		cairo_line_to(c, (p->tic - a - NEGATIVE_SPAN*.001*snst->sample_rate) * width/p->period, height);
		cairo_line_to(c, (p->tic - a + POSITIVE_SPAN*.001*snst->sample_rate) * width/p->period, height);
		cairo_line_to(c, (p->tic - a + POSITIVE_SPAN*.001*snst->sample_rate) * width/p->period, 0);
		set_color(c, COL_TIC, ALPHA_BAND); // tic window: grey tint
		cairo_fill(c);

		cairo_move_to(c, (toc - a - NEGATIVE_SPAN*.001*snst->sample_rate) * width/p->period, 0);
		cairo_line_to(c, (toc - a - NEGATIVE_SPAN*.001*snst->sample_rate) * width/p->period, height);
		cairo_line_to(c, (toc - a + POSITIVE_SPAN*.001*snst->sample_rate) * width/p->period, height);
		cairo_line_to(c, (toc - a + POSITIVE_SPAN*.001*snst->sample_rate) * width/p->period, 0);
		set_color(c, COL_TOC, ALPHA_BAND); // toc window: gold tint
		cairo_fill(c);
	}

	// Time axis in ms, 0 at the tic (same convention as the tic/toc panels)
	{
		GtkAllocation top;
		gtk_widget_get_allocation(gtk_widget_get_toplevel(op->period_drawing_area), &top);
		int font = top.width / 90;
		if(font < 12)
			font = 12;

		double sr = snst->sample_rate > 0 ? snst->sample_rate : snst->nominal_sr;
		double span_ms = p ? 1000. * p->period / sr : 1000. * 7200. / snst->guessed_bph;
		double t_left = p ? 1000. * (a - p->tic) / sr : 0;
		draw_ms_axis(c, width, height, font, t_left, span_ms);
	}

	if(p) {
		draw_graph(a,b,c,p,op->period_drawing_area);

		set_signal_color(c, COL_WAVE, old);
		cairo_stroke_preserve(c);
		cairo_fill(c);
	} else {
		cairo_move_to(c, .5, height / 2 + .5);
		cairo_line_to(c, width - .5, height / 2 + .5);
		set_color(c, COL_AXIS, ALPHA_STALE);
		cairo_stroke(c);
	}

	return FALSE;
}

/* The measured rate changes a little with every reading, which would make the
 * rate guide lines swing all the time. For the live display the guide follows a
 * low-pass filtered rate instead; a big change is a new situation (another
 * watch, a different bph) and is followed at once. Saved snapshots are static
 * and use their own rate. */
#define GUIDE_RATE_TAU  10.0	// filter time constant, seconds
#define GUIDE_RATE_JUMP 5.0	// s/d

static double guide_rate(struct output_panel *op)
{
	double rate = op->snst->rate;
	if(!op->computer)
		return rate;

	gint64 now = g_get_monotonic_time();
	if(!op->guide_time || fabs(rate - op->guide_rate) > GUIDE_RATE_JUMP) {
		op->guide_rate = rate;
	} else {
		// Time based, so that the number of redraws makes no difference
		double dt = (now - op->guide_time) / 1e6;
		op->guide_rate += (1 - exp(-dt / GUIDE_RATE_TAU)) * (rate - op->guide_rate);
	}
	op->guide_time = now;
	return op->guide_rate;
}

#define LEGEND_DOT  0
#define LEGEND_LINE 1
#define LEGEND_DASH 2

/* One legend entry (marker and text) at baseline y; returns where the next one starts */
static double legend_item(cairo_t *c, double x, double y, int style, struct color col, const char *text)
{
	static const double dashes[2] = {4, 3};
	cairo_text_extents_t e;

	set_color(c, col, 1);
	if(style == LEGEND_DOT) {
		cairo_arc(c, x + 3, y - 3.5, 3, 0, 2 * M_PI);
		cairo_fill(c);
		x += 10;
	} else {
		cairo_set_line_width(c, 1.5);
		if(style == LEGEND_DASH)
			cairo_set_dash(c, dashes, 2, 0);
		cairo_move_to(c, x, y - 3.5);
		cairo_line_to(c, x + 16, y - 3.5);
		cairo_stroke(c);
		set_dotted(c, 0);
		x += 20;
	}
	set_color(c, COL_AXIS, 0.85);
	cairo_text_extents(c, text, &e);
	cairo_move_to(c, x, y);
	cairo_show_text(c, text);
	return x + e.x_advance + 14;
}

static gboolean paperstrip_draw_event(GtkWidget *widget, cairo_t *c, struct output_panel *op)
{
	int i;
	struct snapshot *snst = op->snst;
	const int valid_events_wp =
		snst->events &&
		snst->events_count > 0 &&
		snst->events_wp >= 0 &&
		snst->events_wp < snst->events_count;
	uint64_t time = snst->timestamp ? snst->timestamp : get_timestamp(snst->is_light);
	double sweep;
	double zoom_factor;
	double safe_sample_rate = snst->sample_rate > 0 ? snst->sample_rate : snst->nominal_sr;
	int safe_bph = snst->guessed_bph > 0 ? snst->guessed_bph : (snst->bph ? snst->bph : DEFAULT_BPH);
	const double trace_zoom = snst->trace_zoom > 0 ? snst->trace_zoom : 1.0;
	double slope = 1000; // detected rate: 1000 -> do not display
	if(snst->calibrate) {
		sweep = snst->nominal_sr;
		zoom_factor = PAPERSTRIP_ZOOM_CAL * trace_zoom;
		slope = (double) snst->cal * zoom_factor / (10 * 3600 * 24);
	} else {
		sweep = safe_sample_rate * 3600. / safe_bph;
		zoom_factor = PAPERSTRIP_ZOOM * trace_zoom;
		if(valid_events_wp && snst->events[snst->events_wp])
			slope = - guide_rate(op) * zoom_factor / (3600. * 24.);
		else
			op->guide_time = 0; // no trace: start over when it comes back
	}
	if(!isfinite(sweep) || sweep <= 0 || !isfinite(zoom_factor) || zoom_factor <= 0)
		return FALSE;

	cairo_init(c);

	GtkAllocation temp;
	gtk_widget_get_allocation (op->paperstrip_drawing_area, &temp);

	int width = temp.width;
	int height = temp.height;

	int stopped = 0;
	if( valid_events_wp &&
	    snst->events[snst->events_wp] &&
	    time > 5 * snst->nominal_sr + snst->events[snst->events_wp]) {
		time = 5 * snst->nominal_sr + snst->events[snst->events_wp];
		stopped = 1;
	}

	int strip_width = round(width / (1 + PAPERSTRIP_MARGIN));
	int left_margin = (width - strip_width) / 2;
	int right_margin = (width + strip_width) / 2;

	GtkAllocation toplevel;
	gtk_widget_get_allocation(gtk_widget_get_toplevel(widget), &toplevel);
	int font = toplevel.width / 90;
	if(font < 12)
		font = 12;

	/* Layout: scales on top, the data in the middle with the newest beats at
	 * its bottom edge (older ones move up), legend at the bottom. */
	const int scale_rows = snst->calibrate ? 1 : 2;
	const double plot_top = scale_rows * (font + 4) + 4;
	const double plot_bottom = height - (font + 8);
	if(plot_bottom - plot_top < 20)
		return FALSE;

	// Width of the strip in ms (a beat in normal mode, a second in calibration, divided by the zoom)
	const double window_ms = snst->calibrate ? 1000. / zoom_factor : 3600000. / (safe_bph * zoom_factor);
	// Amplitude scale: linear, 135 deg at the left edge, 360 deg at the right edge
	const double amp_min = 135.0, amp_max = 360.0;
#define AMP_X(a)   (left_margin + ((a) - amp_min) / (amp_max - amp_min) * (strip_width - 1))
	// The beat position runs from right to left, so that a fast watch (positive
	// rate) leans to the right with the newest beats at the bottom
#define RULER_X(t) (right_margin - (t) * strip_width / window_ms)

	// Ruler in ms: the smallest round step that gives at most 10 divisions
	static const double ruler_steps[] = {0.1, 0.2, 0.5, 1, 2, 5, 10, 20, 50, 100, 0};
	double ruler = 100;
	for(i = 0; ruler_steps[i]; i++) {
		if(window_ms / ruler_steps[i] <= 10) {
			ruler = ruler_steps[i];
			break;
		}
	}

	char s[32];
	int k, a;
	cairo_text_extents_t extents;

	double now = sweep*ceil(time/sweep);
	double ten_s = safe_sample_rate * 10 / sweep;
	double last_line = fmod(now/sweep, ten_s);
	int last_tenth = floor(now/(sweep*ten_s));

	// Everything below is clipped to the data area
	cairo_save(c);
	cairo_rectangle(c, 0, plot_top, width, plot_bottom - plot_top);
	cairo_clip(c);
	cairo_set_line_width(c, 1);

	// Vertical grid: ruler lines (white, dotted) and amplitude lines (green, dotted)
	for(k = 1; k * ruler < window_ms - ruler / 2; k++) {
		double x = round(RULER_X(k * ruler)) + .5;
		cairo_move_to(c, x, plot_top);
		cairo_line_to(c, x, plot_bottom);
	}
	set_axis_style(c, COL_AXIS, 0);
	cairo_stroke(c);
	if(!snst->calibrate) {
		for(a = 150; a <= 350; a += 50) {
			double x = round(AMP_X(a)) + .5;
			cairo_move_to(c, x, plot_top);
			cairo_line_to(c, x, plot_bottom);
		}
		set_axis_style(c, COL_AMP, 0);
		cairo_stroke(c);
	}

	// Window edges: a beat leaving one edge comes back at the other
	cairo_move_to(c, left_margin + .5, plot_top);
	cairo_line_to(c, left_margin + .5, plot_bottom);
	cairo_move_to(c, right_margin + .5, plot_top);
	cairo_line_to(c, right_margin + .5, plot_bottom);
	set_axis_style(c, COL_AXIS, 1);
	cairo_stroke(c);

	// Time marks move with the data: dotted every 10 s, solid every minute,
	// labelled with the time since the start of the measurement
	cairo_set_font_size(c, font * 0.8);
	for(i=0;;i++) {
		int tenth = last_tenth - i;
		double y = plot_bottom - 0.5 - round(last_line + i*ten_s);
		if(y < plot_top || tenth < 0) break;
		cairo_move_to(c, .5, y);
		cairo_line_to(c, width-.5, y);
		set_axis_style(c, COL_AXIS, !(tenth % 6));
		cairo_stroke(c);
		set_dotted(c, 0);

		snprintf(s, sizeof(s), "%d:%02d", tenth / 6, (tenth % 6) * 10);
		cairo_text_extents(c, s, &extents);
		set_color(c, COL_AXIS, 0.7);
		cairo_move_to(c, fmax(1, left_margin - extents.x_advance - 3), y - 2);
		cairo_show_text(c, s);
	}

	// Rate guide: lines with the slope of the measured rate (in calibration, of
	// the current cal value), dashed in the reference colour. The trace should
	// run parallel to them. Age is counted in rows from the bottom edge.
	cairo_set_line_width(c, 1.2);
	slope *= -strip_width; // mirrored together with the beat dots, see below
	if(slope <= 2 && slope >= -2) {
		static const double guide_dashes[2] = {6, 4};
		const double rows = plot_bottom;
		cairo_set_dash(c, guide_dashes, 2, 0);
		for(i=0; i<4; i++) {
			double y = 0;
			cairo_move_to(c, (double)width * (i+.5) / 4, plot_bottom);
			for(;;) {
				double x = y * slope + (double)width * (i+.5) / 4;
				x = fmod(x, width);
				if(x < 0) x += width;
				double nx = x + slope * (rows - y);
				if(nx >= 0 && nx <= width) {
					cairo_line_to(c, nx, plot_bottom - rows);
					break;
				} else {
					double d = slope > 0 ? width - x : x;
					y += d / fabs(slope);
					cairo_line_to(c, slope > 0 ? width : 0, plot_bottom - y);
					y += 1;
					if(y > rows) break;
					cairo_move_to(c, slope > 0 ? 0 : width, plot_bottom - y);
				}
			}
		}
		set_color(c, COL_RATE, 0.9);
		cairo_stroke(c);
		set_dotted(c, 0);
	}

	// Amplitude history (green); higher amplitude is further right, like in the tic and toc panels
	if(!snst->calibrate && snst->amps_count && snst->amps && snst->amps_time) {
		int path_started = 0;
		cairo_set_line_width(c, 1.5);
		for(i = snst->amps_wp;;) {
			if(!snst->amps_time[i]) break;
			double event_age = now - snst->amps_time[i];
			double row = floor(event_age / sweep);
			if(row >= plot_bottom) break;

			double amp_deg = snst->la * snst->amps[i];
			double amp_norm = (amp_deg - amp_min) / (amp_max - amp_min);
			if(amp_norm < 0) amp_norm = 0;
			if(amp_norm > 1) amp_norm = 1;

			double column = left_margin + amp_norm * (strip_width - 1);
			double y = plot_bottom - row - .5;
			if(!path_started) {
				cairo_move_to(c, column, y);
				path_started = 1;
			} else {
				cairo_line_to(c, column, y);
			}

			if(--i < 0) i = snst->amps_count - 1;
			if(i == snst->amps_wp) break;
		}
		if(path_started) {
			set_signal_color(c, COL_AMP, stopped);
			cairo_stroke(c);
		}
	}

	// Beats: one dot per beat, tic in grey and toc in gold (neutral white for
	// the reference ticks of a calibration). Faded when the signal is lost.
	for(i = snst->events_wp; valid_events_wp;) {
		if(!snst->events_count || !snst->events[i]) break;
		double event = now - snst->events[i] + snst->trace_centering + sweep * PAPERSTRIP_MARGIN / (2 * zoom_factor);
		int column = floor(fmod(event, (sweep / zoom_factor)) * strip_width / (sweep / zoom_factor));
		int row = floor(event / sweep);
		if(row >= plot_bottom) break;

		struct color dot = COL_WAVE;
		if(!snst->calibrate && snst->events_tictoc)
			dot = snst->events_tictoc[i] ? COL_TIC : COL_TOC;
		set_signal_color(c, dot, stopped);

		// Mirrored horizontally: later in the beat is further left, so a
		// fast watch (positive rate) leans right, parallel to the rate guide
		double y = plot_bottom - 1 - row;
		cairo_rectangle(c, width - 1 - column, y, 1, 1);
		cairo_fill(c);
		if(column < width - strip_width && row > 0) {
			// Repeat in the left margin, one row newer, so that the trace
			// looks continuous across the window edge
			cairo_rectangle(c, width - 1 - column - strip_width, y + 1, 1, 1);
			cairo_fill(c);
		}
		if(--i < 0) i = snst->events_count - 1;
		if(i == snst->events_wp) break;
	}
	cairo_restore(c);

	// Scales on top: amplitude in degrees (green), position in the beat in ms (white)
	cairo_set_line_width(c, 1);
	cairo_set_font_size(c, font);
	const double y_amp = font + 2;
	const double y_ms = scale_rows == 2 ? 2 * font + 6 : font + 2;
	if(!snst->calibrate) {
		set_color(c, COL_AMP, 1);
		for(a = 150; a <= 350; a += 50) {
			snprintf(s, sizeof(s), "%d\xC2\xB0", a);
			cairo_text_extents(c, s, &extents);
			cairo_move_to(c, AMP_X(a) - extents.x_advance / 2, y_amp);
			cairo_show_text(c, s);
		}
	}
	// The ruler counts from the right edge, so its unit goes where the numbers grow
	cairo_text_extents(c, "ms", &extents);
	double unit_x = font / 4;
	double unit_end = unit_x + extents.x_advance;
	cairo_set_source(c, white);
	for(k = 0; k * ruler <= window_ms + 1e-9; k++) {
		snprintf(s, sizeof(s), "%g", k * ruler);
		cairo_text_extents(c, s, &extents);
		double x = RULER_X(k * ruler) - extents.x_advance / 2;
		if(x < unit_end + font / 2 || x + extents.x_advance > width) continue; // keep clear of the unit
		cairo_move_to(c, x, y_ms);
		cairo_show_text(c, s);
	}
	cairo_move_to(c, unit_x, y_ms);
	cairo_show_text(c, "ms");

	// Legend
	cairo_set_font_size(c, font * 0.9);
	double lx = left_margin;
	const double ly = height - 5;
	if(snst->calibrate) {
		lx = legend_item(c, lx, ly, LEGEND_DOT, COL_WAVE, "reference tick");
		lx = legend_item(c, lx, ly, LEGEND_DASH, COL_RATE, "current cal");
	} else {
		lx = legend_item(c, lx, ly, LEGEND_DOT, COL_TIC, "tic");
		lx = legend_item(c, lx, ly, LEGEND_DOT, COL_TOC, "toc");
		lx = legend_item(c, lx, ly, LEGEND_LINE, COL_AMP, "amplitude");
		lx = legend_item(c, lx, ly, LEGEND_DASH, COL_RATE, "rate guide");
	}

#undef AMP_X
#undef RULER_X
	return FALSE;
}

#ifdef DEBUG
static gboolean debug_draw_event(GtkWidget *widget, cairo_t *c, struct output_panel *op)
{
	UNUSED(widget);
	cairo_init(c);

	struct snapshot *snst = op->snst;
	struct processing_buffers *p;
	if(snst->calibrate)
		p = &op->computer->pdata->buffers[0];
	else
		p = snst->pb;

	if(p) {
		double a = snst->nominal_sr / 10;
		double b = snst->nominal_sr * 2;

		draw_debug_graph(a,b,c,p,op->debug_drawing_area);

		set_signal_color(c, COL_WAVE, snst->is_old);
		cairo_stroke(c);
	}

	return FALSE;
}
#endif

static void handle_clear_trace(GtkButton *b, struct output_panel *op)
{
	UNUSED(b);
	if(op->computer) {
		lock_computer(op->computer);
		if(!op->snst->calibrate) {
			memset(op->snst->events,0,op->snst->events_count*sizeof(uint64_t));
			memset(op->snst->events_tictoc,0,op->snst->events_count*sizeof(unsigned char));
			memset(op->snst->amps,0,op->snst->amps_count*sizeof(*op->snst->amps));
			memset(op->snst->amps_time,0,op->snst->amps_count*sizeof(*op->snst->amps_time));
			op->snst->trace_zoom = 1.0;
			op->computer->clear_trace = 1;
		}
		unlock_computer(op->computer);
		gtk_widget_queue_draw(op->paperstrip_drawing_area);
	}
}

static void handle_center_trace(GtkButton *b, struct output_panel *op)
{
	UNUSED(b);
	struct snapshot *snst = op->snst;
	if(!snst || !snst->events || snst->events_count <= 0)
		return;
	if(snst->events_wp < 0 || snst->events_wp >= snst->events_count)
		return;
	uint64_t last_ev = snst->events[snst->events_wp];
	double new_centering;
	if(last_ev) {
		double sweep;
		double zoom_factor;
		double time = snst->timestamp ? snst->timestamp : get_timestamp(snst->is_light);
		double now;
		double chart_width;
		if(snst->calibrate) {
			sweep = (double) snst->nominal_sr;
			zoom_factor = PAPERSTRIP_ZOOM_CAL * (snst->trace_zoom > 0 ? snst->trace_zoom : 1.0);
		} else {
			if(snst->sample_rate <= 0 || snst->guessed_bph <= 0)
				return;
			sweep = snst->sample_rate * 3600. / snst->guessed_bph;
			zoom_factor = PAPERSTRIP_ZOOM * (snst->trace_zoom > 0 ? snst->trace_zoom : 1.0);
		}
		if(!isfinite(sweep) || sweep <= 0 || !isfinite(zoom_factor) || zoom_factor <= 0)
			return;
		now = sweep * ceil(time / sweep);
		chart_width = sweep / zoom_factor;
		if(!isfinite(now) || !isfinite(chart_width) || chart_width <= 0)
			return;
		new_centering = fmod(0.5 * chart_width - fmod(now - last_ev, chart_width), chart_width);
		if(new_centering < 0)
			new_centering += chart_width;
	} else 
		new_centering = 0;
	if(!isfinite(new_centering))
		return;
	snst->trace_centering = new_centering;
	gtk_widget_queue_draw(op->paperstrip_drawing_area);
}

static void shift_trace(struct output_panel *op, double direction)
{
	struct snapshot *snst = op->snst;
	if(!snst)
		return;
	double chart_width;
	double zoom_factor;
	if(snst->calibrate) {
		chart_width = (double) snst->nominal_sr;
		zoom_factor = PAPERSTRIP_ZOOM_CAL * (snst->trace_zoom > 0 ? snst->trace_zoom : 1.0);
	} else {
		if(snst->sample_rate <= 0 || snst->guessed_bph <= 0)
			return;
		chart_width = snst->sample_rate * 3600. / snst->guessed_bph;
		zoom_factor = PAPERSTRIP_ZOOM * (snst->trace_zoom > 0 ? snst->trace_zoom : 1.0);
	}
	if(!isfinite(chart_width) || chart_width <= 0 || !isfinite(zoom_factor) || zoom_factor <= 0)
		return;
	chart_width /= zoom_factor;
	if(!isfinite(chart_width) || chart_width <= 0)
		return;
	snst->trace_centering = fmod(snst->trace_centering + chart_width * 0.1 * direction, chart_width);
	if(snst->trace_centering < 0)
		snst->trace_centering += chart_width;
	gtk_widget_queue_draw(op->paperstrip_drawing_area);
}

static void zoom_trace(struct output_panel *op, double factor)
{
	struct snapshot *snst = op->snst;
	if(!snst)
		return;
	if(snst->trace_zoom <= 0)
		snst->trace_zoom = 1.0;
	snst->trace_zoom *= factor;
	if(snst->trace_zoom < 0.25)
		snst->trace_zoom = 0.25;
	if(snst->trace_zoom > 8.0)
		snst->trace_zoom = 8.0;
	gtk_widget_queue_draw(op->paperstrip_drawing_area);
}

static void handle_zoom_out(GtkButton *b, struct output_panel *op)
{
	UNUSED(b);
	zoom_trace(op, 1.0 / 1.25);
}

static void handle_zoom_in(GtkButton *b, struct output_panel *op)
{
	UNUSED(b);
	zoom_trace(op, 1.25);
}

static void handle_zoom_reset(GtkButton *b, struct output_panel *op)
{
	UNUSED(b);
	if(!op->snst)
		return;
	op->snst->trace_zoom = 1.0;
	gtk_widget_queue_draw(op->paperstrip_drawing_area);
}

static void handle_left(GtkButton *b, struct output_panel *op)
{
	UNUSED(b);
	shift_trace(op,-1);
}

static void handle_right(GtkButton *b, struct output_panel *op)
{
	UNUSED(b);
	shift_trace(op,1);
}

/* While calibrating, the waveform panels are meaningless: show instructions instead */
#define TITLE_PAPERSTRIP  "Paper strip: beat timing over time"
#define TITLE_PAPERSTRIP_CAL "Paper strip: reference ticks"

/* Wrap a panel in a frame with a small title on top */
static GtkWidget *titled_frame(GtkWidget *child, const char *title)
{
	GtkWidget *frame = gtk_frame_new(title);
	gtk_frame_set_label_align(GTK_FRAME(frame), 0.01, 0.5);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);
	gtk_container_add(GTK_CONTAINER(frame), child);
	return frame;
}

static void set_frame_title(GtkWidget *frame, const char *title)
{
	const gchar *current = gtk_frame_get_label(GTK_FRAME(frame));
	if(!current || strcmp(current, title))
		gtk_frame_set_label(GTK_FRAME(frame), title);
}

static void update_right_side(struct output_panel *op)
{
	int cal = op->snst && op->snst->calibrate;
	const char *page = cal ? "calibration" : "panels";
	const gchar *current = gtk_stack_get_visible_child_name(GTK_STACK(op->right_stack));
	if(!current || strcmp(current, page))
		gtk_stack_set_visible_child_name(GTK_STACK(op->right_stack), page);

	set_frame_title(op->paperstrip_frame, cal ? TITLE_PAPERSTRIP_CAL : TITLE_PAPERSTRIP);
}

static GtkWidget *make_calibration_instructions(void)
{
	static const char *markup =
		"<span size='x-large' weight='bold'>Calibration in progress</span>\n"
		"\n"
		"Calibration measures how fast your sound card clock really runs, so that "
		"rates are not skewed by the computer. It takes about 15 minutes (900 ticks).\n"
		"\n"
		"<b>1. Use a quartz reference</b>\n"
		"Place a quartz watch or clock that ticks once per second (an analog one with "
		"a seconds hand) very close to the microphone, ideally touching it. "
		"Its accuracy becomes your calibration accuracy.\n"
		"\n"
		"<b>2. Keep it quiet</b>\n"
		"Work in a quiet room, switch off nearby fans and speakers, and do not touch "
		"the desk while calibrating. Raise the microphone gain only as far as needed to "
		"hear the tick clearly without distortion.\n"
		"\n"
		"<b>3. Watch the status at the top</b>\n"
		"<b>cal (wait)</b>: no steady 1 tick per second found yet, check the setup.\n"
		"<b>cal (acq.) NN %</b>: collecting ticks, leave everything untouched.\n"
		"<b>cal (done) \302\261X.X s/d</b>: finished. The value is copied to the "
		"<b>cal</b> field and saved.\n"
		"<b>cal (fail)</b>: the signal was too irregular. Improve the setup and start again.\n"
		"\n"
		"<b>4. Finish</b>\n"
		"When it shows <b>done</b>, untick <b>Calibrate</b> in the menu to return to "
		"normal measurement. Repeat once to confirm the result is consistent, and "
		"calibrate again whenever you change the audio input or the sample rate.";

	GtkWidget *label = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(label), markup);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_label_set_max_width_chars(GTK_LABEL(label), 80);
	gtk_label_set_xalign(GTK_LABEL(label), 0);
	gtk_widget_set_halign(label, GTK_ALIGN_CENTER);
	gtk_widget_set_valign(label, GTK_ALIGN_CENTER);
	gtk_widget_set_margin_start(label, 20);
	gtk_widget_set_margin_end(label, 20);
	gtk_widget_set_margin_top(label, 20);
	gtk_widget_set_margin_bottom(label, 20);

	// Scroll if the window is too small to show everything
	GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(scroll), label);
	return scroll;
}

void op_set_snapshot(struct output_panel *op, struct snapshot *snst)
{
	op->snst = snst;
	if(op->snst && op->snst->trace_zoom <= 0)
		op->snst->trace_zoom = 1.0;
	gtk_widget_set_sensitive(op->clear_button, !snst->calibrate);
	update_right_side(op);
}

void op_set_border(struct output_panel *op, int i)
{
	gtk_container_set_border_width(GTK_CONTAINER(op->panel), i);
}

void op_destroy(struct output_panel *op)
{
	snapshot_destroy(op->snst);
	free(op);
}

struct output_panel *init_output_panel(struct computer *comp, struct snapshot *snst, int border)
{
	struct output_panel *op = malloc(sizeof(struct output_panel));

	op->computer = comp;
	op->snst = snst;
	op->guide_rate = 0;
	op->guide_time = 0;
	op->audio_meter_level = 0;
	op->audio_meter_time = 0;
	op->audio_meter_active = 0;
	op->audio_meter_transition_count = 0;
	op->audio_status = -1;

	op->panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
	gtk_container_set_border_width(GTK_CONTAINER(op->panel), border);

	// Info area on top
	op->output_drawing_area = gtk_drawing_area_new();
	gtk_widget_set_size_request(op->output_drawing_area, 0, OUTPUT_WINDOW_HEIGHT);
	gtk_box_pack_start(GTK_BOX(op->panel), op->output_drawing_area, FALSE, TRUE, 0);
	g_signal_connect (op->output_drawing_area, "draw", G_CALLBACK(output_draw_event), op);
	gtk_widget_set_events(op->output_drawing_area, GDK_EXPOSURE_MASK);

	GtkWidget *hbox2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
	gtk_box_pack_start(GTK_BOX(op->panel), hbox2, TRUE, TRUE, 0);

	GtkWidget *vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
	gtk_box_pack_start(GTK_BOX(hbox2), vbox2, FALSE, TRUE, 0);

	// Paperstrip
	op->paperstrip_drawing_area = gtk_drawing_area_new();
	gtk_widget_set_size_request(op->paperstrip_drawing_area, 300, 0);
	op->paperstrip_frame = titled_frame(op->paperstrip_drawing_area, TITLE_PAPERSTRIP);
	gtk_box_pack_start(GTK_BOX(vbox2), op->paperstrip_frame, TRUE, TRUE, 0);
	g_signal_connect (op->paperstrip_drawing_area, "draw", G_CALLBACK(paperstrip_draw_event), op);
	gtk_widget_set_events(op->paperstrip_drawing_area, GDK_EXPOSURE_MASK);

	GtkWidget *hbox3 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
	gtk_box_pack_start(GTK_BOX(vbox2), hbox3, FALSE, TRUE, 0);

	// < button
	GtkWidget *left_button = gtk_button_new_with_label("<");
	gtk_box_pack_start(GTK_BOX(hbox3), left_button, TRUE, TRUE, 0);
	g_signal_connect (left_button, "clicked", G_CALLBACK(handle_left), op);

	// CLEAR button
	if(comp) {
		op->clear_button = gtk_button_new_with_label("Clear");
		gtk_box_pack_start(GTK_BOX(hbox3), op->clear_button, TRUE, TRUE, 0);
		g_signal_connect (op->clear_button, "clicked", G_CALLBACK(handle_clear_trace), op);
		gtk_widget_set_sensitive(op->clear_button, !snst->calibrate);
	}

	// CENTER button
	GtkWidget *center_button = gtk_button_new_with_label("Center");
	gtk_box_pack_start(GTK_BOX(hbox3), center_button, TRUE, TRUE, 0);
	g_signal_connect (center_button, "clicked", G_CALLBACK(handle_center_trace), op);

	GtkWidget *zoom_out_button = gtk_button_new_with_label("-");
	gtk_box_pack_start(GTK_BOX(hbox3), zoom_out_button, TRUE, TRUE, 0);
	g_signal_connect (zoom_out_button, "clicked", G_CALLBACK(handle_zoom_out), op);

	GtkWidget *zoom_reset_button = gtk_button_new_with_label("1x");
	gtk_box_pack_start(GTK_BOX(hbox3), zoom_reset_button, TRUE, TRUE, 0);
	g_signal_connect (zoom_reset_button, "clicked", G_CALLBACK(handle_zoom_reset), op);

	GtkWidget *zoom_in_button = gtk_button_new_with_label("+");
	gtk_box_pack_start(GTK_BOX(hbox3), zoom_in_button, TRUE, TRUE, 0);
	g_signal_connect (zoom_in_button, "clicked", G_CALLBACK(handle_zoom_in), op);

	// > button
	GtkWidget *right_button = gtk_button_new_with_label(">");
	gtk_box_pack_start(GTK_BOX(hbox3), right_button, TRUE, TRUE, 0);
	g_signal_connect (right_button, "clicked", G_CALLBACK(handle_right), op);

	// Right side: waveform panels, or calibration instructions while calibrating
	op->right_stack = gtk_stack_new();
	gtk_box_pack_start(GTK_BOX(hbox2), op->right_stack, TRUE, TRUE, 0);

	GtkWidget *vbox3 = gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
	gtk_stack_add_named(GTK_STACK(op->right_stack), vbox3, "panels");

	GtkWidget *cal_info = titled_frame(make_calibration_instructions(), "Calibration instructions");
	gtk_stack_add_named(GTK_STACK(op->right_stack), cal_info, "calibration");

	// Tic waveform area
	op->tic_drawing_area = gtk_drawing_area_new();
	gtk_box_pack_start(GTK_BOX(vbox3), titled_frame(op->tic_drawing_area,
			"Tic: averaged sound of the first beat"), TRUE, TRUE, 0);
	g_signal_connect (op->tic_drawing_area, "draw", G_CALLBACK(tic_draw_event), op);
	gtk_widget_set_events(op->tic_drawing_area, GDK_EXPOSURE_MASK);

	// Toc waveform area
	op->toc_drawing_area = gtk_drawing_area_new();
	gtk_box_pack_start(GTK_BOX(vbox3), titled_frame(op->toc_drawing_area,
			"Toc: averaged sound of the second beat"), TRUE, TRUE, 0);
	g_signal_connect (op->toc_drawing_area, "draw", G_CALLBACK(toc_draw_event), op);
	gtk_widget_set_events(op->toc_drawing_area, GDK_EXPOSURE_MASK);

	// Period waveform area
	op->period_drawing_area = gtk_drawing_area_new();
	gtk_box_pack_start(GTK_BOX(vbox3), titled_frame(op->period_drawing_area,
			"Full period: tic and toc (highlighted)"), TRUE, TRUE, 0);
	g_signal_connect (op->period_drawing_area, "draw", G_CALLBACK(period_draw_event), op);
	gtk_widget_set_events(op->period_drawing_area, GDK_EXPOSURE_MASK);

#ifdef DEBUG
	op->debug_drawing_area = gtk_drawing_area_new();
	gtk_box_pack_start(GTK_BOX(vbox3), titled_frame(op->debug_drawing_area,
			"Debug: processed signal"), TRUE, TRUE, 0);
	g_signal_connect (op->debug_drawing_area, "draw", G_CALLBACK(debug_draw_event), op);
	gtk_widget_set_events(op->debug_drawing_area, GDK_EXPOSURE_MASK);
#endif

	// GtkStack only switches to visible children
	gtk_widget_show_all(vbox3);
	gtk_widget_show_all(cal_info);
	update_right_side(op);

	return op;
}
