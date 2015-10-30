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

#define _POSIX_C_SOURCE 200112L
#define _THUMBS_CONFIG

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>

#include "thumbs.h"
#include "util.h"
#include "config.h"

#if HAVE_LIBEXIF
#include <libexif/exif-data.h>
void exif_auto_orientate(const fileinfo_t*);
#endif

#define INTEGER_FIT(w,h,dim)  ((int)(MAX(1, dim / MAX(w, h))))

static char *cache_dir;

char* tns_cache_filepath(const char *filepath)
{
	size_t len;
	char *cfile = NULL;

	if (cache_dir == NULL || filepath == NULL || *filepath != '/')
		return NULL;

	if (strncmp(filepath, cache_dir, strlen(cache_dir)) != 0) {
		/* don't cache images inside the cache directory! */
		len = strlen(cache_dir) + strlen(filepath) + 2;
		cfile = (char*) s_malloc(len);
		snprintf(cfile, len, "%s/%s", cache_dir, filepath + 1);
	}
	return cfile;
}

Imlib_Image tns_cache_load(const char *filepath, bool *outdated)
{
	char *cfile;
	struct stat cstats, fstats;
	Imlib_Image im = NULL;

	if (filepath == NULL)
		return NULL;
	if (stat(filepath, &fstats) < 0)
		return NULL;

	if ((cfile = tns_cache_filepath(filepath)) != NULL) {
		if (stat(cfile, &cstats) == 0) {
			if (cstats.st_mtime == fstats.st_mtime)
				im = imlib_load_image(cfile);
			else
				*outdated = true;
		}
		free(cfile);
	}
	return im;
}

void tns_cache_write(Imlib_Image im, const char *filepath, bool force)
{
	char *cfile, *dirend;
	struct stat cstats, fstats;
	struct utimbuf times;
	Imlib_Load_Error err = 0;

	if (im == NULL || filepath == NULL)
		return;
	if (stat(filepath, &fstats) < 0)
		return;

	if ((cfile = tns_cache_filepath(filepath)) != NULL) {
		if (force || stat(cfile, &cstats) < 0 ||
		    cstats.st_mtime != fstats.st_mtime)
		{
			if ((dirend = strrchr(cfile, '/')) != NULL) {
				*dirend = '\0';
				err = r_mkdir(cfile);
				*dirend = '/';
			}
			if (err == 0) {
				imlib_context_set_image(im);
				if (imlib_image_has_alpha()) {
					imlib_image_set_format("png");
				} else {
					imlib_image_set_format("jpg");
					imlib_image_attach_data_value("quality", NULL, 90, NULL);
				}
				imlib_save_image_with_error_return(cfile, &err);
			}
			if (err == 0) {
				times.actime = fstats.st_atime;
				times.modtime = fstats.st_mtime;
				utime(cfile, &times);
			}
		}
		free(cfile);
	}
}

void tns_clean_cache(tns_t *tns)
{
	int dirlen;
	bool delete;
	char *cfile, *filename, *tpos;
	r_dir_t dir;

	if (cache_dir == NULL)
		return;

	if (r_opendir(&dir, cache_dir) < 0) {
		warn("could not open thumbnail cache directory: %s", cache_dir);
		return;
	}

	dirlen = strlen(cache_dir);

	while ((cfile = r_readdir(&dir)) != NULL) {
		filename = cfile + dirlen;
		delete = false;

		if ((tpos = strrchr(filename, '.')) != NULL) {
			*tpos = '\0';
			if (access(filename, F_OK) < 0)
				delete = true;
			*tpos = '.';
		}
		if (delete) {
			if (unlink(cfile) < 0)
				warn("could not delete cache file: %s", cfile);
		}
		free(cfile);
	}
	r_closedir(&dir);
}


void tns_init(tns_t *tns, fileinfo_t *files, const int *cnt, int *sel,
              win_t *win)
{
	int len;
	const char *homedir, *dsuffix = "";

	if (tns == NULL)
		return;

	if (cnt != NULL && *cnt > 0) {
		tns->thumbs = (thumb_t*) s_malloc(*cnt * sizeof(thumb_t));
		memset(tns->thumbs, 0, *cnt * sizeof(thumb_t));
	} else {
		tns->thumbs = NULL;
	}
	tns->files = files;
	tns->cnt = cnt;
	tns->initnext = tns->loadnext = 0;
	tns->first = tns->end = tns->r_first = tns->r_end = 0;
	tns->sel = sel;
	tns->win = win;
	tns->dirty = false;

	tns->zl = MIN(default_thumbsize_index, sizeof(thumb_sizes) / sizeof(int));
	tns->zl = MAX(0, tns->zl);
	tns->zmultl = 0;
	tns->need_alpha = false;
	tns_zoom(tns, 1);

	if ((homedir = getenv("XDG_CACHE_HOME")) == NULL || homedir[0] == '\0') {
		homedir = getenv("HOME");
		dsuffix = "/.cache";
	}
	if (homedir != NULL) {
		free(cache_dir);
		len = strlen(homedir) + strlen(dsuffix) + 6;
		cache_dir = (char*) s_malloc(len);
		snprintf(cache_dir, len, "%s%s/sxiv", homedir, dsuffix);
	} else {
		warn("could not locate thumbnail cache directory");
	}
}

void tns_free(tns_t *tns)
{
	int i;

	if (tns == NULL)
		return;

	if (tns->thumbs != NULL) {
		for (i = 0; i < *tns->cnt; i++) {
			if (tns->thumbs[i].im != NULL) {
				imlib_context_set_image(tns->thumbs[i].im);
				imlib_free_image();
			}
		}
		free(tns->thumbs);
		tns->thumbs = NULL;
	}

	free(cache_dir);
	cache_dir = NULL;
}

Imlib_Image tns_scale_down(Imlib_Image im, int dim)
{
	int w, h;
	float z, zw, zh;

	imlib_context_set_image(im);
	w = imlib_image_get_width();
	h = imlib_image_get_height();
	zw = (float) dim / (float) w;
	zh = (float) dim / (float) h;
	z = MIN(zw, zh);
	z = MIN(z, 1.0);

	if (z < 1.0) {
		imlib_context_set_anti_alias(1);
		im = imlib_create_cropped_scaled_image(0, 0, w, h,
		                                       MAX(z * w, 1), MAX(z * h, 1));
		if (im == NULL)
			die("could not allocate memory");
		imlib_free_image_and_decache();
	}
	return im;
}

bool tns_load(tns_t *tns, int n, bool force, bool cache_only)
{
	int w, h;
	int maxwh = thumb_sizes[ARRLEN(thumb_sizes)-1];
	bool cache_hit = false;
	char *cfile;
	float zw, zh;
	thumb_t *t;
	fileinfo_t *file;
	Imlib_Image im = NULL;

	if (tns == NULL || tns->thumbs == NULL)
		return false;
	if (n < 0 || n >= *tns->cnt)
		return false;
	file = &tns->files[n];
	if (file->name == NULL || file->path == NULL)
		return false;

	t = &tns->thumbs[n];

	if (t->im != NULL) {
		imlib_context_set_image(t->im);
		imlib_free_image();
		t->im = NULL;
	}

	if (!force) {
		if ((im = tns_cache_load(file->path, &force)) != NULL) {
			imlib_context_set_image(im);
			if (imlib_image_get_width() < maxwh &&
			    imlib_image_get_height() < maxwh)
			{
				if ((cfile = tns_cache_filepath(file->path)) != NULL) {
					unlink(cfile);
					free(cfile);
				}
				imlib_free_image_and_decache();
				im = NULL;
			} else {
				cache_hit = true;
			}
#if HAVE_LIBEXIF
		} else if (!force) {
			int pw = 0, ph = 0, x = 0, y = 0;
			bool err;
			ExifData *ed;
			ExifEntry *entry;
			ExifContent *ifd;
			ExifByteOrder byte_order;
			int tmpfd;
			char tmppath[] = "/tmp/sxiv-XXXXXX";
			Imlib_Image tmpim;

			if ((ed = exif_data_new_from_file(file->path)) != NULL) {
				if (ed->data != NULL && ed->size > 0 &&
				    (tmpfd = mkstemp(tmppath)) >= 0)
				{
					err = write(tmpfd, ed->data, ed->size) != ed->size;
					close(tmpfd);

					if (!err && (tmpim = imlib_load_image(tmppath)) != NULL) {
						byte_order = exif_data_get_byte_order(ed);
						ifd = ed->ifd[EXIF_IFD_EXIF];
						entry = exif_content_get_entry(ifd, EXIF_TAG_PIXEL_X_DIMENSION);
						if (entry != NULL)
							pw = exif_get_long(entry->data, byte_order);
						entry = exif_content_get_entry(ifd, EXIF_TAG_PIXEL_Y_DIMENSION);
						if (entry != NULL)
							ph = exif_get_long(entry->data, byte_order);

						imlib_context_set_image(tmpim);
						w = imlib_image_get_width();
						h = imlib_image_get_height();

						if (pw > w && ph > h && (pw - ph >= 0) == (w - h >= 0)) {
							zw = (float) pw / (float) w;
							zh = (float) ph / (float) h;
							if (zw < zh) {
								pw /= zh;
								x = (w - pw) / 2;
								w = pw;
							} else if (zw > zh) {
								ph /= zw;
								y = (h - ph) / 2;
								h = ph;
							}
						}
						if (w >= maxwh || h >= maxwh) {
							if ((im = imlib_create_cropped_image(x, y, w, h)) == NULL)
								die("could not allocate memory");
						}
						imlib_free_image_and_decache();
					}
					unlink(tmppath);
				}
				exif_data_unref(ed);
			}
#endif
		}
	}

	if (im == NULL && (access(file->path, R_OK) < 0 ||
	    (im = imlib_load_image(file->path)) == NULL))
	{
		if (file->flags & FF_WARN)
			warn("could not open image: %s", file->name);
		return false;
	}
	imlib_context_set_image(im);
	if (!cache_hit) {
#if HAVE_LIBEXIF
		exif_auto_orientate(file);
#endif
		im = tns_scale_down(im, maxwh);
		imlib_context_set_image(im);
		if (imlib_image_get_width() == maxwh || imlib_image_get_height() == maxwh)
			tns_cache_write(im, file->path, true);
	}

	if (cache_only) {
		imlib_free_image_and_decache();
	} else {
		t->im = tns_scale_down(im, thumb_sizes[tns->zl]);
		imlib_context_set_image(t->im);
		t->w = imlib_image_get_width();
		t->h = imlib_image_get_height();
		tns->dirty = true;
	}
	file->flags |= FF_TN_INIT;

	if (n == tns->initnext)
		while (++tns->initnext < *tns->cnt && ((++file)->flags & FF_TN_INIT));
	if (n == tns->loadnext && !cache_only)
		while (++tns->loadnext < tns->end && (++t)->im != NULL);

	return true;
}

void tns_unload(tns_t *tns, int n)
{
	thumb_t *t;

	if (tns == NULL || tns->thumbs == NULL)
		return;
	if (n < 0 || n >= *tns->cnt)
		return;

	t = &tns->thumbs[n];

	if (t->im != NULL) {
		imlib_context_set_image(t->im);
		imlib_free_image();
		t->im = NULL;
	}
}

int tns_max_scale(tns_t *tns)
{
	thumb_t *t;
	float avgw, avgh;
	int maxw, maxh;
	int nvisiblethumbs;
	int maxscale;
	int i;

	avgw = 8;
	avgh = 8;
	maxw = 8;
	maxh = 8;
	nvisiblethumbs = 0;
	for (i = tns->first; i < tns->end; i++) {
		t = &tns->thumbs[i];
		if (t->im != NULL) {
			nvisiblethumbs++;
			avgw += t->w;
			avgh += t->h;
			maxw = MAX(t->w, maxw);
			maxh = MAX(t->h, maxh);
		}
	}

	if (nvisiblethumbs > 0) {
		avgw /= nvisiblethumbs;
		avgh /= nvisiblethumbs;
	}

	//	warn("avgw = %d, avgh = %d, imul = %d", avgw, avgh, global_imul);
	maxscale = MAX(1, INTEGER_FIT(avgw, avgh, thumb_sizes[tns->zl]) + tns->autozoom_threshold);
	if (tns->win && ((tns->dim + tns->bw) >= MIN(tns->win->w, tns->win->h)))
		maxscale=1;
	return maxscale;
}


void tns_check_view(tns_t *tns, bool scrolled)
{
	int r;

	if (tns == NULL)
		return;

	tns->first -= tns->first % tns->cols;
	r = *tns->sel % tns->cols;

	if (scrolled) {
		/* move selection into visible area */
		if (*tns->sel >= tns->first + tns->cols * tns->rows)
			*tns->sel = tns->first + r + tns->cols * (tns->rows - 1);
		else if (*tns->sel < tns->first)
			*tns->sel = tns->first + r;
	} else {
		/* scroll to selection */
		if (tns->first + tns->cols * tns->rows <= *tns->sel) {
			tns->first = *tns->sel - r - tns->cols * (tns->rows - 1);
			tns->dirty = true;
		} else if (tns->first > *tns->sel) {
			tns->first = *tns->sel - r;
			tns->dirty = true;
		}
	}
	tns->max_scale = tns_max_scale(tns);
}



void tns_render(tns_t *tns)
{
	thumb_t *t;
	win_t *win;
	int i, cnt, r, x, y;
	int cols, rows;
	char oldaa;
	int zoom;
	int fitmul;

	if (tns == NULL || tns->thumbs == NULL || tns->win == NULL)
		return;
	if (!tns->dirty)
		return;

	win = tns->win;
	win_clear(win);
	imlib_context_set_drawable(win->buf.pm);

	zoom = thumbnail_zoom_levels[tns->zmultl];
	cols = win->w / tns->dim;
	rows = win->h / tns->dim;
	if ((cols < 0) || (rows == 0)) {
		tns->cols = 1;
		tns->rows = 1;
		return;
	} else {
		tns->cols = cols;
		tns->rows = rows;
	}

	if (*tns->cnt < tns->cols * tns->rows) {
		tns->first = 0;
		cnt = *tns->cnt;
	} else {
		tns_check_view(tns, false);
		cnt = tns->cols * tns->rows;
		if ((r = tns->first + cnt - *tns->cnt) >= tns->cols)
			tns->first -= r - r % tns->cols;
		if (r > 0)
			cnt -= r % tns->cols;
	}
	r = cnt % tns->cols ? 1 : 0;
	tns->x = x = (win->w - MIN(cnt, tns->cols) * tns->dim) / 2 + tns->bw + 3;
	tns->y = y = (win->h - (cnt / tns->cols + r) * tns->dim) / 2 + tns->bw + 3;
	if (tns->x < 0) {
		warn("negtnx %d; ww,cnt,cols,dim,bw=%d %d %d %d %d %d", x, win->w, cnt, tns->cols, tns->dim, tns->bw);
		x = MAX(1, (tns->bw + 1) / 2 + 1);
		tns->x = x;
	}
	if (tns->y < 0) {
		warn("negtny %d; wh,cnt,cols,dim,bw=%d %d %d %d %d %d", y, win->h, cnt, tns->cols, tns->dim, tns->bw);
		y = MAX(1, (tns->bw + 1) / 2 + 1);
		tns->y = y;
	}
	if (tns->x > 2147483000)
		warn("postnx %d; ww,cnt,cols,dim,bw=%d %d %d %d %d %d", x, win->w, cnt, tns->cols, tns->dim, tns->bw);
//	warn("tnx, tny = %d, %d", tns->x, tns->y);
	tns->loadnext = *tns->cnt;
	tns->end = tns->first + cnt;

	for (i = tns->r_first; i < tns->r_end; i++) {
		if ((i < tns->first || i >= tns->end) && tns->thumbs[i].im != NULL)
			tns_unload(tns, i);
	}
	tns->r_first = tns->first;
	tns->r_end = tns->end;

	oldaa = imlib_context_get_anti_alias();
	for (i = tns->first; i < tns->end; i++) {
		t = &tns->thumbs[i];
		if (t->im != NULL) {
			fitmul = INTEGER_FIT(t->w, t->h, thumb_sizes[tns->zl]);
			fitmul = MIN(fitmul, tns->max_scale);
			t->x = ((thumb_sizes[tns->zl] * zoom ) / 100);
			t->x -= ((t->w * zoom * fitmul) / 100);
			t->x = t->x / 2 + x;
			if (t->x < 0) {
				warn("negtx %d; x=%d", t->x, x);
				t->x = 1;
			}
			t->y = y + (((thumb_sizes[tns->zl] * zoom ) / 100) \
			         - ((t->h * zoom * fitmul) / 100)) / 2;
			if (t->y < 0) {
				warn("negtx %d; x=%d", t->x, x);
				t->y = 1;
			}
			imlib_context_set_image(t->im);
			if (tns->need_alpha && !imlib_image_has_alpha()) {
				imlib_context_set_image(imlib_clone_image());
				imlib_image_set_has_alpha(1);
			}
			if ((zoom * fitmul) <= THUMBNAIL_PIXELIZE_AT)
				imlib_context_set_anti_alias(1);
			else
				imlib_context_set_anti_alias(0);
			imlib_render_image_on_drawable_at_size(t->x, t->y,
				(t->w * zoom * fitmul) / 100, (t->h * zoom * fitmul) / 100);
			if (tns->files[i].flags & FF_MARK)
				tns_mark(tns, i, true);
			if (t->im != imlib_context_get_image()) {
				imlib_free_image();
				imlib_context_set_image(t->im);
			}
		} else {
			tns->loadnext = MIN(tns->loadnext, i);
		}
		if ((i + 1) % tns->cols == 0) {
			x = tns->x;
			y += tns->dim;
		} else {
			x += tns->dim;
		}
	}
	imlib_context_set_anti_alias(oldaa);
	tns->dirty = false;
	tns_highlight(tns, *tns->sel, true);
}

void tns_mark(tns_t *tns, int n, bool mark)
{
	if (tns == NULL || tns->thumbs == NULL || tns->win == NULL)
		return;

	if (n >= 0 && n < *tns->cnt && tns->thumbs[n].im != NULL) {
		win_t *win = tns->win;
		thumb_t *t = &tns->thumbs[n];
		unsigned long col = win->fullscreen ? win->fscol : win->bgcol;
		int x,y;
		int zoom;
		int fitmul;

		zoom = thumbnail_zoom_levels[tns->zmultl];
		fitmul = INTEGER_FIT(t->w, t->h, thumb_sizes[tns->zl]);
		fitmul = MIN(fitmul, tns->max_scale);

		x = t->x + ((t->w * zoom * fitmul) / 100);
		y = t->y + ((t->h * zoom * fitmul) / 100);

		win_draw_rect(win, x - 1, y + 1, 1, tns->bw, true, 1, col);
		win_draw_rect(win, x + 1, y - 1, tns->bw, 1, true, 1, col);

		if (mark)
			col = win->selcol;

		win_draw_rect(win, x, y, tns->bw + 2, tns->bw + 2, true, 1, col);

		if (!mark && n == *tns->sel)
			tns_highlight(tns, n, true);
	}
}

void tns_highlight(tns_t *tns, int n, bool hl)
{
	if (tns == NULL || tns->thumbs == NULL || tns->win == NULL)
		return;

	if (n >= 0 && n < *tns->cnt && tns->thumbs[n].im != NULL) {
		win_t *win = tns->win;
		thumb_t *t = &tns->thumbs[n];
		unsigned long col;
		int oxy = (tns->bw + 1) / 2 + 1, owh = tns->bw + 2;
		int zoom;
		int fitmul;

		zoom = thumbnail_zoom_levels[tns->zmultl];
		fitmul = INTEGER_FIT(t->w, t->h, thumb_sizes[tns->zl]);
		fitmul = MIN(fitmul, tns->max_scale);

		if (hl)
			col = win->selcol;
		else
			col = win->fullscreen ? win->fscol : win->bgcol;

		win_draw_rect(win, t->x - oxy, t->y - oxy,
		              ((t->w * zoom * fitmul) / 100) + owh, ((t->h * zoom * fitmul) / 100) + owh,
		              false, tns->bw, col);

		if (tns->files[n].flags & FF_MARK)
			tns_mark(tns, n, true);
	}
}

int tns_move_selection(tns_t *tns, direction_t dir, int cnt)
{
	int old, max;

	if (tns == NULL || tns->thumbs == NULL)
		return false;

	old = *tns->sel;
	cnt = cnt > 1 ? cnt : 1;

	switch (dir) {
		case DIR_UP:
			*tns->sel = MAX(*tns->sel - cnt * tns->cols, *tns->sel % tns->cols);
			break;
		case DIR_DOWN:
			max = tns->cols * ((*tns->cnt - 1) / tns->cols) +
			      MIN((*tns->cnt - 1) % tns->cols, *tns->sel % tns->cols);
			*tns->sel = MIN(*tns->sel + cnt * tns->cols, max);
			break;
		case DIR_LEFT:
			*tns->sel = MAX(*tns->sel - cnt, 0);
			break;
		case DIR_RIGHT:
			*tns->sel = MIN(*tns->sel + cnt, *tns->cnt - 1);
			break;
	}

	if (*tns->sel != old) {
		tns_highlight(tns, old, false);
		tns_check_view(tns, false);
		if (!tns->dirty)
			tns_highlight(tns, *tns->sel, true);
	}
	return ((*tns->sel) - old);
}

bool tns_scroll(tns_t *tns, direction_t dir, bool screen)
{
	int d, max, old;

	if (tns == NULL)
		return false;

	old = tns->first;
	d = tns->cols * (screen ? tns->rows : 1);

	if (dir == DIR_DOWN) {
		max = *tns->cnt - tns->cols * tns->rows;
		if (*tns->cnt % tns->cols != 0)
			max += tns->cols - *tns->cnt % tns->cols;
		tns->first = MIN(tns->first + d, max);
	} else if (dir == DIR_UP) {
		tns->first = MAX(tns->first - d, 0);
	}

	if (tns->first != old) {
		tns_check_view(tns, true);
		tns->dirty = true;
	}
	return tns->first != old;
}

bool tns_zoom(tns_t *tns, int d)
{
	int i, oldzl, oldzmultl;

	if (tns == NULL || tns->thumbs == NULL)
		return false;

	oldzl = tns->zl;
	oldzmultl = tns->zmultl;
	if (oldzmultl == 0){
		tns->zl += -(d < 0) + (d > 0);
		tns->zl = MAX(tns->zl, 0);
		tns->zl = MIN(tns->zl, ARRLEN(thumb_sizes)-1);
	}

	tns->bw = ((thumb_sizes[tns->zl] - 1) >> 5) + 1;
	tns->bw = MIN(tns->bw, 4);
	tns->dim = ((thumb_sizes[tns->zl] * thumbnail_zoom_levels[oldzmultl]) / 100) + 2 * tns->bw + 6;
	if (oldzl == (ARRLEN(thumb_sizes) - 1)){
		int mindim = MIN(tns->win->w, tns->win->h);
		int thisdim;
		if (tns->zl == oldzl){
			tns->zmultl += -(d < 0) + (d > 0);
			tns->zmultl = MAX(tns->zmultl, 0);
			tns->zmultl = MIN(tns->zmultl, ARRLEN(thumbnail_zoom_levels)-1);
		} else {
			/* zooming out to a smaller thumb */
			tns->zmultl = 0;
		}

		thisdim = ((thumb_sizes[tns->zl] * thumbnail_zoom_levels[tns->zmultl]) / 100) \
		            + 2 * tns->bw + 6;
		if (thisdim >= mindim && tns->zl == oldzl)
			tns->zmultl = oldzmultl;
		else
			tns->dim = thisdim;
	}
	if (tns->zl != oldzl) {
		for (i = 0; i < *tns->cnt; i++)
			tns_unload(tns, i);
		tns->max_scale = tns_max_scale(tns);
		tns->dirty = true;
	} else if (tns->zmultl != oldzmultl){
		tns->max_scale = tns_max_scale(tns);
		tns->dirty = true;
	}
	return ((tns->zl != oldzl) | (tns->zmultl != oldzmultl));
}

int tns_translate(tns_t *tns, int x, int y)
{
	int n;

	if (tns == NULL || tns->thumbs == NULL)
		return -1;
	if (x < tns->x || y < tns->y)
		return -1;

	n = tns->first + (y - tns->y) / tns->dim * tns->cols +
	    (x - tns->x) / tns->dim;
	if (n >= *tns->cnt)
		n = -1;

	return n;
}
