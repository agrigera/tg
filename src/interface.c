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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>
#include <ctype.h>

#ifdef DEBUG
int testing = 0;
#endif

int preset_bph[] = PRESET_BPH;
static const int available_sample_rates[] = {22050, 32000, 44100, 48000, 96000, 0};
static const char *available_sample_rate_labels[] = {"22.05 kHz", "32 kHz", "44.1 kHz", "48 kHz", "96 kHz", NULL};

static guint kick_computer(struct main_window *w);
guint save_on_change_timer(struct main_window *w);
static void remove_runtime_timers(struct main_window *w);
static void update_sensitivity(struct main_window *w);

void print_debug(char *format,...)
{
	va_list args;
	va_start(args,format);
	vfprintf(stderr,format,args);
	va_end(args);
}

void error(char *format,...)
{
	char s[100];
	va_list args;

	va_start(args,format);
	int size = vsnprintf(s,100,format,args);
	va_end(args);

	char *t;
	if(size < 100) {
		t = s;
	} else {
		t = alloca(size+1);
		va_start(args,format);
		vsnprintf(t,size+1,format,args);
		va_end(args);
	}

	fprintf(stderr,"%s\n",t);

#ifdef DEBUG
	if(testing) return;
#endif

	GtkWidget *dialog = gtk_message_dialog_new(NULL,0,GTK_MESSAGE_ERROR,GTK_BUTTONS_CLOSE,"%s",t);
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}

static void refresh_results(struct main_window *w)
{
	w->active_snapshot->bph = w->bph;
	w->active_snapshot->la = w->la;
	w->active_snapshot->cal = w->cal;
	compute_results(w->active_snapshot);
}

static void kill_computer(struct main_window *w);
static void watch_sync_selection(struct main_window *w);
static void show_position(struct main_window *w, int pos);

static void handle_bph_change(GtkComboBox *b, struct main_window *w)
{
	if(!w->controls_active) return;
	char *s = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(b));
	if(s) {
		int bph;
		char *t;
		int n = (int)strtol(s,&t,10);
		if(*t || n < MIN_BPH || n > MAX_BPH) bph = 0;
		else bph = n;
		g_free(s);
		w->bph = bph;
		refresh_results(w);
		gtk_widget_queue_draw(w->notebook);
		watch_sync_selection(w);
	}
}

static void handle_la_change(GtkSpinButton *b, struct main_window *w)
{
	if(!w->controls_active) return;
	double la = gtk_spin_button_get_value(b);
	if(la < MIN_LA || la > MAX_LA) la = DEFAULT_LA;
	w->la = la;
	refresh_results(w);
	gtk_widget_queue_draw(w->notebook);
	watch_sync_selection(w);
}

/* Show bph in the bph combo box: a preset entry, "guess" (0) or custom text */
static void set_bph_combo(struct main_window *w, int bph)
{
	int i, current = 0;
	for(i = 0; preset_bph[i]; i++) {
		if(bph == preset_bph[i]) {
			current = i+1;
			break;
		}
	}
	if(current || bph == 0)
		gtk_combo_box_set_active(GTK_COMBO_BOX(w->bph_combo_box), current);
	else {
		char s[32];
		sprintf(s,"%d",bph);
		GtkEntry *e = GTK_ENTRY(gtk_bin_get_child(GTK_BIN(w->bph_combo_box)));
		gtk_entry_set_text(e,s);
	}
}

/* ---- Saved watches (bph + lift angle presets stored in the config file) ---- */

#define WATCH_NONE_LABEL "(none)"

/* Name of the watch selected in the combo box, or NULL for "(none)" */
static const char *selected_watch(struct main_window *w)
{
	const gchar *id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(w->watch_combo_box));
	return id && *id ? id : NULL;
}

static void watch_update_buttons(struct main_window *w)
{
	// Watches change bph and lift angle, which are locked while calibrating
	int enabled = w->controls_active && !w->calibrate;
	gtk_widget_set_sensitive(w->watch_combo_box, enabled);
	gtk_widget_set_sensitive(w->watch_save_button, enabled);
	gtk_widget_set_sensitive(w->watch_delete_button, enabled && selected_watch(w));
}

/* Select a watch by name without applying it (NULL selects "(none)") */
static void watch_select(struct main_window *w, const char *name)
{
	w->watch_updating++;
	if(!name || !gtk_combo_box_set_active_id(GTK_COMBO_BOX(w->watch_combo_box), name))
		gtk_combo_box_set_active_id(GTK_COMBO_BOX(w->watch_combo_box), "");
	w->watch_updating--;
	watch_update_buttons(w);
}

/* The last watch the user picked, kept in [tg] so it survives restarts */
static gchar *watch_remembered(struct main_window *w)
{
	return g_key_file_get_string(w->config_file, "tg", "watch", NULL);
}

static void watch_remember(struct main_window *w, const char *name)
{
	if(name)
		g_key_file_set_string(w->config_file, "tg", "watch", name);
	else
		g_key_file_remove_key(w->config_file, "tg", "watch", NULL);
}

static int watch_matches(struct main_window *w, const char *name, int bph, double la)
{
	int b;
	double l;
	if(!name || get_watch(w, name, &b, &l)) return 0;
	return b == bph && fabs(l - la) < 0.05;
}

/* Rebuild the combo box from the config file and select `name` */
static void watch_populate(struct main_window *w, const char *name)
{
	GtkComboBoxText *c = GTK_COMBO_BOX_TEXT(w->watch_combo_box);
	w->watch_updating++;
	gtk_combo_box_text_remove_all(c);
	gtk_combo_box_text_append(c, "", WATCH_NONE_LABEL);
	gchar **names = list_watches(w);
	int i;
	for(i = 0; names[i]; i++)
		gtk_combo_box_text_append(c, names[i], names[i]);
	g_strfreev(names);
	w->watch_updating--;
	watch_select(w, name);
}

/* Select the remembered watch if the current bph/lift angle still match it */
static void watch_restore(struct main_window *w)
{
	gchar *name = watch_remembered(w);
	watch_select(w, watch_matches(w, name, w->bph, w->la) ? name : NULL);
	g_free(name);
}

/* After a manual bph/lift angle edit, drop the selection if it no longer matches */
static void watch_sync_selection(struct main_window *w)
{
	if(w->watch_updating) return;
	const char *name = selected_watch(w);
	if(name && !watch_matches(w, name, w->bph, w->la))
		watch_select(w, NULL);
}

static void watch_apply(struct main_window *w, int bph, double la)
{
	w->watch_updating++;
	set_bph_combo(w, bph);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w->la_spin_button), la);
	w->watch_updating--;

	w->bph = bph;
	w->la = la;
	refresh_results(w);
	gtk_widget_queue_draw(w->notebook);
}

static void handle_watch_change(GtkComboBox *b, struct main_window *w)
{
	UNUSED(b);
	if(w->watch_updating || !w->controls_active) return;
	watch_update_buttons(w);

	const char *name = selected_watch(w);
	if(!name) {
		watch_remember(w, NULL);
		return;
	}

	int bph;
	double la;
	if(get_watch(w, name, &bph, &la)) {
		watch_populate(w, NULL);
		return;
	}
	watch_remember(w, name);
	watch_apply(w, bph, la);
}

static void watch_message(struct main_window *w, GtkMessageType type, const char *text)
{
	GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(w->window),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			type, GTK_BUTTONS_CLOSE, "%s", text);
	gtk_dialog_run(GTK_DIALOG(d));
	gtk_widget_destroy(d);
}

static int watch_confirm(struct main_window *w, const char *text)
{
	GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(w->window),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, "%s", text);
	int ret = gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_YES;
	gtk_widget_destroy(d);
	return ret;
}

static void handle_watch_save(GtkButton *b, struct main_window *w)
{
	UNUSED(b);
	if(!w->controls_active) return;

	const char *sel = selected_watch(w);
	gchar *prefill = sel ? g_strdup(sel) : watch_remembered(w);

	GtkWidget *dialog = gtk_dialog_new_with_buttons("Save watch", GTK_WINDOW(w->window),
			GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
			"_Cancel", GTK_RESPONSE_CANCEL,
			"_Save", GTK_RESPONSE_ACCEPT,
			NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

	GtkWidget *area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	gtk_container_set_border_width(GTK_CONTAINER(area), 10);
	gtk_box_set_spacing(GTK_BOX(area), 6);

	gchar *bph_text = w->bph ? g_strdup_printf("%d bph", w->bph) : g_strdup("bph guess");
	gchar *text = g_strdup_printf("Save the current settings (%s, lift angle %g deg) as:", bph_text, w->la);
	GtkWidget *label = gtk_label_new(text);
	gtk_label_set_xalign(GTK_LABEL(label), 0);
	gtk_box_pack_start(GTK_BOX(area), label, FALSE, FALSE, 0);
	g_free(text);
	g_free(bph_text);

	GtkWidget *entry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(entry), WATCH_NAME_MAX);
	gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
	gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Watch name");
	if(prefill) gtk_entry_set_text(GTK_ENTRY(entry), prefill);
	gtk_box_pack_start(GTK_BOX(area), entry, FALSE, FALSE, 0);
	g_free(prefill);

	gtk_widget_show_all(area);
	gtk_widget_grab_focus(entry);

	gchar *name = NULL;
	while(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		gchar *t = g_strstrip(g_strdup(gtk_entry_get_text(GTK_ENTRY(entry))));
		if(!watch_name_valid(t)) {
			gchar *msg = g_strdup_printf("Please enter a name of up to %d characters, without '[' or ']'.", WATCH_NAME_MAX);
			watch_message(w, GTK_MESSAGE_ERROR, msg);
			g_free(msg);
			g_free(t);
			continue;
		}
		if(watch_exists(w, t)) {
			gchar *msg = g_strdup_printf("A watch named \"%s\" already exists.\nReplace it with the current settings?", t);
			int ok = watch_confirm(w, msg);
			g_free(msg);
			if(!ok) {
				g_free(t);
				continue;
			}
		}
		name = t;
		break;
	}
	gtk_widget_destroy(dialog);

	if(name) {
		watch_remember(w, name);
		set_watch(w, name, w->bph, w->la); // also writes the config file
		watch_populate(w, name);
		g_free(name);
	}
}

static void handle_watch_delete(GtkButton *b, struct main_window *w)
{
	UNUSED(b);
	if(!w->controls_active) return;
	const char *sel = selected_watch(w);
	if(!sel) return;

	gchar *name = g_strdup(sel);
	gchar *msg = g_strdup_printf("Delete the saved watch \"%s\"?\nThe current bph and lift angle are kept.", name);
	if(watch_confirm(w, msg)) {
		gchar *remembered = watch_remembered(w);
		if(remembered && !strcmp(remembered, name))
			watch_remember(w, NULL);
		g_free(remembered);
		delete_watch(w, name); // also writes the config file
		watch_populate(w, NULL);
	}
	g_free(msg);
	g_free(name);
}

static void handle_cal_change(GtkSpinButton *b, struct main_window *w)
{
	if(!w->controls_active) return;
	int cal = gtk_spin_button_get_value(b);
	w->cal = cal;
	refresh_results(w);
	gtk_widget_queue_draw(w->notebook);
}

/* The audio input is a button with the device name, cut to AUDIO_NAME_CHARS
 * characters (its tooltip has the full name). Opening it lists every device in full. */
#define AUDIO_NAME_CHARS 24

static void audio_show(struct main_window *w, const char *name, int is_default)
{
	gtk_label_set_text(GTK_LABEL(w->audio_label), name);
	gchar *tip = g_strconcat(name, is_default ? " (default device)" : "", NULL);
	gtk_widget_set_tooltip_text(w->audio_combo_box, tip);
	g_free(tip);
}

static void audio_refresh_selection(struct main_window *w)
{
	int *devices = NULL;
	char **names = NULL;
	int count = 0;
	int default_device = AUDIO_DEVICE_DEFAULT;
	if(!list_audio_input_devices(&devices, &names, &count, &default_device)) {
		for(int i = 0; i < count; i++) {
			if(devices[i] == w->audio_device) {
				audio_show(w, names[i], devices[i] == default_device);
				break;
			}
		}
		free_audio_input_devices(devices, names, count);
	} else {
		audio_show(w, "No audio input", 0);
	}
}

/* Use a device: the same steps the old combo box took when its selection changed */
static void audio_apply_device(struct main_window *w, int device)
{
	if(!w->controls_active || device == w->audio_device) return;
	w->pending_audio_device = device;
	w->pending_audio_rate = w->nominal_sr;
	w->restart_audio = 1;
	gtk_widget_set_tooltip_text(w->audio_combo_box, "Opening audio input...");
	gtk_widget_set_sensitive(w->audio_combo_box, FALSE);
	gtk_widget_set_sensitive(w->sample_rate_combo_box, FALSE);

	lock_computer(w->computer);
	if(w->computer->recompute >= 0)
		kill_computer(w);
	unlock_computer(w->computer);
}

static void handle_audio_pick(GtkButton *item, struct main_window *w)
{
	GtkWidget *popover = gtk_widget_get_ancestor(GTK_WIDGET(item), GTK_TYPE_POPOVER);
	if(popover) gtk_widget_hide(popover);

	audio_apply_device(w, GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "device")));
}

static void handle_sample_rate_change(GtkComboBox *b, struct main_window *w)
{
	if(!w->controls_active) return;
	const gchar *id = gtk_combo_box_get_active_id(b);
	if(!id) return;

	char *end = NULL;
	long sample_rate = strtol(id, &end, 10);
	if(!end || *end || sample_rate <= 0)
		return;
	if((int)sample_rate == w->nominal_sr)
		return;

	w->pending_audio_device = w->audio_device;
	w->pending_audio_rate = (int)sample_rate;
	w->restart_audio = 1;
	gtk_widget_set_sensitive(w->audio_combo_box, FALSE);
	gtk_widget_set_sensitive(w->sample_rate_combo_box, FALSE);

	lock_computer(w->computer);
	if(w->computer->recompute >= 0)
		kill_computer(w);
	unlock_computer(w->computer);
}

static gboolean output_cal(GtkSpinButton *spin, gpointer data)
{
	UNUSED(data);
	GtkAdjustment *adj;
	gchar *text;
	int value;

	adj = gtk_spin_button_get_adjustment (spin);
	value = (int)gtk_adjustment_get_value (adj);
	text = g_strdup_printf ("%c%d.%d", value < 0 ? '-' : '+', abs(value)/10, abs(value)%10);
	gtk_entry_set_text (GTK_ENTRY (spin), text);
	g_free (text);

	return TRUE;
}

static gboolean input_cal(GtkSpinButton *spin, double *val, gpointer data)
{
	UNUSED(data);
	double x = 0;
	sscanf(gtk_entry_get_text (GTK_ENTRY (spin)), "%lf", &x);
	int n = round(x*10);
	if(n < MIN_CAL) n = MIN_CAL;
	if(n > MAX_CAL) n = MAX_CAL;
	*val = n;

	return TRUE;
}

static void on_shutdown(GApplication *app, void *p)
{
	UNUSED(p);
	debug("Main loop has terminated\n");
	struct main_window *w = g_object_get_data(G_OBJECT(app), "main-window");
	if(w) {
		save_config(w);
		if(w->computer)
			computer_destroy(w->computer);
		op_destroy(w->active_panel);
		close_config(w);
		free(w);
	}
	terminate_portaudio();
}

static void recompute(struct main_window *w);
static void computer_callback(void *w);

static void remove_runtime_timers(struct main_window *w)
{
	if(w->kick_timeout) {
		g_source_remove(w->kick_timeout);
		w->kick_timeout = 0;
	}
	if(w->save_timeout) {
		g_source_remove(w->save_timeout);
		w->save_timeout = 0;
	}
}

static guint computer_terminated(struct main_window *w)
{
	if(w->zombie) {
		debug("Closing main window\n");
		gtk_widget_destroy(w->window);
	} else {
		debug("Restarting computer");

		if(w->restart_audio) {
			int requested_device = w->pending_audio_device;
			int requested_rate = w->pending_audio_rate;
			int actual_rate = requested_rate;
			double real_rate = 0;
			int failed = 0;
			terminate_portaudio();
			set_audio_input_device(requested_device);
			set_audio_sample_rate(requested_rate);
			failed = start_portaudio(&actual_rate, &real_rate);
			if(failed && requested_device != AUDIO_DEVICE_DEFAULT) {
				terminate_portaudio();
				set_audio_input_device(AUDIO_DEVICE_DEFAULT);
				set_audio_sample_rate(requested_rate);
				actual_rate = requested_rate;
				failed = start_portaudio(&actual_rate, &real_rate);
			}
			if(failed && requested_rate != PA_SAMPLE_RATE) {
				terminate_portaudio();
				set_audio_input_device(AUDIO_DEVICE_DEFAULT);
				set_audio_sample_rate(PA_SAMPLE_RATE);
				actual_rate = PA_SAMPLE_RATE;
				failed = start_portaudio(&actual_rate, &real_rate);
			}
			if(failed) {
				terminate_portaudio();
				w->audio_device = AUDIO_DEVICE_DEFAULT;
				w->nominal_sr = PA_SAMPLE_RATE;
				set_audio_input_device(w->audio_device);
				set_audio_sample_rate(w->nominal_sr);
				audio_show(w, "No audio input", 0);
				error("Could not open the requested audio input; select another device to retry.");
			} else {
				w->audio_device = get_audio_input_device();
				w->nominal_sr = get_audio_sample_rate();
				audio_refresh_selection(w);
				if(w->audio_device != requested_device || w->nominal_sr != requested_rate)
					error("The requested audio input or rate is unavailable; using a fallback.");
			}
			w->pending_audio_device = w->audio_device;
			w->pending_audio_rate = w->nominal_sr;
			w->restart_audio = 0;
			if(w->sample_rate_combo_box) {
				char id[32];
				sprintf(id, "%d", w->nominal_sr);
				gtk_combo_box_set_active_id(GTK_COMBO_BOX(w->sample_rate_combo_box), id);
			}
		}

		struct computer *c = start_computer(w->nominal_sr, w->bph, w->la, w->cal, w->is_light);
		if(!c) {
			remove_runtime_timers(w);
			w->zombie = 1;
			error("Failed to restart computation thread");
			gtk_widget_destroy(w->window);
		} else {
			computer_destroy(w->computer);
			w->active_panel->computer = w->computer = c;

			w->computer->callback = computer_callback;
			w->computer->callback_data = w;

			recompute(w);
			update_sensitivity(w);
		}
	}
	return FALSE;
}

static void computer_quit(void *w)
{
	gdk_threads_add_idle((GSourceFunc)computer_terminated,w);
}

static void kill_computer(struct main_window *w)
{
	w->computer->recompute = -1;
	w->computer->callback = computer_quit;
	w->computer->callback_data = w;
}

static gboolean quit(struct main_window *w)
{
	remove_runtime_timers(w);
	w->zombie = 1;
	if(w->computer) {
		lock_computer(w->computer);
		kill_computer(w);
		unlock_computer(w->computer);
	} else {
		gtk_widget_destroy(w->window);
	}
	return FALSE;
}

static gboolean delete_event(GtkWidget *widget, GdkEvent *event, gpointer w)
{
	UNUSED(widget);
	UNUSED(event);
	debug("Received delete event\n");
	quit((struct main_window *)w);
	return TRUE;
}

static void handle_quit(GtkMenuItem *m, struct main_window *w)
{
	UNUSED(m);
	quit(w);
}

static void recompute(struct main_window *w)
{
	w->computer_timeout = 0;
	lock_computer(w->computer);
	if(w->computer->recompute >= 0) {
		if(w->is_light != w->computer->actv->is_light) {
			kill_computer(w);
		} else {
			w->computer->bph = w->bph;
			w->computer->la = w->la;
			w->computer->calibrate = w->calibrate;
			w->computer->recompute = 1;
		}
	}
	unlock_computer(w->computer);
}

static guint kick_computer(struct main_window *w)
{
	w->computer_timeout++;
	if(w->calibrate && w->computer_timeout < 10) {
		return TRUE;
	} else {
		recompute(w);
		return TRUE;
	}
}

/* Menu items whose availability depends on the program state (e.g. "Save all
 * snapshots" only with snapshots open) remember the wanted state here, so that
 * calibration can lock them and later restore them correctly. */
static void set_menu_item_enabled(struct main_window *w, GtkWidget *item, int enabled)
{
	g_object_set_data(G_OBJECT(item), "enabled-wanted", GINT_TO_POINTER(enabled ? 1 : 2));
	gtk_widget_set_sensitive(item, enabled && !w->calibrate);
}

static int menu_item_wanted(GtkWidget *item)
{
	return GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "enabled-wanted")) != 2;
}

/* Apply widget sensitivity from controls_active (real-time tab shown) and calibrate */
static void update_sensitivity(struct main_window *w)
{
	int live = w->controls_active;
	int settings = live && !w->calibrate;

	watch_update_buttons(w);
	gtk_widget_set_sensitive(w->bph_combo_box, settings);
	gtk_widget_set_sensitive(w->la_spin_button, settings);
	gtk_widget_set_sensitive(w->cal_spin_button, settings);
	// Changing input or rate would restart audio and invalidate the calibration
	gtk_widget_set_sensitive(w->audio_combo_box, settings);
	gtk_widget_set_sensitive(w->sample_rate_combo_box, settings);

	// Always allow stopping a running calibration, whichever tab is shown
	gtk_widget_set_sensitive(w->cal_button, live || w->calibrate);
	gtk_menu_item_set_label(GTK_MENU_ITEM(w->cal_button),
			w->calibrate ? "Stop calibration" : "Start calibration");

	// Other menu entries are locked while calibrating (Quit stays available)
	gtk_widget_set_sensitive(w->open_item, !w->calibrate);
	gtk_widget_set_sensitive(w->light_item, !w->calibrate);
	gtk_widget_set_sensitive(w->save_item, menu_item_wanted(w->save_item) && !w->calibrate);
	gtk_widget_set_sensitive(w->save_all_item, menu_item_wanted(w->save_all_item) && !w->calibrate);
	gtk_widget_set_sensitive(w->close_all_item, menu_item_wanted(w->close_all_item) && !w->calibrate);
}

static void handle_calibrate(GtkMenuItem *b, struct main_window *w)
{
	UNUSED(b);
	w->calibrate = !w->calibrate;
	update_sensitivity(w);
	recompute(w);
}

static void handle_light(GtkCheckMenuItem *b, struct main_window *w)
{
	int button_state = gtk_check_menu_item_get_active(b) == TRUE;
	if(button_state != w->is_light) {
		w->is_light = button_state;
		recompute(w);
	}
}

static void controls_active(struct main_window *w, int active)
{
	w->controls_active = active;
	update_sensitivity(w);
	if(active) {
		gtk_widget_show(w->snapshot_button);
		gtk_widget_hide(w->snapshot_name);
	} else {
		gtk_widget_hide(w->snapshot_button);
		gtk_widget_show(w->snapshot_name);
	}
}

static int blank_string(char *s)
{
	if(!s) return 1;
	for(;*s;s++)
		if(!isspace((unsigned char)*s)) return 0;
	return 1;
}

static void handle_tab_changed(GtkNotebook *nbk, GtkWidget *panel, guint x, struct main_window *w)
{
	UNUSED(nbk);
	UNUSED(x);
	// These are NULL for the Real Time tab
	struct output_panel *op = g_object_get_data(G_OBJECT(panel), "op-pointer");
	char *tab_name = g_object_get_data(G_OBJECT(panel), "tab-name");

	controls_active(w, !op);

	int bph, cal;
	double la;
	struct snapshot *snap;
	if(op) {
		gtk_entry_set_text(GTK_ENTRY(w->snapshot_name_entry), tab_name ? tab_name : "");
		show_position(w, op->snst->position);
		bph = op->snst->bph;
		cal = op->snst->cal;
		la = op->snst->la;
		snap = op->snst;
	} else {
		show_position(w, w->live_position);
		bph = w->bph;
		cal = w->cal;
		la = w->la;
		snap = w->active_snapshot;
	}

	set_bph_combo(w, bph);

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w->la_spin_button), la);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w->cal_spin_button), cal);
	set_menu_item_enabled(w, w->save_item, !snap->calibrate && snap->pb);

	// Snapshot tabs show their own bph/la, so no saved watch is selected there
	if(op)
		watch_select(w, NULL);
	else
		watch_restore(w);
}

static void handle_tab_closed(GtkNotebook *nbk, GtkWidget *panel, guint x, struct main_window *w)
{
	UNUSED(x);
	if(gtk_notebook_get_n_pages(nbk) == 1 && !w->zombie) {
		gtk_notebook_set_show_tabs(GTK_NOTEBOOK(nbk), FALSE);
		gtk_notebook_set_show_border(GTK_NOTEBOOK(nbk), FALSE);
		set_menu_item_enabled(w, w->save_all_item, FALSE);
		set_menu_item_enabled(w, w->close_all_item, FALSE);
	}
	// Now, are we sure that we are not going to segfault?
	struct output_panel *op = g_object_get_data(G_OBJECT(panel), "op-pointer");
	if(op) op_destroy(op);
	free(g_object_get_data(G_OBJECT(panel), "tab-name"));
}

static void handle_close_tab(GtkButton *b, struct output_panel *p)
{
	UNUSED(b);
	gtk_widget_destroy(p->panel);
}

/* ---- Watch position of snapshots ---- */

static const char *position_codes[POS_COUNT] = {
	NULL, "DU", "DD", "CU", "CD", "CL", "CR"
};

static const char *position_labels[POS_COUNT] = {
	"(not set)",
	"Dial up (DU)",
	"Dial down (DD)",
	"Crown up (CU)",
	"Crown down (CD)",
	"Crown left (CL)",
	"Crown right (CR)"
};

/* Tab title: "<name or Snapshot>" plus " (<position>)" when a position is set */
static void update_tab_label(GtkWidget *panel)
{
	GtkLabel *label = g_object_get_data(G_OBJECT(panel), "tab-label");
	struct output_panel *op = g_object_get_data(G_OBJECT(panel), "op-pointer");
	if(!label || !op) return;

	const char *name = g_object_get_data(G_OBJECT(panel), "tab-name");
	if(!name) name = "Snapshot";
	int pos = op->snst->position;

	if(pos > POS_NONE && pos < POS_COUNT) {
		gchar *text = g_strdup_printf("%s (%s)", name, position_codes[pos]);
		gtk_label_set_text(label, text);
		g_free(text);
	} else
		gtk_label_set_text(label, name);
}

static void handle_name_change(GtkEntry *e, struct main_window *w)
{
	int p = gtk_notebook_get_current_page(GTK_NOTEBOOK(w->notebook));
	GtkWidget *panel = gtk_notebook_get_nth_page(GTK_NOTEBOOK(w->notebook), p);
	if(!g_object_get_data(G_OBJECT(panel), "op-pointer")) return; // real-time tab
	free( g_object_get_data(G_OBJECT(panel), "tab-name") );
	char *name = (char *)gtk_entry_get_text(e);
	name = blank_string(name) ? NULL : strdup(name);
	g_object_set_data(G_OBJECT(panel), "tab-name", name);
	update_tab_label(panel);
}

static void handle_position_change(GtkComboBox *b, struct main_window *w)
{
	if(w->position_updating) return;
	int p = gtk_notebook_get_current_page(GTK_NOTEBOOK(w->notebook));
	GtkWidget *panel = gtk_notebook_get_nth_page(GTK_NOTEBOOK(w->notebook), p);
	struct output_panel *op = g_object_get_data(G_OBJECT(panel), "op-pointer");

	int pos = gtk_combo_box_get_active(b);
	if(pos < POS_NONE || pos >= POS_COUNT) pos = POS_NONE;
	if(!op) {
		// Real-time tab: remember it for the next snapshot
		w->live_position = pos;
		return;
	}
	op->snst->position = pos;
	update_tab_label(panel);
}

/* Show a position in the combo box without triggering a change */
static void show_position(struct main_window *w, int pos)
{
	if(pos < POS_NONE || pos >= POS_COUNT) pos = POS_NONE;
	w->position_updating++;
	gtk_combo_box_set_active(GTK_COMBO_BOX(w->snapshot_position_combo), pos);
	w->position_updating--;
}

#ifdef WIN_XP
static GtkWidget *image_from_file(char *filename)
{
	char *dir = g_win32_get_package_installation_directory_of_module(NULL);
	char *s;
	if(dir) {
		s = alloca( strlen(dir) + strlen(filename) + 2 );
		sprintf(s, "%s/%s", dir, filename);
	} else {
		s = filename;
	}
	GtkWidget *img = gtk_image_new_from_file(s);
	g_free(dir);
	return img;
}
#endif

static GtkWidget *make_tab_label(char *name, struct output_panel *panel_to_close)
{
	GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

	char *nm = panel_to_close ? name ? name : "Snapshot" : "Real time";
	GtkWidget *label = gtk_label_new(nm);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);

	if(panel_to_close) {
#ifdef WIN_XP
		GtkWidget *image = image_from_file("window-close.png");
#else
		GtkWidget *image = gtk_image_new_from_icon_name("window-close-symbolic", GTK_ICON_SIZE_MENU);
#endif
		GtkWidget *button = gtk_button_new();
		gtk_button_set_image(GTK_BUTTON(button), image);
		gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
		g_signal_connect(button, "clicked", G_CALLBACK(handle_close_tab), panel_to_close);
		gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
		g_object_set_data(G_OBJECT(panel_to_close->panel), "op-pointer", panel_to_close);
		g_object_set_data(G_OBJECT(panel_to_close->panel), "tab-label", label);
		g_object_set_data(G_OBJECT(panel_to_close->panel), "tab-name", name ? strdup(name) : NULL);
		update_tab_label(panel_to_close->panel);
	}

	gtk_widget_show_all(hbox);

	return hbox;
}

static void handle_paperstrip_settings_changed(void *data, double tick_seconds,
	double window_seconds)
{
	struct main_window *w = data;
	w->paperstrip_tick_seconds = tick_seconds;
	w->paperstrip_window_seconds = window_seconds;
	save_on_change(w);
}

static void add_new_tab(struct snapshot *s, char *name, struct main_window *w)
{
	struct output_panel *op = init_output_panel(NULL, s, 5);
	op_set_paperstrip_settings(op, w->active_panel->paperstrip_tick_seconds,
		w->active_panel->paperstrip_window_seconds);
	GtkWidget *label = make_tab_label(name, op);
	gtk_widget_show_all(op->panel);

	op_set_border(w->active_panel, 5);
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(w->notebook), TRUE);
	gtk_notebook_set_show_border(GTK_NOTEBOOK(w->notebook), TRUE);
	gtk_notebook_append_page(GTK_NOTEBOOK(w->notebook), op->panel, label);
	gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(w->notebook), op->panel, TRUE);
	set_menu_item_enabled(w, w->save_all_item, TRUE);
	set_menu_item_enabled(w, w->close_all_item, TRUE);
}

static void handle_snapshot(GtkButton *b, struct main_window *w)
{
	UNUSED(b);
	if(w->active_snapshot->calibrate) return;
	struct snapshot *s = snapshot_clone(w->active_snapshot);
	s->timestamp = get_timestamp(s->is_light);
	s->audio_status = w->active_panel->audio_status;
	s->position = w->live_position; // can still be changed on the snapshot tab
	// Name the snapshot after the selected watch, if any ("Snapshot" otherwise)
	add_new_tab(s, (char *)selected_watch(w), w);
}

/* Keyboard shortcut for the most frequent action: Ctrl+Space takes a snapshot.
 * A chord is used because a plain key would clash with typing in the entries
 * and with the focused button. It presses the button itself, so it is only
 * available when the button is: real-time tab, measurable signal, not calibrating. */
static gboolean handle_key_press(GtkWidget *widget, GdkEventKey *event, struct main_window *w)
{
	UNUSED(widget);
	guint mods = event->state & gtk_accelerator_get_default_mod_mask();
	if(mods == GDK_CONTROL_MASK && event->keyval == GDK_KEY_space) {
		if(gtk_widget_get_visible(w->snapshot_button) && gtk_widget_get_sensitive(w->snapshot_button))
			gtk_button_clicked(GTK_BUTTON(w->snapshot_button));
		return TRUE;
	}
	return FALSE;
}

static void chooser_set_filters(GtkFileChooser *chooser)
{
	GtkFileFilter *tgj_filter = gtk_file_filter_new();
	gtk_file_filter_set_name(tgj_filter, ".tgj");
	gtk_file_filter_add_pattern(tgj_filter, "*.tgj");
	gtk_file_chooser_add_filter(chooser, tgj_filter);

	GtkFileFilter *all_filter = gtk_file_filter_new();
	gtk_file_filter_set_name(all_filter, "All files");
	gtk_file_filter_add_pattern(all_filter, "*");
	gtk_file_chooser_add_filter(chooser, all_filter);

	// On windows seems not to work...
	gtk_file_chooser_set_filter(chooser, tgj_filter);
}

static FILE *fopen_check(char *filename, char *mode, struct main_window *w)
{
	FILE *f = NULL;

#ifdef _WIN32
	wchar_t *name = NULL;
	wchar_t *md = NULL;

	name = (wchar_t*)g_convert(filename, -1, "UTF-16LE", "UTF-8", NULL, NULL, NULL);
	if(!name) goto error;

	md = (wchar_t*)g_convert(mode, -1, "UTF-16LE", "UTF-8", NULL, NULL, NULL);
	if(!md) goto error;

	f = _wfopen(name, md);

error:	g_free(name);
	g_free(md);
#else
	f = fopen(filename, mode);
#endif

	if(!f) {
		GtkWidget *dialog;
		dialog = gtk_message_dialog_new(GTK_WINDOW(w->window),0,GTK_MESSAGE_ERROR,GTK_BUTTONS_CLOSE,
					"Error opening file\n");
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
	}

	return f;
}

static FILE *choose_file_for_save(struct main_window *w, char *title, char *suggestion)
{
	FILE *f = NULL;
	GtkWidget *dialog = gtk_file_chooser_dialog_new (title,
			GTK_WINDOW(w->window),
			GTK_FILE_CHOOSER_ACTION_SAVE,
			"Cancel",
			GTK_RESPONSE_CANCEL,
			"Save",
			GTK_RESPONSE_ACCEPT,
			NULL);
	GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
	if(suggestion)
		gtk_file_chooser_set_current_name(chooser, suggestion);

	chooser_set_filters(chooser);

	if(GTK_RESPONSE_ACCEPT == gtk_dialog_run (GTK_DIALOG (dialog)))
	{
		GFile *gf = gtk_file_chooser_get_file(chooser);
		char *filename = g_file_get_path(gf);
		g_object_unref(gf);
		if(!strcmp(".tgj", gtk_file_filter_get_name(gtk_file_chooser_get_filter(chooser)))) {
			char *s = strdup(filename);
			if(strlen(s) > 3 && strcasecmp(".tgj", s + strlen(s) - 4)) {
				char *t = g_malloc(strlen(filename)+5);
				sprintf(t,"%s.tgj",filename);
				g_free(filename);
				filename = t;
			}
			free(s);
		}
		struct stat stst;
		int do_open = 0;
		if(!stat(filename, &stst)) {
			GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(w->window),
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_QUESTION,
				GTK_BUTTONS_OK_CANCEL,
				"File %s already exists. Do you want to replace it?",
				filename);
			do_open = GTK_RESPONSE_OK == gtk_dialog_run(GTK_DIALOG(dialog));
			gtk_widget_destroy(dialog);
		} else
			do_open = 1;
		if(do_open) {
			f = fopen_check(filename, "wb", w);
			if(f) {
				char *uri = g_filename_to_uri(filename,NULL,NULL);
				if(f && uri)
					gtk_recent_manager_add_item(
						gtk_recent_manager_get_default(), uri);
				g_free(uri);
			}
		}
		g_free (filename);
	}

	gtk_widget_destroy(dialog);

	return f;
}

static void save_current(GtkMenuItem *m, struct main_window *w)
{
	UNUSED(m);
	int p = gtk_notebook_get_current_page(GTK_NOTEBOOK(w->notebook));
	GtkWidget *tab = gtk_notebook_get_nth_page(GTK_NOTEBOOK(w->notebook), p);
	struct output_panel *op = g_object_get_data(G_OBJECT(tab), "op-pointer");
	struct snapshot *snapshot = op ? op->snst : w->active_snapshot;
	char *name = g_object_get_data(G_OBJECT(tab), "tab-name");

	if(snapshot->calibrate || !snapshot->pb) return;

	snapshot = snapshot_clone(snapshot);

	if(!snapshot->timestamp)
		snapshot->timestamp = get_timestamp(snapshot->is_light);

	FILE *f = choose_file_for_save(w, "Save current display", name);

	if(f) {
		if(write_file(f, &snapshot, &name, 1)) {
			GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(w->window),0,GTK_MESSAGE_ERROR,GTK_BUTTONS_CLOSE,
						"Error writing file");
			gtk_dialog_run(GTK_DIALOG(dialog));
			gtk_widget_destroy(dialog);
		}
		fclose(f);
	}

	snapshot_destroy(snapshot);
}

static void close_all(GtkMenuItem *m, struct main_window *w)
{
	UNUSED(m);
	int i = 0;
	while(i < gtk_notebook_get_n_pages(GTK_NOTEBOOK(w->notebook))) {
		GtkWidget *tab = gtk_notebook_get_nth_page(GTK_NOTEBOOK(w->notebook), i);
		struct output_panel *op = g_object_get_data(G_OBJECT(tab), "op-pointer");
		if(!op) {  // This one is the real-time tab
			i++;
			continue;
		}
		gtk_widget_destroy(tab);
	}
}

static void save_all(GtkMenuItem *m, struct main_window *w)
{
	UNUSED(m);
	FILE *f = choose_file_for_save(w, "Save all snapshots", NULL);
	if(!f) return;

	int i, j, tabs = gtk_notebook_get_n_pages(GTK_NOTEBOOK(w->notebook));
	struct snapshot *s[tabs];
	char *names[tabs];

	for(i = j = 0; i < tabs; i++) {
		GtkWidget *tab = gtk_notebook_get_nth_page(GTK_NOTEBOOK(w->notebook), i);
		struct output_panel *op = g_object_get_data(G_OBJECT(tab), "op-pointer");
		if(!op) continue; // This one is the real-time tab
		s[j] = op->snst;
		names[j++] = g_object_get_data(G_OBJECT(tab), "tab-name");
	}

	if(write_file(f, s, names, j)) {
		GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(w->window),0,GTK_MESSAGE_ERROR,GTK_BUTTONS_CLOSE,
					"Error writing file");
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
	}

	fclose(f);
}

static void load_snapshots(FILE *f, char *name, struct main_window *w)
{
	struct snapshot **s;
	char **names;
	uint64_t cnt;
	if(!read_file(f, &s, &names, &cnt)) {
		uint64_t i;
		for(i = 0; i < cnt; i++) {
			add_new_tab(s[i], names[i] ? names[i] : name, w);
			free(names[i]);
		}
		free(s);
		free(names);
	} else {
		GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(w->window),0,GTK_MESSAGE_ERROR,GTK_BUTTONS_CLOSE,
					"Error reading file: %s", name);
		gtk_dialog_run(GTK_DIALOG(dialog));
		gtk_widget_destroy(dialog);
	}
}

static void load_from_file(char *filename, struct main_window *w)
{
	FILE *f = fopen_check(filename, "rb", w);
	if(f) {
		char *filename_cpy = strdup(filename);
		char *name = basename(filename_cpy);
		name = g_filename_to_utf8(name, -1, NULL, NULL, NULL);
		if(name && strlen(name) > 3 && !strcasecmp(".tgj", name + strlen(name) - 4))
			name[strlen(name) - 4] = 0;
		load_snapshots(f, name, w);
		free(filename_cpy);
		g_free(name);
		fclose(f);
	}
}

static void load(GtkMenuItem *m, struct main_window *w)
{
	UNUSED(m);
	GtkWidget *dialog = gtk_file_chooser_dialog_new ("Open",
			GTK_WINDOW(w->window),
			GTK_FILE_CHOOSER_ACTION_OPEN,
			"Cancel",
			GTK_RESPONSE_CANCEL,
			"Open",
			GTK_RESPONSE_ACCEPT,
			NULL);
	GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);

	chooser_set_filters(chooser);

	if(GTK_RESPONSE_ACCEPT == gtk_dialog_run (GTK_DIALOG (dialog)))
	{
		GFile *gf = gtk_file_chooser_get_file(chooser);
		char *filename = g_file_get_path(gf);
		g_object_unref(gf);
		load_from_file(filename, w);
		g_free (filename);
	}

	gtk_widget_destroy(dialog);
}

/* The top bar is three bars (watch, audio, snapshot) in a flow box: side by side
 * when the window is wide, wrapping to a second row when it is narrow. */
static GtkWidget *make_bar(struct main_window *w)
{
	GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
	gtk_container_set_border_width(GTK_CONTAINER(bar), 4);

	gtk_container_add(GTK_CONTAINER(w->bar_flow), bar);
	return bar;
}

/* Compact +/- buttons for the spin buttons of the top bar (class "compact"):
 * the default ones are much taller and wider than the number they change */
static void apply_compact_css(void)
{
	static const gchar css[] =
		"spinbutton.compact button { min-width: 14px; min-height: 0px; padding: 0px 2px; }";
	GtkCssProvider *provider = gtk_css_provider_new();
	gtk_css_provider_load_from_data(provider, css, -1, NULL);
	gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
		GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_object_unref(provider);
}

/* Set up the main window and populate with widgets */
static void init_main_window(struct main_window *w)
{
	apply_compact_css();
	w->window = gtk_application_window_new(w->app);

	gtk_widget_set_size_request(w->window, 950, 700);

	gtk_container_set_border_width(GTK_CONTAINER(w->window), 10);
	g_signal_connect(w->window, "delete_event", G_CALLBACK(delete_event), w);
	g_signal_connect(w->window, "key-press-event", G_CALLBACK(handle_key_press), w);

	gtk_window_set_title(GTK_WINDOW(w->window), PROGRAM_NAME " " VERSION);
	gtk_window_set_icon_name (GTK_WINDOW(w->window), PACKAGE);

	GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
	gtk_container_add(GTK_CONTAINER(w->window), vbox);

	// Top strip: the bars on the left, the command menu on the right
	GtkWidget *top_strip = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
	gtk_box_pack_start(GTK_BOX(vbox), top_strip, FALSE, FALSE, 0);

	w->bar_flow = gtk_flow_box_new();
	gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(w->bar_flow), GTK_SELECTION_NONE);
	gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(w->bar_flow), FALSE);
	gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(w->bar_flow), 16);
	gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(w->bar_flow), 6);
	gtk_box_pack_start(GTK_BOX(top_strip), w->bar_flow, TRUE, TRUE, 0);

	// Watch bar
	GtkWidget *hbox = make_bar(w);

	// Saved watch selector (populated once bph and lift angle widgets exist)
	GtkWidget *label = NULL;
	w->watch_combo_box = gtk_combo_box_text_new();
	gtk_widget_set_tooltip_text(w->watch_combo_box, "Saved watches (bph and lift angle)");
	gtk_box_pack_start(GTK_BOX(hbox), w->watch_combo_box, FALSE, FALSE, 0);
	w->watch_save_button = gtk_button_new_from_icon_name("document-save-symbolic", GTK_ICON_SIZE_BUTTON);
	gtk_widget_set_tooltip_text(w->watch_save_button, "Save the current bph and lift angle as a watch");
	gtk_box_pack_start(GTK_BOX(hbox), w->watch_save_button, FALSE, FALSE, 0);
	w->watch_delete_button = gtk_button_new_from_icon_name("edit-delete-symbolic", GTK_ICON_SIZE_BUTTON);
	gtk_widget_set_tooltip_text(w->watch_delete_button, "Delete the selected watch");
	gtk_box_pack_start(GTK_BOX(hbox), w->watch_delete_button, FALSE, FALSE, 0);

	// BPH label
	label = gtk_label_new("BEAT");
	gtk_widget_set_tooltip_text(label, "Beats per hour (bph)");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	// BPH combo box
	w->bph_combo_box = gtk_combo_box_text_new_with_entry();
	gtk_entry_set_width_chars(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(w->bph_combo_box))), 6);
	gtk_box_pack_start(GTK_BOX(hbox), w->bph_combo_box, FALSE, FALSE, 0);
	// Fill in pre-defined values
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(w->bph_combo_box), "guess");
	int i,current = 0;
	for(i = 0; preset_bph[i]; i++) {
		char s[100];
		sprintf(s,"%d", preset_bph[i]);
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(w->bph_combo_box), s);
		if(w->bph == preset_bph[i]) current = i+1;
	}
	if(current || w->bph == 0)
		gtk_combo_box_set_active(GTK_COMBO_BOX(w->bph_combo_box), current);
	else {
		char s[32];
		sprintf(s,"%d",w->bph);
		GtkEntry *e = GTK_ENTRY(gtk_bin_get_child(GTK_BIN(w->bph_combo_box)));
		gtk_entry_set_text(e,s);
	}
	g_signal_connect (w->bph_combo_box, "changed", G_CALLBACK(handle_bph_change), w);

	// Lift angle label
	label = gtk_label_new("L.A.");
	gtk_widget_set_tooltip_text(label, "Lift angle (degrees)");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	// Lift angle spin button
	w->la_spin_button = gtk_spin_button_new_with_range(MIN_LA, MAX_LA, 1);
	gtk_entry_set_width_chars(GTK_ENTRY(w->la_spin_button), 3);
	gtk_style_context_add_class(gtk_widget_get_style_context(w->la_spin_button), "compact");
	gtk_box_pack_start(GTK_BOX(hbox), w->la_spin_button, FALSE, FALSE, 0);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w->la_spin_button), w->la);
	g_signal_connect(w->la_spin_button, "value_changed", G_CALLBACK(handle_la_change), w);

	// Fill the saved watch selector and hook it up
	g_signal_connect(w->watch_combo_box, "changed", G_CALLBACK(handle_watch_change), w);
	g_signal_connect(w->watch_save_button, "clicked", G_CALLBACK(handle_watch_save), w);
	g_signal_connect(w->watch_delete_button, "clicked", G_CALLBACK(handle_watch_delete), w);
	watch_populate(w, NULL);
	watch_restore(w);

	// Audio bar
	hbox = make_bar(w);

	// Audio input: a button with the device name; opening it lists every device in full
	w->audio_combo_box = gtk_menu_button_new();
	gtk_box_pack_start(GTK_BOX(hbox), w->audio_combo_box, FALSE, FALSE, 0);

	w->audio_label = gtk_label_new("");
	gtk_label_set_max_width_chars(GTK_LABEL(w->audio_label), AUDIO_NAME_CHARS);
	gtk_label_set_ellipsize(GTK_LABEL(w->audio_label), PANGO_ELLIPSIZE_END);
	GtkWidget *audio_button_content = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_box_pack_start(GTK_BOX(audio_button_content), w->audio_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(audio_button_content),
		gtk_image_new_from_icon_name("pan-down-symbolic", GTK_ICON_SIZE_MENU), FALSE, FALSE, 0);
	gtk_container_add(GTK_CONTAINER(w->audio_combo_box), audio_button_content);

	GtkWidget *audio_list = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
	gtk_container_set_border_width(GTK_CONTAINER(audio_list), 6);
	GtkWidget *audio_popover = gtk_popover_new(w->audio_combo_box);
	gtk_container_add(GTK_CONTAINER(audio_popover), audio_list);
	gtk_menu_button_set_popover(GTK_MENU_BUTTON(w->audio_combo_box), audio_popover);

	int *audio_devices = NULL;
	char **audio_names = NULL;
	int audio_count = 0;
	int default_audio_device = AUDIO_DEVICE_DEFAULT;
	if(!list_audio_input_devices(&audio_devices, &audio_names, &audio_count, &default_audio_device)) {
		int i, chosen = w->audio_device != AUDIO_DEVICE_DEFAULT ? w->audio_device : default_audio_device;
		int found = 0;
		for(i = 0; i < audio_count; i++)
			if(audio_devices[i] == chosen) found = 1;
		if(!found && audio_count > 0)
			chosen = audio_devices[0];

		for(i = 0; i < audio_count; i++) {
			int is_default = audio_devices[i] == default_audio_device;
			gchar *full = g_strconcat(audio_names[i], is_default ? " (default)" : "", NULL);
			GtkWidget *item = gtk_button_new_with_label(full);
			g_free(full);
			gtk_button_set_relief(GTK_BUTTON(item), GTK_RELIEF_NONE);
			gtk_widget_set_halign(gtk_bin_get_child(GTK_BIN(item)), GTK_ALIGN_START);
			g_object_set_data(G_OBJECT(item), "device", GINT_TO_POINTER(audio_devices[i]));
			g_object_set_data(G_OBJECT(item), "default", GINT_TO_POINTER(is_default));
			g_object_set_data_full(G_OBJECT(item), "name", g_strdup(audio_names[i]), g_free);
			g_signal_connect(item, "clicked", G_CALLBACK(handle_audio_pick), w);
			gtk_box_pack_start(GTK_BOX(audio_list), item, FALSE, FALSE, 0);

			if(audio_devices[i] == chosen) {
				w->audio_device = chosen;
				audio_show(w, audio_names[i], is_default);
			}
		}
		free_audio_input_devices(audio_devices, audio_names, audio_count);
	}
	if(audio_count == 0)
		audio_show(w, "No audio device", 0);
	gtk_widget_show_all(audio_list);

	// Sample rate label
	label = gtk_label_new("rate");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	// Sample rate combo box
	w->sample_rate_combo_box = gtk_combo_box_text_new();
	gtk_box_pack_start(GTK_BOX(hbox), w->sample_rate_combo_box, FALSE, FALSE, 0);
	for(i = 0; available_sample_rates[i]; i++) {
		char id[32];
		sprintf(id, "%d", available_sample_rates[i]);
		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(w->sample_rate_combo_box), id, available_sample_rate_labels[i]);
	}
	{
		char id[32];
		sprintf(id, "%d", w->nominal_sr);
		if(!gtk_combo_box_set_active_id(GTK_COMBO_BOX(w->sample_rate_combo_box), id))
			gtk_combo_box_set_active(GTK_COMBO_BOX(w->sample_rate_combo_box), 2);
	}
	g_signal_connect(w->sample_rate_combo_box, "changed", G_CALLBACK(handle_sample_rate_change), w);

	// Calibration label
	label = gtk_label_new("cal");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	// Calibration spin button
	w->cal_spin_button = gtk_spin_button_new_with_range(MIN_CAL, MAX_CAL, 1);
	gtk_box_pack_start(GTK_BOX(hbox), w->cal_spin_button, FALSE, FALSE, 0);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w->cal_spin_button), w->cal);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w->cal_spin_button), FALSE);
	gtk_entry_set_width_chars(GTK_ENTRY(w->cal_spin_button), 5);
	gtk_style_context_add_class(gtk_widget_get_style_context(w->cal_spin_button), "compact");
	g_signal_connect(w->cal_spin_button, "value_changed", G_CALLBACK(handle_cal_change), w);
	g_signal_connect(w->cal_spin_button, "output", G_CALLBACK(output_cal), NULL);
	g_signal_connect(w->cal_spin_button, "input", G_CALLBACK(input_cal), NULL);

	// Snapshot bar
	hbox = make_bar(w);

	// Watch position: on the real-time tab it is used for the next snapshot,
	// on a snapshot tab it edits that snapshot (shown in the tab title, saved with it)
	w->snapshot_position_combo = gtk_combo_box_text_new();
	int pos;
	for(pos = POS_NONE; pos < POS_COUNT; pos++)
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(w->snapshot_position_combo), position_labels[pos]);
	gtk_combo_box_set_active(GTK_COMBO_BOX(w->snapshot_position_combo), w->live_position);
	gtk_widget_set_tooltip_text(w->snapshot_position_combo,
		"Position of the watch.\n"
		"\n"
		"Real time: applied to\n"
		"the next snapshot.\n"
		"Snapshot: changes the\n"
		"position of this one.\n"
		"\n"
		"Shown in the tab title\n"
		"and saved with the\n"
		"snapshot.");
	gtk_box_pack_start(GTK_BOX(hbox), w->snapshot_position_combo, FALSE, FALSE, 0);
	g_signal_connect(w->snapshot_position_combo, "changed", G_CALLBACK(handle_position_change), w);

	// Snapshot button
	w->snapshot_button = gtk_button_new_with_label("Take Snapshot");
	gtk_widget_set_tooltip_text(w->snapshot_button,
		"Keep the current\n"
		"measurement in a\n"
		"new tab.\n"
		"\n"
		"Shortcut: Ctrl+Space");
	gtk_box_pack_start(GTK_BOX(hbox), w->snapshot_button, FALSE, FALSE, 0);
	gtk_widget_set_sensitive(w->snapshot_button, FALSE);
	g_signal_connect(w->snapshot_button, "clicked", G_CALLBACK(handle_snapshot), w);

	// Snapshot name field
	GtkWidget *name_label = gtk_label_new("Current snapshot:");
	w->snapshot_name_entry = gtk_entry_new();
	w->snapshot_name = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
	gtk_box_pack_start(GTK_BOX(w->snapshot_name), name_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(w->snapshot_name), w->snapshot_name_entry, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), w->snapshot_name, FALSE, FALSE, 0);
	g_signal_connect(w->snapshot_name_entry, "changed", G_CALLBACK(handle_name_change), w);

	// Command menu
	GtkWidget *command_menu = gtk_menu_new();
	GtkWidget *command_menu_button = gtk_menu_button_new();
#ifdef WIN_XP
	GtkWidget *image = image_from_file("open-menu.png");
#else
	GtkWidget *image = gtk_image_new_from_icon_name("open-menu-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
#endif
	gtk_button_set_image(GTK_BUTTON(command_menu_button), image);
	g_object_set(G_OBJECT(command_menu_button), "direction", GTK_ARROW_DOWN, NULL);
	g_object_set(G_OBJECT(command_menu), "halign", GTK_ALIGN_END, NULL);
	gtk_menu_button_set_popup(GTK_MENU_BUTTON(command_menu_button), command_menu);
	gtk_box_pack_end(GTK_BOX(top_strip), command_menu_button, FALSE, FALSE, 0);
	
	// ... Open
	w->open_item = gtk_menu_item_new_with_label("Open");
	gtk_menu_shell_append(GTK_MENU_SHELL(command_menu), w->open_item);
	g_signal_connect(w->open_item, "activate", G_CALLBACK(load), w);

	// ... Save
	w->save_item = gtk_menu_item_new_with_label("Save current display");
	gtk_menu_shell_append(GTK_MENU_SHELL(command_menu), w->save_item);
	g_signal_connect(w->save_item, "activate", G_CALLBACK(save_current), w);
	set_menu_item_enabled(w, w->save_item, FALSE);

	// ... Save all
	w->save_all_item = gtk_menu_item_new_with_label("Save all snapshots");
	gtk_menu_shell_append(GTK_MENU_SHELL(command_menu), w->save_all_item);
	g_signal_connect(w->save_all_item, "activate", G_CALLBACK(save_all), w);
	set_menu_item_enabled(w, w->save_all_item, FALSE);

	gtk_menu_shell_append(GTK_MENU_SHELL(command_menu), gtk_separator_menu_item_new());

	// ... Light checkbox
	GtkWidget *light_checkbox = gtk_check_menu_item_new_with_label("Light algorithm");
	w->light_item = light_checkbox;
	gtk_widget_set_tooltip_text(light_checkbox,
		"Reduces CPU load, \n"
		"for slow computers.\n"
		"\n"
		"It processes half of\n"
		"the audio samples, \n"
		"uses shorter analysis\n"
		"windows and skips the\n"
		"noise suppressor.\n"
		"Results update faster,\n"
		"but they are less stable\n"
		"and more sensitive to \n"
		"background noise.\n"
		"Toggling this setting will\n"
		"restart your measurement."
		);
	gtk_menu_shell_append(GTK_MENU_SHELL(command_menu), light_checkbox);
	g_signal_connect(light_checkbox, "toggled", G_CALLBACK(handle_light), w);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(light_checkbox), w->is_light);

	// ... Start / Stop calibration (label switches with the state)
	w->cal_button = gtk_menu_item_new_with_label("Start calibration");
	gtk_widget_set_tooltip_text(w->cal_button,
		"Measures the error of\n"
		"your sound card clock,\n"
		"using a quartz watch\n"
		"ticking once per second\n"
		"as reference.\n"
		"\n"
		"Takes about 15 minutes.\n"
		"The result is stored\n"
		"in the cal field."
		);
	gtk_menu_shell_append(GTK_MENU_SHELL(command_menu), w->cal_button);
	g_signal_connect(w->cal_button, "activate", G_CALLBACK(handle_calibrate), w);

	gtk_menu_shell_append(GTK_MENU_SHELL(command_menu), gtk_separator_menu_item_new());

	// ... Close all
	w->close_all_item = gtk_menu_item_new_with_label("Close all snapshots");
	gtk_menu_shell_append(GTK_MENU_SHELL(command_menu), w->close_all_item);
	g_signal_connect(w->close_all_item, "activate", G_CALLBACK(close_all), w);
	set_menu_item_enabled(w, w->close_all_item, FALSE);

	// ... Quit
	GtkWidget *quit_item = gtk_menu_item_new_with_label("Quit");
	gtk_menu_shell_append(GTK_MENU_SHELL(command_menu), quit_item);
	g_signal_connect(quit_item, "activate", G_CALLBACK(handle_quit), w);

	gtk_widget_show_all(command_menu);

	// The tabs' container
	w->notebook = gtk_notebook_new();
	gtk_box_pack_start(GTK_BOX(vbox), w->notebook, TRUE, TRUE, 0);
	gtk_notebook_set_scrollable(GTK_NOTEBOOK(w->notebook), TRUE);
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(w->notebook), FALSE);
	gtk_notebook_set_show_border(GTK_NOTEBOOK(w->notebook), FALSE);
	g_signal_connect(w->notebook, "page-removed", G_CALLBACK(handle_tab_closed), w);
	g_signal_connect_after(w->notebook, "switch-page", G_CALLBACK(handle_tab_changed), w);

	// The main tab
	GtkWidget *tab_label = make_tab_label(NULL, NULL);
	gtk_notebook_append_page(GTK_NOTEBOOK(w->notebook), w->active_panel->panel, tab_label);
	gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(w->notebook), w->active_panel->panel, TRUE);

	gtk_window_maximize(GTK_WINDOW(w->window));
	gtk_widget_show_all(w->window);
	gtk_widget_hide(w->snapshot_name);
	gtk_window_set_focus(GTK_WINDOW(w->window), NULL);
}

guint save_on_change_timer(struct main_window *w)
{
	save_on_change(w);
	return TRUE;
}

guint refresh(struct main_window *w)
{
	if(!w->computer)
		return FALSE;
	lock_computer(w->computer);
	struct snapshot *s = w->computer->curr;
	if(s) {
		double trace_centering = w->active_snapshot->trace_centering;
		double trace_zoom = w->active_snapshot->trace_zoom;
		snapshot_destroy(w->active_snapshot);
		w->active_snapshot = s;
		w->computer->curr = NULL;
		s->trace_centering = trace_centering;
		s->trace_zoom = trace_zoom > 0 ? trace_zoom : 1.0;
		if(w->computer->clear_trace && !s->calibrate) {
			memset(s->events,0,s->events_count*sizeof(uint64_t));
			memset(s->events_tictoc,0,s->events_count*sizeof(unsigned char));
			memset(s->amps,0,s->amps_count*sizeof(*s->amps));
			memset(s->amps_time,0,s->amps_count*sizeof(*s->amps_time));
		}
		if(s->calibrate && s->cal_state == 1 && s->cal_result != w->cal) {
			w->cal = s->cal_result;
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(w->cal_spin_button), s->cal_result);
		}
	}
	unlock_computer(w->computer);
	refresh_results(w);
	op_set_snapshot(w->active_panel, w->active_snapshot);

	int p = gtk_notebook_get_current_page(GTK_NOTEBOOK(w->notebook));
	GtkWidget *panel = gtk_notebook_get_nth_page(GTK_NOTEBOOK(w->notebook), p);
	int photogenic = 0;
	if(!g_object_get_data(G_OBJECT(panel), "op-pointer")) {
		photogenic = !w->active_snapshot->calibrate && w->active_snapshot->pb;
		set_menu_item_enabled(w, w->save_item, photogenic);
		gtk_widget_queue_draw(w->notebook);
	}
	gtk_widget_set_sensitive(w->snapshot_button, photogenic);
	return FALSE;
}

static void computer_callback(void *w)
{
	gdk_threads_add_idle((GSourceFunc)refresh,w);
}

static void start_interface(GApplication* app, void *p)
{
	UNUSED(p);
	double real_sr;

	initialize_palette();

	struct main_window *w = malloc(sizeof(struct main_window));
	w->app = GTK_APPLICATION(app);

	w->zombie = 0;
	w->controls_active = 1;
	w->cal = MIN_CAL - 1;
	w->bph = 0;
	w->la = DEFAULT_LA;
	w->calibrate = 0;
	w->is_light = 0;
	w->paperstrip_tick_seconds = 10.0;
	w->paperstrip_window_seconds = 60.0;
	w->nominal_sr = PA_SAMPLE_RATE;
	w->audio_device = AUDIO_DEVICE_DEFAULT;
	w->restart_audio = 0;
	w->pending_audio_device = AUDIO_DEVICE_DEFAULT;
	w->pending_audio_rate = PA_SAMPLE_RATE;
	w->watch_updating = 0;
	w->position_updating = 0;
	w->live_position = POS_NONE;

	load_config(w);
	if(!isfinite(w->paperstrip_tick_seconds) || w->paperstrip_tick_seconds < 0.01 ||
		w->paperstrip_tick_seconds > 3600.0)
		w->paperstrip_tick_seconds = 10.0;
	if(!isfinite(w->paperstrip_window_seconds) || w->paperstrip_window_seconds < 10.0 ||
		w->paperstrip_window_seconds > 86400.0)
		w->paperstrip_window_seconds = 60.0;
	/* Always start with the default input; some drivers block while opening. */
	w->audio_device = AUDIO_DEVICE_DEFAULT;
	set_audio_input_device(w->audio_device);
	set_audio_sample_rate(w->nominal_sr);

	if(start_portaudio(&w->nominal_sr, &real_sr)) {
		terminate_portaudio();
		w->nominal_sr = PA_SAMPLE_RATE;
		set_audio_sample_rate(w->nominal_sr);
		if(start_portaudio(&w->nominal_sr, &real_sr)) {
			g_application_quit(app);
			return;
		}
		error("Saved sample rate unavailable; using the default rate.");
	}
	w->audio_device = get_audio_input_device();
	w->nominal_sr = get_audio_sample_rate();

	if(w->la < MIN_LA || w->la > MAX_LA) w->la = DEFAULT_LA;
	if(w->bph < MIN_BPH || w->bph > MAX_BPH) w->bph = 0;
	if(w->cal < MIN_CAL || w->cal > MAX_CAL)
		w->cal = (real_sr - w->nominal_sr) * (3600*24) / w->nominal_sr;

	w->computer_timeout = 0;

	w->computer = start_computer(w->nominal_sr, w->bph, w->la, w->cal, w->is_light);
	if(!w->computer) {
		error("Error starting computation thread");
		g_application_quit(app);
		return;
	}
	w->computer->callback = computer_callback;
	w->computer->callback_data = w;

	w->active_snapshot = w->computer->curr;
	w->computer->curr = NULL;
	compute_results(w->active_snapshot);

	w->active_panel = init_output_panel(w->computer, w->active_snapshot, 0);
	op_set_paperstrip_settings(w->active_panel, w->paperstrip_tick_seconds,
		w->paperstrip_window_seconds);
	op_set_paperstrip_settings_callback(w->active_panel, handle_paperstrip_settings_changed, w);

	init_main_window(w);

	w->kick_timeout = g_timeout_add_full(G_PRIORITY_LOW,100,(GSourceFunc)kick_computer,w,NULL);
	w->save_timeout = g_timeout_add_full(G_PRIORITY_LOW,10000,(GSourceFunc)save_on_change_timer,w,NULL);
#ifdef DEBUG
	if(testing)
		g_timeout_add_full(G_PRIORITY_LOW,3000,(GSourceFunc)quit,w,NULL);
#endif

	g_object_set_data(G_OBJECT(app), "main-window", w);
}

static void handle_activate(GApplication* app, void *p)
{
	UNUSED(p);
	struct main_window *w = g_object_get_data(G_OBJECT(app), "main-window");
	if(w) gtk_window_present(GTK_WINDOW(w->window));
}

static void handle_open(GApplication* app, GFile **files, int cnt, char *hint, void *p)
{
	UNUSED(hint);
	UNUSED(p);
	struct main_window *w = g_object_get_data(G_OBJECT(app), "main-window");
	if(w) {
		int i;
		for(i = 0; i < cnt; i++) {
			char *path = g_file_get_path(files[i]);
			// This partially works around a bug in XP (i.e. gtk+ bundle 3.6.4)
			path = g_locale_to_utf8(path, -1, NULL, NULL, NULL);
			if(!path) continue;
			load_from_file(path, w);
			g_free(path);
		}
		gtk_notebook_set_current_page(GTK_NOTEBOOK(w->notebook), -1);
		gtk_window_present(GTK_WINDOW(w->window));
	}
}

int main(int argc, char **argv)
{
	gtk_disable_setlocale();

#ifdef DEBUG
	if(argc > 1 && !strcmp("test",argv[1])) {
		testing = 1;
		argv++; argc--;
	}
#endif

	GtkApplication *app = gtk_application_new ("li.ciovil.tg", G_APPLICATION_HANDLES_OPEN);
	g_signal_connect (app, "startup", G_CALLBACK (start_interface), NULL);
	g_signal_connect (app, "activate", G_CALLBACK (handle_activate), NULL);
	g_signal_connect (app, "open", G_CALLBACK (handle_open), NULL);
	g_signal_connect (app, "shutdown", G_CALLBACK (on_shutdown), NULL);
	int ret = g_application_run (G_APPLICATION (app), argc, argv);
	g_object_unref (app);

	debug("Interface exited with status %d\n",ret);

	return ret;
}
