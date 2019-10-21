/* Copyright 2011, 2012, 2014 Bert Muennich
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

#include "sxiv.h"
#define _IMAGE_CONFIG
#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

/* XXX where does the code from old void cleanup(void) go? */

bool img_frame_goto(img_t *, int);

void swap_files(int, int);
void shift_file(int, int);
void shift_marked_files(int);
void shift_marked_files_to2(int, int, int, int);
void remove_file(int, bool);
void clone_file(int);
void load_image(int);
bool mark_image(int, bool);
void close_info(void);
void open_info(void);
int ptr_third_x(void);
void redraw(void);
void reset_cursor(void);
void animate(void);
void slideshow(void);
void set_timeout(timeout_f, int, bool);
void reset_timeout(timeout_f);

extern appmode_t mode;
extern img_t img;
extern tns_t tns;
extern win_t win;

extern fileinfo_t *files;
extern int filecnt, fileidx;
extern int alternate;
extern int markcnt;
extern int markidx;

extern int prefix;
extern bool extprefix;

void set_view_current_file(float x, float y, float zoom, float yzoom);

static void write_entry(const char *filename, bool extended, float x, float y, float zoom)
{
	if (extended) {
		printf("%s\tx=%.2f,y=%.2f,zoom=%.2f\n", filename, x, y, zoom * 100.0);
	} else {
		printf("%s\n", filename);
	}
}

bool cg_quit(arg_t _)
{
	unsigned int i;

	if (options->to_stdout && markcnt > 0) {
		for (i = 0; i < filecnt; i++) {
			if (files[i].flags & FF_MARK)
				write_entry(files[i].name, options->extended_filelist, files[i].x, files[i].y, files[i].zoom );
		}
	}
	if (options->all_to_stdout) {
		for (i = 0; i < filecnt; i++)
			write_entry(files[i].name, options->extended_filelist, files[i].x, files[i].y, files[i].zoom);
	}
	exit(EXIT_SUCCESS);
}

bool cg_switch_mode(arg_t _)
{
	if (mode == MODE_IMAGE) {
		if (tns.thumbs == NULL)
			tns_init(&tns, files, &filecnt, &fileidx, &win);
		img_close(&img, false);
		set_view_current_file(img.x, img.y, img.zoom, img.yzoom);
		reset_timeout(reset_cursor);
		if (img.ss.on) {
			img.ss.on = false;
			reset_timeout(slideshow);
		}
		tns.dirty = true;
		mode = MODE_THUMB;
	} else {
//		printf ("T2I: %.2f, %.2f, %.2f\n", img.zoom, img.x, img.y);
//		printf ("    %.2f, %.2f, %.2f\n", files[fileidx].zoom, files[fileidx].x,files[fileidx].y);
		load_image(fileidx);
//		printf ("T2I[2]: %.2f, %.2f, %.2f\n", img.zoom, img.x, img.y);
//		printf ("    %.2f, %.2f, %.2f\n", files[fileidx].zoom, files[fileidx].x,files[fileidx].y);
		mode = MODE_IMAGE;
	}
	return true;
}

bool cg_toggle_fullscreen(arg_t _)
{
	win_toggle_fullscreen(&win);
	/* redraw after next ConfigureNotify event */
	set_timeout(redraw, TO_REDRAW_RESIZE, false);
	if (mode == MODE_IMAGE)
		img.checkpan = img.dirty = true;
	else
		tns.dirty = true;
	return false;
}

bool cg_toggle_bar(arg_t _)
{
	win_toggle_bar(&win);
	if (mode == MODE_IMAGE) {
		if (win.bar.h > 0)
			open_info();
		else
			close_info();
		img.checkpan = img.dirty = true;
	} else {
		tns.dirty = true;
	}
	return true;
}

bool cg_toggle_synczoom(arg_t _)
{
	img_toggle_synczoom(&img);
	return true;
}

bool cg_prefix_external(arg_t _)
{
	extprefix = true;
	return false;
}

bool cg_reload_image(arg_t _)
{
	if (mode == MODE_IMAGE) {
		load_image(fileidx);
	} else {
		win_set_cursor(&win, CURSOR_WATCH);
		if (!tns_load(&tns, fileidx, true, false)) {
			remove_file(fileidx, false);
			tns.dirty = true;
		}
	}
	return true;
}

bool cg_clone_image(arg_t n)
{
	if (prefix != 0)
		n*=prefix;
        while (n>0) {
		clone_file(fileidx);
		n--;
	}
	return true;
}

bool cg_insert_from_clipboard(arg_t _)
{
	/* Stubbed */
	FILE *fp;
	char path[2048];
	char **pathlist;
	char **tmp_pathlist;
	int pathlist_size = 256;
	int new_pathlist_size;
	int npaths = 0;

        fp = popen("xsel -ob", "r");
	if (fp == NULL) {
		fprintf(stderr, "Failed to execute xsel -ob");
		return false;
	}
	
	pathlist = emalloc(sizeof(char *) * pathlist_size);

	while (fgets(path, sizeof(path), fp) != NULL)
	{
		if (strlen(path) == 0)
			continue;
		npaths += 1;
		if (npaths > pathlist_size) {
			new_pathlist_size = MIN(pathlist_size * 2, 65536);
			if (new_pathlist_size == pathlist_size) {
				fprintf(stderr, "Bailout after hitting 65536 filelist entries");
				free(pathlist);
				return false;	
			}
			tmp_pathlist = emalloc(sizeof(char *) * new_pathlist_size);
			memcpy(tmp_pathlist, pathlist, sizeof(char *) * (npaths - 1));
			free(pathlist);
			pathlist = tmp_pathlist;
		}
		/* XXX check path exists and is not a directory */
		*(pathlist + (npaths - 1)) = strdup(path);
	}

	/* STUBBED until insert_files is actually implemented *?
//	insert_files(pathlist, npaths);
	 note -- this assumes insert_files will reuse the strings rather than allocating copies
*/
	free(pathlist);
	/* Will depend on xsel. Obtain data via "xsel -ob".
           There is probably an XLib way of doing this, but I don't know it yet.

	   1. Get list of files to insert
           2. Insert [file in files if file exists]
           3. Dirty thumbs

	   Should return true unless there are no valid, existing filenames in the list.

	   Absolute filenames are strongly preferable, unless you happen to know the working
           directory of the particular sxiv instance you are trying to paste into.
	*/
	return false;
}

bool cg_remove_image(arg_t _)
{
	remove_file(fileidx, true);
	if (mode == MODE_IMAGE)
		load_image(fileidx);
	else
		tns.dirty = true;
	return true;
}

bool cg_remove_marked(arg_t _)
{
        int i;

	for (i = 0; i < filecnt; i++) {
		if (files[i].flags & FF_MARK) {
                        remove_file(i, true);
			i=0;
			continue;
                }
	}
	/* attempt to patch bug where 1 image remains (the first marked item */
	if (markcnt > 0) {
		cg_remove_marked(_);
	}

	if (mode != MODE_IMAGE)
		tns.dirty = true;
        return true;
}

static bool _reorder_image_to(arg_t a, int where)
{
	long n = (long)a;
	if (n > 0) {
		// insert in slot immediately following alternate location (ALT*9876543210...)
		n = ((where + 1) - fileidx) ;
		shift_file(fileidx, n);
		if ((fileidx < alternate) && (where == alternate)) {
			alternate--;
			where--;
		}
	} else {
		// insert in slot immediately preceding alternate location (...0123456789*ALT)
		n = (where - fileidx);
		shift_file(fileidx, n);
		if ((fileidx > alternate) && (where == alternate)) {
			alternate++;
			where++;
		}
	}
	tns.dirty = true;
	return true;
}

bool cg_reorder_image_to_alternate(arg_t a)
{
	return _reorder_image_to(a, alternate);
}

// XXX pointless
bool cg_reorder_image_to_current(arg_t a)
{
	return _reorder_image_to(a, fileidx);
}

bool cg_reorder_image(arg_t a)
{
	long n = (long)a;
	if (prefix != 0) {
		if (n < 0)
			n = -prefix;
		else
			n = prefix;
	}
	shift_file(fileidx, n);
	tns.dirty = true;
	if (mode == MODE_IMAGE) {
		load_image(fileidx);
	}
	return true;
}

bool cg_reorder_marked_images(arg_t _dir)
{
	long dir = (long)_dir;
	// Moves all marked images as a block to either the start or end of filelist.
	// XXX support moving to other locations.
	if (markcnt == 0)
	{
		files[fileidx].flags = files[fileidx].flags | FF_MARK;
		markcnt++;
		shift_marked_files(dir);
		if (dir == 1)
			files[filecnt - 1].flags = files[filecnt - 1].flags ^ FF_MARK;
		else
			files[0].flags = files[0].flags ^ FF_MARK;
		markcnt--;
	} else {
		shift_marked_files(dir);
	}
	tns.dirty = true;

	if (mode == MODE_IMAGE) {
		load_image(fileidx);
	}
	return true;
}


static bool _reorder_marked_images_to(arg_t a, int where)
{
	long n = (long)a;
	long final_fileidx = fileidx;
	// try to preserve position of current file,
	// but leave minimum space for insertion of marked files
	if (n > 0) {
		final_fileidx = MIN(fileidx, filecnt-(markcnt+1));
	} else if (n < 0) {
		final_fileidx = MAX(fileidx, markcnt);
	}
// int index, int direction, int fix_srcindex, int fix_destindex
	if (where == fileidx)
		where = final_fileidx;
	shift_marked_files_to2(where, n, fileidx, final_fileidx);
	tns.dirty = true;
/*
	long n = (long)a;
	int i;
	int saved_fileidx;

	if (markcnt == 0) {
		if (where == alternate)
			cg_reorder_image_to_alternate(a);
		else
			cg_reorder_image_to_current(a);
		return true;
	}

	// XXX this is a really bizarre hack
	// we save the old fileidx, then iterate over the marked files, in reverse, setting fileidx to the index of the marked file and calling cg_reorder_image_to_alternate.
	saved_fileidx = fileidx;
	// note:
	// correctness:
	//  n < 0:
	//    i > alt  OK
	//    i < alt  Rotates marked items inplace
	//  n > 0:
	//    i > alt  Items are placed correctly but in the reverse order.
	//    i < alt  only the 1st item is moved
	//
	for (i = 0; i < filecnt; i++) {
		if (files[i].flags & FF_MARK) {
			fileidx=i;
			if (where == alternate)
				cg_reorder_image_to_alternate(a);
			else
				cg_reorder_image_to_current(a);
		}
	}
	// still, it makes for a good clear factorization.
	fileidx = saved_fileidx;
*/
	return true;
}

bool cg_reorder_marked_images_to_alternate(arg_t a)
{
	_reorder_marked_images_to(a, alternate);
	return (markcnt > 0 ? true : false);
}

bool cg_reorder_marked_images_to_current(arg_t a)
{
	_reorder_marked_images_to(a, fileidx);
	return (markcnt > 0 ? true : false);
}

bool cg_first(arg_t _)
{
	if (mode == MODE_IMAGE && fileidx != 0) {
		load_image(0);
		return true;
	} else if (mode == MODE_THUMB && fileidx != 0) {
		fileidx = 0;
		tns.dirty = true;
		return true;
	} else {
		return false;
	}
}

bool cg_n_or_last(arg_t _)
{
	int n = prefix != 0 && prefix - 1 < filecnt ? prefix - 1 : filecnt - 1;

	if (mode == MODE_IMAGE && fileidx != n) {
		load_image(n);
		return true;
	} else if (mode == MODE_THUMB && fileidx != n) {
		fileidx = n;
		tns.dirty = true;
		return true;
	} else {
		return false;
	}
}

bool cg_scroll_screen(arg_t dir)
{
	bool tmp;
	if (mode == MODE_IMAGE) {
		/* XXX need a way to expose RANDOM(1/PAN_FRAC | npixels) */
		tmp = img_pan(&img, dir, -1);
		if (tmp)
			set_view_current_file(img.x, img.y, img.zoom, img.yzoom);
	} else
		tmp= tns_scroll(&tns, dir, true);
	return tmp;
}

bool cg_zoom(arg_t d)
{
	bool tmp;
	if (mode == MODE_THUMB) {
		return tns_zoom(&tns, d);
	} else {
		if (d > 0)
			tmp = img_zoom_in(&img, d);
		else if (d < 0)
			tmp= img_zoom_out(&img, 0 - d);
		set_view_current_file(img.x, img.y, img.zoom, img.yzoom);
		return tmp;
	}

	return false;
}

bool cg_toggle_image_mark(arg_t _)
{
	return mark_image(fileidx, !(files[fileidx].flags & FF_MARK));
}

bool cg_reverse_marks(arg_t _)
{
	int i;

	for (i = 0; i < filecnt; i++) {
		files[i].flags ^= FF_MARK;
		markcnt += files[i].flags & FF_MARK ? 1 : -1;
	}
	if (mode == MODE_THUMB)
		tns.dirty = true;
	return true;
}

bool cg_mark_range(arg_t _)
{
	int d = markidx < fileidx ? 1 : -1, end, i;
	bool dirty = false, on = !!(files[markidx].flags & FF_MARK);

	for (i = markidx + d, end = fileidx + d; i != end; i += d)
		dirty |= mark_image(i, on);
	return dirty;
}

bool cg_unmark_all(arg_t _)
{
	int i;

	for (i = 0; i < filecnt; i++)
		files[i].flags &= ~FF_MARK;
	markcnt = 0;
	if (mode == MODE_THUMB)
		tns.dirty = true;
	return true;
}

bool cg_navigate_marked(arg_t n)
{
	int d, i;
	int new = fileidx;

	if (prefix > 0)
		n *= prefix;
	d = n > 0 ? 1 : -1;
	for (i = fileidx + d; n != 0 && i >= 0 && i < filecnt; i += d) {
		if (files[i].flags & FF_MARK) {
			n -= d;
			new = i;
		}
	}
	if (new != fileidx) {
		if (mode == MODE_IMAGE) {
			load_image(new);
		} else {
			fileidx = new;
			tns.dirty = true;
		}
		return true;
	} else {
		return false;
	}
}

bool cg_change_gamma(arg_t d)
{
	if (img_change_gamma(&img, d * (prefix > 0 ? prefix : 1))) {
		if (mode == MODE_THUMB)
			tns.dirty = true;
		return true;
	} else {
		return false;
	}
}

bool ci_navigate(arg_t n)
{
	if (prefix > 0)
		n *= prefix;
	n += fileidx;
	if (n < 0)
		n = 0;
	if (n >= filecnt)
		n = filecnt - 1;

	if (n != fileidx) {
		load_image(n);
		return true;
	} else {
		return false;
	}
}

bool ci_cursor_navigate(arg_t _)
{
	return ci_navigate(ptr_third_x() - 1);
}

bool ci_alternate(arg_t _)
{
	load_image(alternate);
	return true;
}

bool ci_navigate_frame(arg_t d)
{
	int frame;
	if (prefix > 0)
		d *= prefix;

	if (img.multi.cnt > 0) {
		frame = (img.multi.sel + d) % img.multi.cnt;
		while (frame < 0)
			frame += img.multi.cnt;
		return img_frame_goto(&img,frame);
	} else {
		return img_frame_navigate(&img, d);
	}
}

bool ci_toggle_animation(arg_t _)
{
	bool dirty = false;

	if (img.multi.cnt > 0) {
		img.multi.animate = !img.multi.animate;
		if (img.multi.animate) {
			dirty = img_frame_animate(&img);
			set_timeout(animate, img.multi.frames[img.multi.sel].delay, true);
		} else {
			reset_timeout(animate);
		}
	}
	return dirty;
}

bool ci_toggle_mouse_pos(arg_t a)
{
	if (img.show_mouse_pos == 0) {
		img.show_mouse_pos = 1;
	} else if (img.show_mouse_pos == 1) {
		img.show_mouse_pos = 2;
	} else {
		img.show_mouse_pos = 0;
	}
	img.dirty = true;
	return true;
}

bool ci_print_mouse_pos(arg_t a)
{
	if (img.show_mouse_pos == 1){
		error(0, 0, "@: %d|%d", img.win->mouse.x, img.win->mouse.y);
	} else {
		error(0, 0, "@: %%%.3f|%.3f", \
                                        ((float)img.win->mouse.x * 100) / ((float)img.w), \
                                        ((float)img.win->mouse.y * 100) / ((float)img.h));
        }
        return true;
}

bool ci_cycle_tiling(arg_t _)
{
	img_cycle_tiling(&img);
	return true;
}


bool ci_scroll(arg_t dir)
{
	return img_pan(&img, dir, prefix);
}

bool ci_scroll_to_edge(arg_t dir)
{
	return img_pan_edge(&img, dir);
}

bool ci_drag(arg_t mode)
{
	int x, y, ox, oy;
	float px, py;
	XEvent e;

	if ((int)(img.w * img.zoom) <= win.w && (int)(img.h * img.zoom) <= win.h)
		return false;
	
	win_set_cursor(&win, CURSOR_DRAG);

	win_cursor_pos(&win, &x, &y);
	ox = x;
	oy = y;

	for (;;) {
		if (mode == DRAG_ABSOLUTE) {
			px = MIN(MAX(0.0, x - win.w*0.1), win.w*0.8) / (win.w*0.8)
			   * (win.w - img.w * (img.zoom * img.wmul));
			py = MIN(MAX(0.0, y - win.h*0.1), win.h*0.8) / (win.h*0.8)
			   * (win.h - img.h * (img.zoom * img.hmul));
		} else {
			px = img.x + x - ox;
			py = img.y + y - oy;
		}

		if (img_pos(&img, px, py)) {
			img_render(&img);
			win_draw(&win);
		}
		XMaskEvent(win.env.dpy,
		           ButtonPressMask | ButtonReleaseMask | PointerMotionMask, &e);
		if (e.type == ButtonPress || e.type == ButtonRelease)
			break;
		while (XCheckTypedEvent(win.env.dpy, MotionNotify, &e));
		ox = x;
		oy = y;
		x = e.xmotion.x;
		y = e.xmotion.y;
	}
	set_timeout(reset_cursor, TO_CURSOR_HIDE, true);
	set_view_current_file(img.x, img.y, img.zoom, img.yzoom);
	return true;
}

bool ci_set_zoom(arg_t zl)
{
	bool tmp = img_zoom(&img, (prefix ? prefix : zl) / 100.0);
	if (tmp)
		set_view_current_file(img.x, img.y, img.zoom, img.yzoom);
	return tmp;
}

bool ci_fit_to_win(arg_t sm)
{
	bool tmp = img_fit_win(&img, sm);
	if (tmp)
		set_view_current_file(img.x, img.y, img.zoom, img.yzoom);
	return tmp;
}

bool ci_rotate(arg_t degree)
{
	img_rotate(&img, degree);
	return true;
}

bool ci_flip(arg_t dir)
{
	img_flip(&img, dir);
	return true;
}

bool ci_cycle_antialias(arg_t _)
{
	img_cycle_antialias(&img);
	return true;
}

bool ci_cycle_scalefactor(arg_t s)
{
	if (prefix != 0)
		s = prefix;
	//warn("cycle_sf arg = %d", s);
	if (s > 99)
		return false;
	if ((s != 0) && ((s % 10) == 0))
		return false;
	if (s==0) {
		img_cycle_scalefactors(&img);
	} else {
		// input '1' to reset scales, for example.
		if (s < 10) {
			img.wmul = s;
			img.hmul = 1;
		} else {
			int fac;
			img.wmul = s / 10;
			img.hmul = s % 10;
			if (img.wmul == img.hmul) {
				img.wmul = 1;
				img.hmul = 1;
			} else {
				for (fac=2; fac < 10; fac++) {
					if (((img.wmul % fac) == 0) && ((img.hmul % fac) == 0)) {
						img.wmul = img.wmul / fac;
						img.hmul = img.hmul / fac;
						break;
					}
				}
			}
			//warn("final w/hmul = %dx%d", img.wmul, img.hmul);
		}
		img.dirty = true;
		img.checkpan = true;
	}
	set_view_current_file(img.x, img.y, img.zoom, img.yzoom);
	return true;
}


bool cg_cycle_silhouetting(arg_t _)
{
	img_cycle_silhouetting(&img);
	if (mode == MODE_THUMB)
		tns.dirty = true;
	return true;
}

bool cg_cycle_opacity(arg_t o)
{
	// 0 : cycle forward
	// -1 : cycle backward
	// 1-6 : set opacity to o
	if (prefix != 0)
		o = prefix;
	if (o > 6)
		return false;
	if (o == -1) {
		//warn("o-1");
		img_cycle_opacity(&img, -1);
	} else {
		//warn("o>=0 : %d", o);
		if (o == 0) {
			img_cycle_opacity(&img, 1);
		} else {
			img.opacity=0;
			img_cycle_opacity(&img, o);
		}
	}
	if (mode == MODE_THUMB)
		tns.dirty = true;
	tns.need_alpha = (img.opacity !=6);
	return true;
}

bool cg_toggle_negalpha(arg_t _)
{
	img_toggle_negalpha(&img);
	if (mode == MODE_THUMB)
		tns.dirty = true;
	return true;
}


bool ci_toggle_alpha(arg_t _)
{
	img.alpha = !img.alpha;
	img.dirty = true;
	img.tile.dirty_cache = true;
	return true;
}

bool ci_slideshow(arg_t _)
{
	if (prefix > 0) {
		img.ss.on = true;
		img.ss.delay = prefix * 100; // specified in tenths of seconds
		set_timeout(slideshow, img.ss.delay, true);
	} else if (img.ss.on) {
		img.ss.on = false;
		reset_timeout(slideshow);
	} else {
		img.ss.on = true;
	}

	return true;
}

bool ct_move_sel(arg_t dir)
{
	return (tns_move_selection(&tns, dir, prefix) != 0);
}

bool ct_move_and_reorder(arg_t dir)
{
	int tn_offset;
	int old_index;
	old_index = fileidx;
	tn_offset = tns_move_selection(&tns, dir, prefix);
	if (tn_offset != 0) {
		swap_files(old_index, tn_offset);
		tns.dirty = true;
		if (mode == MODE_IMAGE) {
			load_image(fileidx);
		}
		return true;
	}
        return false;
}

bool ct_reload_all(arg_t _)
{
	tns_free(&tns);
	tns_init(&tns, files, &filecnt, &fileidx, &win);
	tns.dirty = true;
	return true;
}


#undef  G_CMD
#define G_CMD(c) { -1, cg_##c },
#undef  I_CMD
#define I_CMD(c) { MODE_IMAGE, ci_##c },
#undef  T_CMD
#define T_CMD(c) { MODE_THUMB, ct_##c },

const cmd_t cmds[CMD_COUNT] = {
#include "commands.lst"
};

