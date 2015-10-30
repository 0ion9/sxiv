/* Copyright 2011 Bert Muennich
 *
 * This file is part of sxiv.
 *
 * sxiv is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * sxiv is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with sxiv.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef IMAGE_H
#define IMAGE_H

#include <Imlib2.h>

#include "types.h"
#include "window.h"

typedef struct {
	Imlib_Image im;
	unsigned int delay;
} img_frame_t;

typedef struct {
	img_frame_t *frames;
	int cap;
	int cnt;
	int sel;
	bool animate;
	int length;
} multi_img_t;

typedef struct {
	Imlib_Image im;
	int w;
	int h;
	int wmul;
	int hmul;

	win_t *win;
	float x;
	float y;

	scalemode_t scalemode;
	float zoom;
	float yzoom; // always equals zoom except in distort mode, where fit is done on width and then yzoom is set to fill the window vertically.

	bool checkpan;
	bool dirty;
	int aa;
	bool alpha;
	int show_mouse_pos;

	Imlib_Color_Modifier cmod;
	int gamma;
	// XXX allow prefix-input for setting silhouetting vals, same idea as gamma.
	int silhouetting;
	int opacity;
	bool negate_alpha;
	struct {
		int mode;
		int layout_w;
		int layout_h;
		int display_max_w;
		int display_max_h;
		int tilesx;
		int tilesy;
		unsigned char layout[12][12];
		bool dirty_cache;
		// XXX dynamically allocate this so we can cope with animated images
		Imlib_Image cache[6];
	} tile;
	struct {
		bool on;
		int delay;
	} ss;

	multi_img_t multi;
} img_t;

void img_init(img_t*, win_t*);

bool img_load(img_t*, const fileinfo_t*);
void img_close(img_t*, bool);

void img_render(img_t*);

bool img_fit_win(img_t*, scalemode_t);

bool img_zoom(img_t*, float);
bool img_zoom_in(img_t*);
bool img_zoom_out(img_t*);

bool img_move(img_t*, float, float);
bool img_pan(img_t*, direction_t, int);
bool img_pan_edge(img_t*, direction_t);

void img_rotate(img_t*, degree_t);
void img_flip(img_t*, flipdir_t);

bool img_need_trans(img_t*);

void img_cycle_antialias(img_t*);
void img_cycle_silhouetting(img_t*);
void img_toggle_negalpha(img_t*);
bool img_cycle_opacity(img_t*, int);
void img_cycle_tiling(img_t*);
void img_cycle_scalefactors(img_t*);
bool img_change_gamma(img_t*, int);

bool img_frame_navigate(img_t*, int);
bool img_frame_animate(img_t*);

#endif /* IMAGE_H */
