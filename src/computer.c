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

static int count_events_buffer(const uint64_t *events, int wp, int nevents)
{
	int i, cnt = 0;
	if(!nevents || !events || !events[wp]) return 0;
	for(i = wp; events[i];) {
		cnt++;
		if(--i < 0) i = nevents - 1;
		if(i == wp) break;
	}
	return cnt;
}

struct snapshot *snapshot_clone(struct snapshot *s)
{
	struct snapshot *t = malloc(sizeof(struct snapshot));
	memcpy(t,s,sizeof(struct snapshot));
	if(s->pb) t->pb = pb_clone(s->pb);
	t->events_count = count_events_buffer(s->events, s->events_wp, s->events_count);
	if(t->events_count) {
		t->events_wp = t->events_count - 1;
		t->events = malloc(t->events_count * sizeof(uint64_t));
		t->events_tictoc = malloc(t->events_count * sizeof(unsigned char));
		int i, j;
		for(i = t->events_wp, j = s->events_wp; i >= 0; i--) {
			t->events[i] = s->events[j];
			t->events_tictoc[i] = s->events_tictoc ? s->events_tictoc[j] : 0;
			if(--j < 0) j = s->events_count - 1;
		}
	} else {
		t->events_wp = 0;
		t->events = NULL;
		t->events_tictoc = NULL;
	}

	t->amps_count = count_events_buffer(s->amps_time, s->amps_wp, s->amps_count);
	if(t->amps_count) {
		t->amps_wp = t->amps_count - 1;
		t->amps = malloc(t->amps_count * sizeof(*t->amps));
		t->amps_time = malloc(t->amps_count * sizeof(*t->amps_time));
		int i, j;
		for(i = t->amps_wp, j = s->amps_wp; i >= 0; i--) {
			t->amps[i] = s->amps[j];
			t->amps_time[i] = s->amps_time[j];
			if(--j < 0) j = s->amps_count - 1;
		}
	} else {
		t->amps_wp = 0;
		t->amps = NULL;
		t->amps_time = NULL;
	}
	return t;
}

void snapshot_destroy(struct snapshot *s)
{
	if(s->pb) pb_destroy_clone(s->pb);
	free(s->amps_time);
	free(s->amps);
	free(s->events_tictoc);
	free(s->events);
	free(s);
}

static int guess_bph(double period)
{
	double bph = 7200 / period;
	double min = bph;
	int i,ret;

	ret = 0;
	for(i=0; preset_bph[i]; i++) {
		double diff = fabs(bph - preset_bph[i]);
		if(diff < min) {
			min = diff;
			ret = i;
		}
	}

	return preset_bph[ret];
}

static void compute_update_cal(struct computer *c)
{
	c->actv->signal = analyze_pa_data_cal(c->pdata, c->cdata);
	if(c->actv->pb) {
		pb_destroy_clone(c->actv->pb);
		c->actv->pb = NULL;
	}
	c->actv->cal_state = c->cdata->state;
	c->actv->cal_percent = 100*c->cdata->wp/c->cdata->size;
	if(c->cdata->state == 1)
		c->actv->cal_result = round(10 * c->cdata->calibration);
}

static void compute_update(struct computer *c)
{
	int signal = analyze_pa_data(c->pdata, c->actv->bph, c->actv->la, c->actv->events_from);
	struct processing_buffers *p = c->pdata->buffers;
	int i;
	for(i=0; i<NSTEPS && p[i].ready; i++);
	for(i--; i>=0 && p[i].sigma > p[i].period / 10000; i--);
	if(i>=0) {
		if(c->actv->pb) pb_destroy_clone(c->actv->pb);
		c->actv->pb = pb_clone(&p[i]);
		c->actv->is_old = 0;
		c->actv->signal = i == NSTEPS-1 && p[i].amp < 0 ? signal-1 : signal;
	} else {
		c->actv->is_old = 1;
		c->actv->signal = -signal;
	}
}

static void compute_events_cal(struct computer *c)
{
	struct calibration_data *d = c->cdata;
	struct snapshot *s = c->actv;
	int i;
	for(i=d->wp-1; i >= 0 &&
		d->events[i] > s->events[s->events_wp];
		i--);
	for(i++; i<d->wp; i++) {
		if(d->events[i] / s->nominal_sr <= s->events[s->events_wp] / s->nominal_sr)
			continue;
		if(++s->events_wp == s->events_count) s->events_wp = 0;
		s->events[s->events_wp] = d->events[i];
		s->events_tictoc[s->events_wp] = 1;
		debug("event at %llu\n",s->events[s->events_wp]);
	}
	s->events_from = get_timestamp(s->is_light);
}

static void compute_events(struct computer *c)
{
	struct snapshot *s = c->actv;
	struct processing_buffers *p = c->actv->pb;
	if(p && !s->is_old) {
		uint64_t last = s->events[s->events_wp];
		int i;
		for(i=0; i<EVENTS_MAX && p->events[i]; i++)
			if(p->events[i] > last + floor(p->period / 4)) {
				if(++s->events_wp == s->events_count) s->events_wp = 0;
				s->events[s->events_wp] = p->events[i];
				s->events_tictoc[s->events_wp] = p->events_tictoc ? p->events_tictoc[i] : 0;
				debug("event at %llu\n",s->events[s->events_wp]);
			}
		if(s->amps_count && p->amp > 0) {
			if(++s->amps_wp == s->amps_count) s->amps_wp = 0;
			s->amps[s->amps_wp] = p->amp;
			s->amps_time[s->amps_wp] = p->timestamp;
		}
		s->events_from = p->timestamp - ceil(p->period);
	} else {
		s->events_from = get_timestamp(s->is_light);
	}
}

void compute_results(struct snapshot *s)
{
	s->sample_rate = s->nominal_sr * (1 + (double) s->cal / (10 * 3600 * 24));
	if(s->pb) {
		s->guessed_bph = s->bph ? s->bph : guess_bph(s->pb->period / s->sample_rate);
		s->rate = (7200/(s->guessed_bph * s->pb->period / s->sample_rate) - 1)*24*3600;
		s->be = fabs(s->pb->be) * 1000 / s->sample_rate;
		s->amp = s->la * s->pb->amp; // 0 = not available
		if(s->amp < 135 || s->amp > 360)
			s->amp = 0;
	} else
		s->guessed_bph = s->bph ? s->bph : DEFAULT_BPH;
}

static void *computing_thread(void *void_computer)
{
	struct computer *c = void_computer;
	for(;;) {
		pthread_mutex_lock(&c->mutex);
			while(!c->recompute)
				pthread_cond_wait(&c->cond, &c->mutex);
			if(c->recompute > 0) c->recompute = 0;
			int calibrate = c->calibrate;
			c->actv->bph = c->bph;
			c->actv->la = c->la;
			void (*callback)(void *) = c->callback;
			void *callback_data = c->callback_data;
		pthread_mutex_unlock(&c->mutex);

		if(c->recompute < 0) {
			if(callback) callback(callback_data);
			break;
		}

		if(calibrate && !c->actv->calibrate) {
			c->cdata->wp = 0;
			c->cdata->state = 0;
			c->actv->cal_state = 0;
			c->actv->cal_percent = 0;
		}
		if(calibrate != c->actv->calibrate) {
			memset(c->actv->events,0,c->actv->events_count*sizeof(uint64_t));
			memset(c->actv->events_tictoc,0,c->actv->events_count*sizeof(unsigned char));
		}
		c->actv->calibrate = calibrate;

		if(c->actv->calibrate) {
			compute_update_cal(c);
			compute_events_cal(c);
		} else {
			compute_update(c);
			compute_events(c);
		}

		pthread_mutex_lock(&c->mutex);
			if(c->curr)
				snapshot_destroy(c->curr);
			if(c->clear_trace) {
				if(!calibrate) {
					memset(c->actv->events,0,c->actv->events_count*sizeof(uint64_t));
					memset(c->actv->events_tictoc,0,c->actv->events_count*sizeof(unsigned char));
					memset(c->actv->amps,0,c->actv->amps_count*sizeof(*c->actv->amps));
					memset(c->actv->amps_time,0,c->actv->amps_count*sizeof(*c->actv->amps_time));
				}
				c->clear_trace = 0;
			}
			c->curr = snapshot_clone(c->actv);
		pthread_mutex_unlock(&c->mutex);

		if(callback) callback(callback_data);
	}

	debug("Terminating computation thread\n");

	return NULL;
}

void computer_destroy(struct computer *c)
{
	int i;
	for(i=0; i<NSTEPS; i++)
		pb_destroy(&c->pdata->buffers[i]);
	free(c->pdata->buffers);
	free(c->pdata);
	cal_data_destroy(c->cdata);
	free(c->cdata);
	snapshot_destroy(c->actv);
	if(c->curr)
		snapshot_destroy(c->curr);
	pthread_mutex_destroy(&c->mutex);
	pthread_cond_destroy(&c->cond);
	pthread_join(c->thread, NULL);
	free(c);
}

struct computer *start_computer(int nominal_sr, int bph, double la, int cal, int light)
{
	struct processing_buffers *p = NULL;
	struct processing_data *pd = NULL;
	struct calibration_data *cd = NULL;
	struct snapshot *s = NULL;
	struct computer *c = NULL;
	int initialized_buffers = 0;
	int mutex_initialized = 0;
	int cond_initialized = 0;

	if(light) nominal_sr /= 2;
	set_audio_light(light);

	p = malloc(NSTEPS * sizeof(struct processing_buffers));
	if(!p) goto error;

	int first_step = light ? FIRST_STEP_LIGHT : FIRST_STEP;
	int i;
	for(i=0; i<NSTEPS; i++) {
		p[i].sample_rate = nominal_sr;
		p[i].sample_count = nominal_sr * (1<<(i+first_step));
		setup_buffers(&p[i]);
		initialized_buffers++;
	}

	pd = malloc(sizeof(struct processing_data));
	if(!pd) goto error;
	pd->buffers = p;
	pd->last_tic = 0;
	pd->is_light = light;

	cd = malloc(sizeof(struct calibration_data));
	if(!cd) goto error;
	setup_cal_data(cd);

	s = malloc(sizeof(struct snapshot));
	if(!s) goto error;
	s->timestamp = 0;
	s->nominal_sr = nominal_sr;
	s->pb = NULL;
	s->is_old = 1;
	s->calibrate = 0;
	s->signal = 0;
	s->events_count = EVENTS_COUNT;
	s->events = malloc(EVENTS_COUNT * sizeof(uint64_t));
	if(!s->events) goto error;
	memset(s->events,0,EVENTS_COUNT * sizeof(uint64_t));
	s->events_tictoc = malloc(EVENTS_COUNT * sizeof(unsigned char));
	if(!s->events_tictoc) goto error;
	memset(s->events_tictoc,0,EVENTS_COUNT * sizeof(unsigned char));
	s->events_wp = 0;
	s->events_from = 0;
	s->amps_count = EVENTS_COUNT / 2;
	s->amps = malloc(s->amps_count * sizeof(*s->amps));
	if(!s->amps) goto error;
	memset(s->amps, 0, s->amps_count * sizeof(*s->amps));
	s->amps_time = malloc(s->amps_count * sizeof(*s->amps_time));
	if(!s->amps_time) goto error;
	memset(s->amps_time, 0, s->amps_count * sizeof(*s->amps_time));
	s->amps_wp = 0;
	s->trace_centering = 0;
	s->trace_zoom = 1.0;
	s->bph = bph;
	s->la = la;
	s->cal = cal;
	s->is_light = light;

	c = malloc(sizeof(struct computer));
	if(!c) goto error;
	c->cdata = cd;
	c->pdata = pd;
	c->actv = s;
	c->curr = snapshot_clone(s);
	if(!c->curr) goto error;
	c->recompute = 0;
	c->calibrate = 0;
	c->clear_trace = 0;

	if(pthread_mutex_init(&c->mutex, NULL)) goto thread_init_error;
	mutex_initialized = 1;
	if(pthread_cond_init(&c->cond, NULL)) goto thread_init_error;
	cond_initialized = 1;
	if(pthread_create(&c->thread, NULL, computing_thread, c)) goto thread_init_error;

	return c;

thread_init_error:
	error("Unable to initialize computing thread");
error:
	if(cond_initialized)
		pthread_cond_destroy(&c->cond);
	if(mutex_initialized)
		pthread_mutex_destroy(&c->mutex);

	if(c) {
		if(c->curr)
			snapshot_destroy(c->curr);
		free(c);
	}

	if(s) {
		free(s->amps_time);
		free(s->amps);
		free(s->events_tictoc);
		free(s->events);
		free(s);
	}

	if(cd) {
		cal_data_destroy(cd);
		free(cd);
	}

	if(pd)
		free(pd);

	if(p) {
		for(i = 0; i < initialized_buffers; i++)
			pb_destroy(&p[i]);
		free(p);
	}

	return NULL;
}

void lock_computer(struct computer *c)
{
	pthread_mutex_lock(&c->mutex);
}

void unlock_computer(struct computer *c)
{
	if(c->recompute)
		pthread_cond_signal(&c->cond);
	pthread_mutex_unlock(&c->mutex);
}
