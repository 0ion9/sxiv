/* Copyright 2011, 2012 Bert Muennich
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
#define _IMAGE_CONFIG

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "image.h"
#include "options.h"
#include "util.h"
#include "config.h"
#include "thumbs.h"

#if HAVE_LIBEXIF
#include <libexif/exif-data.h>
#endif

#if HAVE_GIFLIB
#include <gif_lib.h>
enum { DEF_GIF_DELAY = 75 };
#endif

float zoom_min;
float zoom_max;

static int zoomdiff(float z1, float z2)
{
	return (int) (z1 * 1000.0 - z2 * 1000.0);
}

void img_update_colormodifiers_current(img_t *);
void img_update_colormodifiers_none(img_t *);

void img_init(img_t *img, win_t *win)
{
	int i;
	zoom_min = zoom_levels[0] / 100.0;
	zoom_max = zoom_levels[ARRLEN(zoom_levels) - 1] / 100.0;

	if (img == NULL || win == NULL)
		return;

	imlib_context_set_display(win->env.dpy);
	imlib_context_set_visual(win->env.vis);
	imlib_context_set_colormap(win->env.cmap);

	img->im = NULL;
	img->win = win;
	img->scalemode = options->scalemode;
	img->zoom = options->zoom;
	img->zoom = MAX(img->zoom, zoom_min);
	img->zoom = MIN(img->zoom, zoom_max);
	img->checkpan = false;
	img->dirty = false;
	img->aa = ANTI_ALIAS;
	img->alpha = ALPHA_LAYER;
	img->silhouetting = 0;
	img->negate_alpha = false;
	img->opacity = 6;
	img->show_mouse_pos = options->show_mouse_pos;
	img->multi.cap = img->multi.cnt = 0;
	img->multi.animate = options->animate;
	img->multi.length = 0;
	img->tile.mode = 0;
	for (i = 0; i < ARRLEN(img->tile.cache); i++)
		img->tile.cache[i] = NULL;
	img->tile.dirty_cache = true;

	img->cmod = imlib_create_color_modifier();
	imlib_context_set_color_modifier(img->cmod);
	img->gamma = MIN(MAX(options->gamma, -GAMMA_RANGE), GAMMA_RANGE);
	img_update_colormodifiers_current(img);

	img->ss.on = options->slideshow > 0;
	img->ss.delay = options->slideshow > 0 ? options->slideshow * 100 : SLIDESHOW_DELAY * 100;
}

#if HAVE_LIBEXIF
void exif_auto_orientate(const fileinfo_t *file)
{
	ExifData *ed;
	ExifEntry *entry;
	int byte_order, orientation = 0;

	if ((ed = exif_data_new_from_file(file->path)) == NULL)
		return;
	byte_order = exif_data_get_byte_order(ed);
	entry = exif_content_get_entry(ed->ifd[EXIF_IFD_0], EXIF_TAG_ORIENTATION);
	if (entry != NULL)
		orientation = exif_get_short(entry->data, byte_order);
	exif_data_unref(ed);

	switch (orientation) {
		case 5:
			imlib_image_orientate(1);
		case 2:
			imlib_image_flip_vertical();
			break;
		case 3:
			imlib_image_orientate(2);
			break;
		case 7:
			imlib_image_orientate(1);
		case 4:
			imlib_image_flip_horizontal();
			break;
		case 6:
			imlib_image_orientate(1);
			break;
		case 8:
			imlib_image_orientate(3);
			break;
	}
}
#endif

#if HAVE_GIFLIB
bool img_load_gif(img_t *img, const fileinfo_t *file)
{
	GifFileType *gif;
	GifRowType *rows = NULL;
	GifRecordType rec;
	ColorMapObject *cmap;
	DATA32 bgpixel, *data, *ptr;
	DATA32 *prev_frame = NULL;
	Imlib_Image im;
	int i, j, bg, r, g, b;
	int x, y, w, h, sw, sh;
	int px, py, pw, ph;
	int intoffset[] = { 0, 4, 2, 1 };
	int intjump[] = { 8, 8, 4, 2 };
	int transp = -1;
	unsigned int disposal = 0, prev_disposal = 0;
	unsigned int delay = 0;
	bool err = false;

	if (img->multi.cap == 0) {
		img->multi.cap = 8;
		img->multi.frames = (img_frame_t*)
		                    s_malloc(sizeof(img_frame_t) * img->multi.cap);
	}
	img->multi.cnt = img->multi.sel = 0;
	img->multi.length = 0;

#if defined(GIFLIB_MAJOR) && GIFLIB_MAJOR >= 5
	gif = DGifOpenFileName(file->path, NULL);
#else
	gif = DGifOpenFileName(file->path);
#endif
	if (gif == NULL) {
		warn("could not open gif file: %s", file->name);
		return false;
	}
	bg = gif->SBackGroundColor;
	sw = gif->SWidth;
	sh = gif->SHeight;
	px = py = pw = ph = 0;

	do {
		if (DGifGetRecordType(gif, &rec) == GIF_ERROR) {
			err = true;
			break;
		}
		if (rec == EXTENSION_RECORD_TYPE) {
			int ext_code;
			GifByteType *ext = NULL;

			DGifGetExtension(gif, &ext_code, &ext);
			while (ext) {
				if (ext_code == GRAPHICS_EXT_FUNC_CODE) {
					if (ext[1] & 1)
						transp = (int) ext[4];
					else
						transp = -1;

					delay = 10 * ((unsigned int) ext[3] << 8 | (unsigned int) ext[2]);
					disposal = (unsigned int) ext[1] >> 2 & 0x7;
				}
				ext = NULL;
				DGifGetExtensionNext(gif, &ext);
			}
		} else if (rec == IMAGE_DESC_RECORD_TYPE) {
			if (DGifGetImageDesc(gif) == GIF_ERROR) {
				err = true;
				break;
			}
			x = gif->Image.Left;
			y = gif->Image.Top;
			w = gif->Image.Width;
			h = gif->Image.Height;

			rows = (GifRowType*) s_malloc(h * sizeof(GifRowType));
			for (i = 0; i < h; i++)
				rows[i] = (GifRowType) s_malloc(w * sizeof(GifPixelType));
			if (gif->Image.Interlace) {
				for (i = 0; i < 4; i++) {
					for (j = intoffset[i]; j < h; j += intjump[i])
						DGifGetLine(gif, rows[j], w);
				}
			} else {
				for (i = 0; i < h; i++)
					DGifGetLine(gif, rows[i], w);
			}

			ptr = data = (DATA32*) s_malloc(sizeof(DATA32) * sw * sh);
			cmap = gif->Image.ColorMap ? gif->Image.ColorMap : gif->SColorMap;
			r = cmap->Colors[bg].Red;
			g = cmap->Colors[bg].Green;
			b = cmap->Colors[bg].Blue;
			bgpixel = 0x00ffffff & (r << 16 | g << 8 | b);

			for (i = 0; i < sh; i++) {
				for (j = 0; j < sw; j++) {
					if (i < y || i >= y + h || j < x || j >= x + w ||
					    rows[i-y][j-x] == transp)
					{
						if (prev_frame != NULL && (prev_disposal != 2 ||
						    i < py || i >= py + ph || j < px || j >= px + pw))
						{
							*ptr = prev_frame[i * sw + j];
						} else {
							*ptr = bgpixel;
						}
					} else {
						r = cmap->Colors[rows[i-y][j-x]].Red;
						g = cmap->Colors[rows[i-y][j-x]].Green;
						b = cmap->Colors[rows[i-y][j-x]].Blue;
						*ptr = 0xff << 24 | r << 16 | g << 8 | b;
					}
					ptr++;
				}
			}

			im = imlib_create_image_using_copied_data(sw, sh, data);

			for (i = 0; i < h; i++)
				free(rows[i]);
			free(rows);
			free(data);

			if (im == NULL) {
				err = true;
				break;
			}

			imlib_context_set_image(im);
			imlib_image_set_format("gif");
			if (transp >= 0)
				imlib_image_set_has_alpha(1);

			if (disposal != 3)
				prev_frame = imlib_image_get_data_for_reading_only();
			prev_disposal = disposal;
			px = x, py = y, pw = w, ph = h;

			if (img->multi.cnt == img->multi.cap) {
				img->multi.cap *= 2;
				img->multi.frames = (img_frame_t*)
				                    s_realloc(img->multi.frames,
				                              img->multi.cap * sizeof(img_frame_t));
			}
			img->multi.frames[img->multi.cnt].im = im;
			img->multi.frames[img->multi.cnt].delay = delay > 0 ? delay : DEF_GIF_DELAY;
			img->multi.length += img->multi.frames[img->multi.cnt].delay;
			img->multi.cnt++;
		}
	} while (rec != TERMINATE_RECORD_TYPE);

#if defined(GIFLIB_MAJOR) && GIFLIB_MAJOR >= 5 && GIFLIB_MINOR >= 1
	DGifCloseFile(gif, NULL);
#else
	DGifCloseFile(gif);
#endif

	if (err && (file->flags & FF_WARN))
		warn("corrupted gif file: %s", file->name);

	if (img->multi.cnt > 1) {
		imlib_context_set_image(img->im);
		imlib_free_image();
		img->im = img->multi.frames[0].im;
	} else if (img->multi.cnt == 1) {
		imlib_context_set_image(img->multi.frames[0].im);
		imlib_free_image();
		img->multi.cnt = 0;
	}

	imlib_context_set_image(img->im);

	return !err;
}
#endif /* HAVE_GIFLIB */

void img_tiles_recache(img_t *, bool);

bool img_load(img_t *img, const fileinfo_t *file)
{
	const char *fmt;

	if (img == NULL || file == NULL || file->name == NULL || file->path == NULL)
		return false;

	if (access(file->path, R_OK) < 0 ||
	    (img->im = imlib_load_image(file->path)) == NULL)
	{
		if (file->flags & FF_WARN)
			warn("could not open image: %s", file->name);
		return false;
	}

	imlib_context_set_image(img->im);
	imlib_image_set_changes_on_disk();

#if HAVE_LIBEXIF
	exif_auto_orientate(file);
#endif

	if ((fmt = imlib_image_format()) != NULL) {
#if HAVE_GIFLIB
		if (STREQ(fmt, "gif"))
			img_load_gif(img, file);
#endif
	}
	img->w = imlib_image_get_width();
	img->h = imlib_image_get_height();
	img->checkpan = true;
	img->dirty = true;
	img->tile.dirty_cache = true;

	// colormods are pre-applied (cached) in tile mode
	if (img->tile.mode > 0)
	    img_tiles_recache(img, 1);
	else
		img_update_colormodifiers_current(img);
	return true;
}

void img_close(img_t *img, bool decache)
{
	int i;

	if (img == NULL)
		return;

	if (img->multi.cnt > 0) {
		for (i = 0; i < img->multi.cnt; i++) {
			imlib_context_set_image(img->multi.frames[i].im);
			imlib_free_image();
		}
		img->multi.cnt = 0;
		img->im = NULL;
	} else if (img->im != NULL) {
		imlib_context_set_image(img->im);
		if (decache)
			imlib_free_image_and_decache();
		else
			imlib_free_image();
		img->im = NULL;
	}
}

bool img_active_colormods(img_t *img)
{
	if (img->opacity < 6 || img->negate_alpha || img->silhouetting > 0 || img->gamma > 0)
		return true;
	return false;
}

bool img_need_trans(img_t *img)
{
	if (img->opacity < 6)
	    return true;
	return false;
}

void img_check_pan(img_t *img, bool moved)
{
	win_t *win;
	int ox, oy;
	float w, h;

	if (img == NULL || img->im == NULL || img->win == NULL)
		return;

	// notes about img->[xy]:
	// pressing Right reduces img->x
	// pressing Left increases it (up to 0, if image is larger than display)
	// given that right and left DO, in fact, pan right and left,
	// one probably should conclude that img->x < 0 means (img->x / zoom) pixels off the left of the image.
	//

	win = img->win;
	w = img->w * img->zoom;
	h = img->h * img->zoom;
	if (img->tile.mode > 0) {
		// maybe should just set this to win->[wh]?
		w *= 12;
		h *= 12;
	}
	ox = img->x;
	oy = img->y;

	if (w < win->w)
		img->x = (win->w - w) / 2;
	else if (img->x > 0)
		img->x = 0;
	else if (img->x + w < win->w)
		img->x = win->w - w;
	if (h < win->h)
		img->y = (win->h - h) / 2;
	else if (img->y > 0)
		img->y = 0;
	else if (img->y + h < win->h)
		img->y = win->h - h;

	if (img->tile.mode > 0) {
		float stepx = img->w * img->zoom;
		float stepy = img->h * img->zoom;
		if (img->tile.mode != 1) {
			stepx *= 12;
			stepy *= 12;
		}
		if (img->x < 0){
			warn("Cneg x %f", img->x);
			while (img->x < (-stepx))
				img->x += stepx;
			warn("-> %f", img->x);
		} else {
			warn("Cpos x %f", img->x);
			while (img->x > stepx)
				img->x -= stepx;
		}

		if (img->y < 0) {
			warn("Cneg y %f", img->y);
			while (img->y < (-stepy))
				img->y += stepy;
		} else {
			while (img->y > stepy)
				img->y -= stepy;
		}
	}

	if (!moved && (ox != img->x || oy != img->y))
		img->dirty = true;
	warn("check_pan final x,y = %f, %f", img->x, img->y);
}

void img_update_antialias(img_t *img)
{
	char oldaa, newaa;

	oldaa = imlib_context_get_anti_alias();
	newaa = oldaa;

	if (img->aa == 2) {
		if ((img->zoom - IMAGE_PIXELIZE_AT) < -0.001)
			newaa = 1;
		else
			newaa = 0;
	} else {
		newaa = img->aa;
	}

	if (newaa != oldaa) {
		imlib_context_set_anti_alias(newaa);
		img->dirty = true;
	}
}

bool img_fit(img_t *img)
{
	float z, zmax, zw, zh;

	if (img == NULL || img->im == NULL || img->win == NULL)
		return false;
	if (img->scalemode == SCALE_ZOOM)
		return false;

	zmax = img->scalemode == SCALE_DOWN ? 1.0 : zoom_max;
	zw = (float) img->win->w / (float) img->w;
	zh = (float) img->win->h / (float) img->h;
	// minimum 3x3 tiles for fit in tiled mode
	if (img->tile.mode > 0){
		zw /= 3;
		zh /= 3;
	}

	switch (img->scalemode) {
		case SCALE_WIDTH:
			z = zw;
			break;
		case SCALE_HEIGHT:
			z = zh;
			break;
		default:
			z = MIN(zw, zh);
			break;
	}

	z = MAX(z, zoom_min);
	z = MIN(z, zmax);

	if (zoomdiff(z, img->zoom) != 0) {
		img->zoom = z;
		img_update_antialias(img);
		img->dirty = true;
		return true;
	} else {
		return false;
	}
}
/*
* BDACDD
 * CADBOD
 * ACBDCC
 * DBCABB
 * CADBAA
 * CABBBD
 */

/* We could also use this for rotated versions, ie. the set
 * rot0 rot90 rot180 rot270.
 * With a larger pattern, we might even manage both together -- ie.
 * rot0 rot90 rot180 rot270 xflip yflip
 */

static unsigned char _all_adj_layout[6][6] = {
	{1, 3, 0, 2, 3, 3},
	{2, 0, 3, 1, 0, 3},
	{0, 2, 1, 3, 2, 2},
	{3, 1, 2, 0, 1, 1},
	{2, 0, 3, 1, 0, 0},
	{2, 0, 2, 2, 2, 3}
};


/* all_adj including rot90's. generated randomly, with adjacency verif.

'BEDBCADFCCEE'
'FBCAFCFCDCBE'
'ECABCACCDFFF'
'BAFEABDBFDFE'
'ECEFEBBAEBAD'
'CFADBEECFDEE'
'EFBFABADDFDC'
'FDBAFABDCFED'
'AADADDDFFAFB'
'BFEEFCFEDFAC'
'ADFCEEECBBCA'
'FEEFEBFCEBCD'
*/

/* static unsigned char _all_adj_layout12[12][12] = {
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
	{},
*/

// XXX hardcoded '12' size
void _img_update_tiling_layout(img_t *img, int layout_index)
{
	int x, y;
	unsigned char v;
	char buf[13];
    switch (layout_index) {
		case 0:
			warn("SETTING NOFLIPS LAYOUT");
			for (y = 0; y < 12; y++) {
				for (x = 0; x < 12; x++) {
					img->tile.layout[y][x] = 0;
				}
			}
			break;
		case 1:
			warn("SETTING ALLADJ LAYOUT");
			for (y = 0; y < 12; y++) {
				for (x = 0; x < 12; x++) {
					v = _all_adj_layout[y % 6][x % 6];
					warn("v = %d", v);
					img->tile.layout[y][x] = v;
				}
			}
			break;
		case 2:
			warn("SETTING RANDOM LAYOUT");
			for (y = 0; y < 12; y++) {
				for (x = 0; x < 12; x++) {
					v = random() & 0x3;
					img->tile.layout[y][x] = v;
				}
			}
			break;
		case 3:
			// 4 flips + rot90 + rot270
			warn("SETTING RANDOM(+rotate) LAYOUT");
			for (y = 0; y < 12; y++) {
				for (x = 0; x < 12; x++) {
					v = random() & 0x5;
					img->tile.layout[y][x] = v;
				}
			}
			break;

	}
    for (y = 0; y < 12; y++) {
		for (x = 0; x < 12; x++) {
			buf[x] = 48 + img->tile.layout[y][x];
		}
		buf[12] = 0;
		warn(buf);
	}
}

/* TILING
 *
 * There are a number of tiling patterns using VHFLIPS that may be of interest:
 * Rotated versions of these are not listed here.
 *
 * For the sake of genericness, the original version is represented as 'A'
 * , horizontally flipped as 'B', vertically flipped as 'C', and HVflipped as 'D'.
 *
 *
 * Unflipped:
 *  A
 *
 * Basic:
 *
 *  AB
 *  CD
 *
 * Random:
 *  (Random)
 *
 * All adjacencies, 6x6:
 *
 * BDACDD
 * CADBOD
 * ACBDCC
 * DBCABB
 * CADBAA
 * CABBBD
 *
 *  NOTE: there are twelve possible adjacencies in a single axis:
 *        BV BH BO VB VH VO HB HV HO OV OB OH
 *        plus four more self-self:
 *        BB HH VV OO
 *  There are four axes: X, Y, XY and -XY.
 *  The above pattern was solved with the help of a Python matrix checker ('adjsolve.py'),
 *  which verified it satisfies all X, Y, +XY and -XY adjacency constraints, including self-self.
 *
 *  This matrix is also applicable to any set of four tiles, naturally.
 *
 * Tilings for 2-tile set:
 *
 * ABA
 * BAB
 * ABA
 *
 * 3-tile set:
 *
 * ABCABB
 * CABCCA
 * BACAAB
 *
 */


// draw tiles
// expects img->tile.cache to be prefilled.
//
// XXX irregularities in output pixel size at zoom=3.0.
// rounding error.
//


void img_draw_tiles(img_t *img)
{
	float x, y;
	int tx, ty;
	float stepx, stepy;
	float initx, inity;
	float winw, winh;
	int ntiles;
	int sx, sy, sw, sh;
	int dx, dy, dw, dh;
	win_t *win;

	win = img->win;
	ntiles = win->w / (img->w * img->zoom);
	if ((ntiles * (img->w * img->zoom)) < win->w)
		ntiles++;
	dh = win->h / (img->h * img->zoom);
	if ((dh * (img->h * img->zoom)) < win->h);
		dh++;
	ntiles = ntiles * dh;
	winw = win->w;
	winh = win->h;
	stepx = img->w * img->zoom;
	stepy = img->h * img->zoom;
	tx = 0;
	ty = 0;
	x = img->x;
	y = img->y;
	// setup offset into tile pattern
	if (x < 0){
		warn("neg x %f", x);
		while (x < (-img->w * img->zoom)) {
			tx--;
		    x += (img->w * img->zoom);
		}
		warn("-> %f", x);
	} else {
		warn("pos x %f", x);
		while (x > (img->w * img->zoom)) {
		    x -= (img->w * img->zoom);
		    tx++;
		}
	}

	if (y < 0) {
		warn("neg y %f", y);
		while (y < (-img->h * img->zoom)) {
		    y += (img->h * img->zoom);
		    ty--;
		}
	} else {
		while (y > (img->h * img->zoom)) {
		    y -= (img->h * img->zoom);
		    ty++;
		}
	}
	tx = tx % 12;
	ty = ty % 12;
	if (tx < 0)
	    tx = 12 + tx;
	if (ty < 0)
	    ty = 12 + ty;
	// at this point, a negative x or y will be no more than 99% of a tile offwindow
	initx = x;
	inity = y;
	/*while (initx < -(img->zoom * img->w))
	    initx += (img->zoom * img->w);
	while (inity < -(img->zoom * img->h))
	    inity += (img->zoom * img->h);*/

	// note: img->y == -100 means we need to render the bottom 100 px of the image at the top of the display, etc.
	//

    // colormodifiers are applied in the caching stage, don't double-apply them
	img_update_colormodifiers_none(img);
	// XXX use ntiles to decide how large of an area to fill.
	// actually, that should be part of zoom setup?
	warn("simpletiling, img->x = %f, img->y = %f", img->x, img->y);
	warn("initx = %f, inity = %f", initx, inity);
	for (y = inity; y < winh; y += stepy) {
		for (x = initx; x < winw; x += stepx) {
			// maybe needs clipping?
			// anyway appears to work.

			if (x < 0) {
				sx = -(x / img->zoom + 0.5);
				sw = img->w - sx;
				dw = (sw * img->zoom) + 0.5;
				dx = 0;
				warn("@ %f, x == %f -> sx, dx = %d, %d; sw, dw = %d, %d", img->zoom, x, \
					 sx, dx, sw, dw);
				warn("tx %d, ty %d", tx, ty);
			} else {
				sw = img->w;
				sx = 0;
				dx = x;
				dw = (img->w * img->zoom + 0.5);
				warn("@ %f, x == %f -> sx, dx = %d, %d; sw, dw = %d, %d", img->zoom, x, \
					 sx, dx, sw, dw);
			}

			if (y < 0) {
				sy = -(y / img->zoom + 0.5);
				sh = img->h - sy;
				dh = (sh+ 0.5) * img->zoom;
				dy = 0;
				warn("@ %f, y == %f -> sy, dy = %d, %d; sh, dh = %d, %d", img->zoom, y, \
					 sy, dy, sh, dh);

			} else {
				sh = img->h;
				sy = 0;
				dh = (img->h * img->zoom + 0.5);
				dy = y;
				warn("@ %f, y == %f -> sy, dy = %d, %d; sh, dh = %d, %d", img->zoom, y, \
					 sy, dy, sh, dh);
			}


			imlib_context_set_image(img->tile.cache[img->tile.layout[ty][tx]]);
			imlib_render_image_part_on_drawable_at_size(sx, sy, sw, sh, dx, dy, dw, dh);
			tx += 1;
			tx = tx % 12;
		}
		ty += 1;
		ty = ty % 12;
	}
	img_update_colormodifiers_current(img);
}

void img_tiles_recache(img_t *img, bool blend)
{
	int i;
	Imlib_Image im = img->im;
	bool need_trans;

    // XXX we need to do the job of rendering the initial image, here
    // (with or without transparency effects etc)
    // rather than depending on the caller.
	if (img->tile.dirty_cache != true)
		return;
	if (img->tile.mode == 0)
		return;
	for (i = 1; i < 6; i++) {
		if (img->tile.cache[i] != NULL) {
			imlib_context_set_image(img->tile.cache[i]);
			imlib_free_image();
		}
	}
	imlib_context_set_image(im);
	need_trans = true;
	if ((imlib_image_has_alpha() == false) && (img->opacity == 6))
	    need_trans = false;
	if (!need_trans) {
		img->tile.cache[0] = imlib_clone_image();
		imlib_context_set_image(img->tile.cache[0]);
		img_update_colormodifiers_current(img);
		imlib_apply_color_modifier();
		blend = 0;
	} else {
		Imlib_Image bg;

		imlib_context_set_image(img->im);
		// create enough checkerboard/whatever to composite onto...
		if ((bg = imlib_create_image(img->w, img->h)) == NULL)
			die("could not allocate memory");
		imlib_context_set_image(bg);
		imlib_image_set_has_alpha(0);

		if (img->alpha) {
			int i, c, r;
			DATA32 col[2] = { 0xFF666666, 0xFF999999 };
			DATA32 * data = imlib_image_get_data();
			warn("CHECKS ALPHA");
			// working note: I think this generates a checkerboard pattern.
			for (r = 0; r < img->h; r++) {
				i = r * img->w;
				if (r == 0 || r == 8) {
					for (c = 0; c < img->w; c++)
						data[i++] = col[!(c & 8) ^ !r];
				} else {
					memcpy(&data[i], &data[(r & 8) * img->w], img->w * sizeof(data[0]));
				}
			}
			imlib_image_put_back_data(data);
		} else {
			int c;
			warn("FLAT ALPHA");
			c = img->win->fullscreen ? img->win->fscol : img->win->bgcol;
			imlib_context_set_color(c >> 16 & 0xFF, c >> 8 & 0xFF, c & 0xFF, 0xFF);
			imlib_image_fill_rectangle(0, 0, img->w, img->h);
		}
		img_update_colormodifiers_current(img);
		imlib_blend_image_onto_image(img->im, 0, 0, 0, img->w, img->h, 0, 0, img->w, img->h);
		img->tile.cache[0] = bg;
		im = bg;
		need_trans = false;
		blend = 1;
	}
	img_update_colormodifiers_none(img);
	imlib_context_set_image(im);
	imlib_image_set_has_alpha(need_trans);
	imlib_context_set_blend(blend);
	img->tile.cache[1] = imlib_clone_image();
	imlib_context_set_image(img->tile.cache[1]);
	imlib_image_flip_horizontal();
	imlib_image_set_has_alpha(need_trans);
	imlib_context_set_blend(blend);
	imlib_context_set_image(img->tile.cache[0]);
	img->tile.cache[2] = imlib_clone_image();
	imlib_context_set_image(img->tile.cache[2]);
	imlib_image_flip_vertical();
	imlib_image_set_has_alpha(need_trans);
	imlib_context_set_blend(blend);
	imlib_context_set_image(img->tile.cache[0]);
	img->tile.cache[3] = imlib_clone_image();
	imlib_context_set_image(img->tile.cache[3]);
	imlib_image_flip_horizontal();
	imlib_image_flip_vertical();
	imlib_image_set_has_alpha(need_trans);
	imlib_context_set_blend(blend);
	imlib_context_set_image(img->tile.cache[0]);
	img->tile.cache[4] = imlib_clone_image();
	imlib_context_set_image(img->tile.cache[4]);
	imlib_image_orientate(1);
	imlib_image_set_has_alpha(need_trans);
	imlib_context_set_blend(blend);
	imlib_context_set_image(img->tile.cache[0]);
	img->tile.cache[5] = imlib_clone_image();
	imlib_context_set_image(img->tile.cache[5]);
	imlib_image_orientate(3);
	imlib_image_set_has_alpha(need_trans);
	imlib_context_set_blend(blend);
	img->tile.dirty_cache = false;
}

void img_render(img_t *img)
{
	win_t *win;
	int sx, sy, sw, sh;
	int dx, dy, dw, dh;
	Imlib_Image bg;
	unsigned long c;
	bool need_trans;

	if (img == NULL || img->im == NULL || img->win == NULL)
		return;

	win = img->win;
	img_fit(img);

	if (img->checkpan) {
		img_check_pan(img, false);
		img->checkpan = false;
	}

	if (!img->dirty)
		return;

	/* calculate source and destination offsets:
	 *   - part of image drawn on full window, or
	 *   - full image drawn on part of window
	 *
	 * XXX needs relocation. Each image render wants to calculate this,
	 * and when tiling is on, there are many image renders
	 */
	if (img->x <= 0) {
		sx = -img->x / img->zoom + 0.5;
		sw = win->w / img->zoom;
		dx = 0;
		dw = win->w;
	} else {
		sx = 0;
		sw = img->w;
		dx = img->x;
		dw = img->w * img->zoom;
	}
	if (img->y <= 0) {
		sy = -img->y / img->zoom + 0.5;
		sh = win->h / img->zoom;
		dy = 0;
		dh = win->h;
	} else {
		sy = 0;
		sh = img->h;
		dy = img->y;
		dh = img->h * img->zoom;
	}

	win_clear(win);

	imlib_context_set_image(img->im);
	img_update_antialias(img);
	imlib_context_set_drawable(win->buf.pm);

	// this will be true if image originally had alpha OR lowered opacity
	// is set.
	need_trans = img_need_trans(img);
	if (imlib_image_has_alpha())
		need_trans = true;

	if (need_trans && img->tile.mode == 0) {
		// create enough checkerboard/whatever to composite onto...
		if ((bg = imlib_create_image(dw, dh)) == NULL)
			die("could not allocate memory");
		imlib_context_set_image(bg);
		imlib_image_set_has_alpha(0);

		if (img->alpha) {
			int i, c, r;
			DATA32 col[2] = { 0xFF666666, 0xFF999999 };
			DATA32 * data = imlib_image_get_data();
			warn("CHECKS ALPHA");
			// working note: I think this generates a checkerboard pattern.
			for (r = 0; r < dh; r++) {
				i = r * dw;
				if (r == 0 || r == 8) {
					for (c = 0; c < dw; c++)
						data[i++] = col[!(c & 8) ^ !r];
				} else {
					memcpy(&data[i], &data[(r & 8) * dw], dw * sizeof(data[0]));
				}
			}
			imlib_image_put_back_data(data);
		} else {
			warn("FLAT ALPHA");
			c = win->fullscreen ? win->fscol : win->bgcol;
			imlib_context_set_color(c >> 16 & 0xFF, c >> 8 & 0xFF, c & 0xFF, 0xFF);
			imlib_image_fill_rectangle(0, 0, dw, dh);
		}
		// so, now we have our BG.
		// it's set as the context image
		// we throw the main image onto it.
		// now would be the time to put it in the img->tile.cache. and generate alts.
		img_update_colormodifiers_current(img);
		// note: sw + sh are too small, either here or inside img_draw_tiles

		imlib_blend_image_onto_image(img->im, 0, sx, sy, sw, sh, 0, 0, dw, dh);
		img_update_colormodifiers_none(img);
		imlib_render_image_on_drawable(dx, dy);
		imlib_free_image();
		img_update_colormodifiers_current(img);
	} else {
		// note : for now, tiling is only supported on images without alpha channel.
		//img_update_colormodifiers(img);
		/*if (img->opacity < 6){
				// XXX hack. also, thumbnails need set_has_alpha called too.
				// really, this needs to be done when switching images.
				imlib_context_set_blend(1);
				imlib_image_set_has_alpha(1);
		}
		else {
			imlib_context_set_blend(1);
			imlib_image_set_has_alpha(0);
		}*/
		//imlib_context_set_color_modifier(img->cmod);
		if (img->tile.mode == 0){
			//img_update_colormodifiers_current(img);
			imlib_render_image_part_on_drawable_at_size(sx, sy, sw, sh, dx, dy, dw, dh);
		} else {
			warn("tiling");
			// cmods are handled inside these:
			if (img->tile.dirty_cache)
				img_tiles_recache(img, 1);
			img_draw_tiles(img);
		}
	}
	imlib_context_set_image(img->im);
	img->dirty = false;
}

bool img_fit_win(img_t *img, scalemode_t sm)
{
	float oz;

	if (img == NULL || img->im == NULL || img->win == NULL)
		return false;

	oz = img->zoom;
	img->scalemode = sm;

	if (img_fit(img)) {
		img->x = img->win->w / 2 - (img->win->w / 2 - img->x) * img->zoom / oz;
		img->y = img->win->h / 2 - (img->win->h / 2 - img->y) * img->zoom / oz;
		img->checkpan = true;
		return true;
	} else {
		return false;
	}
}

bool img_zoom(img_t *img, float z)
{
	if (img == NULL || img->im == NULL || img->win == NULL)
		return false;

	z = MAX(z, zoom_min);
	z = MIN(z, zoom_max);

	img->scalemode = SCALE_ZOOM;

	if (zoomdiff(z, img->zoom) != 0) {
		img->x = img->win->w / 2 - (img->win->w / 2 - img->x) * z / img->zoom;
		img->y = img->win->h / 2 - (img->win->h / 2 - img->y) * z / img->zoom;
		img->zoom = z;
		img->checkpan = true;
		img_update_antialias(img);
		img->dirty = true;
		return true;
	} else {
		return false;
	}
}

bool img_zoom_in(img_t *img)
{
	int i;
	float z;

	if (img == NULL || img->im == NULL)
		return false;

	for (i = 1; i < ARRLEN(zoom_levels); i++) {
		z = zoom_levels[i] / 100.0;
		if (zoomdiff(z, img->zoom) > 0)
			return img_zoom(img, z);
	}
	return false;
}

bool img_zoom_out(img_t *img)
{
	int i;
	float z;

	if (img == NULL || img->im == NULL)
		return false;

	for (i = ARRLEN(zoom_levels) - 2; i >= 0; i--) {
		z = zoom_levels[i] / 100.0;
		if (zoomdiff(z, img->zoom) < 0)
			return img_zoom(img, z);
	}
	return false;
}

bool img_move(img_t *img, float dx, float dy)
{
	float ox, oy;

	if (img == NULL || img->im == NULL)
		return false;

	ox = img->x;
	oy = img->y;

	img->x += dx;
	img->y += dy;

	img_check_pan(img, true);

	if (ox != img->x || oy != img->y) {
		img->dirty = true;
		return true;
	} else {
		return false;
	}
}

bool img_pan(img_t *img, direction_t dir, int d)
{
	/* d < 0: screen-wise
	 * d = 0: 1/5 of screen
	 * d > 0: num of pixels
	 */
	float x, y;

	if (img == NULL || img->im == NULL || img->win == NULL)
		return false;

	if (d > 0) {
		x = y = MAX(1, (float) d * img->zoom);
	} else {
		x = img->win->w / (d < 0 ? 1 : 5);
		y = img->win->h / (d < 0 ? 1 : 5);
	}

	switch (dir) {
		case DIR_LEFT:
			return img_move(img, x, 0.0);
		case DIR_RIGHT:
			return img_move(img, -x, 0.0);
		case DIR_UP:
			return img_move(img, 0.0, y);
		case DIR_DOWN:
			return img_move(img, 0.0, -y);
	}
	return false;
}

bool img_pan_edge(img_t *img, direction_t dir)
{
	int ox, oy;

	if (img == NULL || img->im == NULL || img->win == NULL)
		return false;

	ox = img->x;
	oy = img->y;

	if (dir & DIR_LEFT)
		img->x = 0;
	if (dir & DIR_RIGHT)
		img->x = img->win->w - img->w * img->zoom;
	if (dir & DIR_UP)
		img->y = 0;
	if (dir & DIR_DOWN)
		img->y = img->win->h - img->h * img->zoom;

	img_check_pan(img, true);

	if (ox != img->x || oy != img->y) {
		img->dirty = true;
		return true;
	} else {
		return false;
	}
}

void img_rotate(img_t *img, degree_t d)
{
	int i, ox, oy, tmp;

	if (img == NULL || img->im == NULL || img->win == NULL)
		return;

	imlib_context_set_image(img->im);
	imlib_image_orientate(d);

	for (i = 0; i < img->multi.cnt; i++) {
		if (i != img->multi.sel) {
			imlib_context_set_image(img->multi.frames[i].im);
			imlib_image_orientate(d);
		}
	}
	if (d == DEGREE_90 || d == DEGREE_270) {
		ox = d == DEGREE_90  ? img->x : img->win->w - img->x - img->w * img->zoom;
		oy = d == DEGREE_270 ? img->y : img->win->h - img->y - img->h * img->zoom;

		img->x = oy + (img->win->w - img->win->h) / 2;
		img->y = ox + (img->win->h - img->win->w) / 2;

		tmp = img->w;
		img->w = img->h;
		img->h = tmp;
		img->checkpan = true;
	}
	img->dirty = true;
}

void img_flip(img_t *img, flipdir_t d)
{
	int i;
	void (*imlib_flip_op[3])(void) = {
		imlib_image_flip_horizontal,
		imlib_image_flip_vertical,
		imlib_image_flip_diagonal
	};

	d = (d & (FLIP_HORIZONTAL | FLIP_VERTICAL)) - 1;

	if (img == NULL || img->im == NULL || d < 0 || d >= ARRLEN(imlib_flip_op))
		return;

	imlib_context_set_image(img->im);
	imlib_flip_op[d]();

	for (i = 0; i < img->multi.cnt; i++) {
		if (i != img->multi.sel) {
			imlib_context_set_image(img->multi.frames[i].im);
			imlib_flip_op[d]();
		}
	}
	img->dirty = true;
}



void img_cycle_antialias(img_t *img)
{
	if (img == NULL || img->im == NULL)
		return;

	img->aa = img->aa + 1;
	if (img->aa > 2)
		img->aa = 0;
	imlib_context_set_image(img->im);
	img_update_antialias(img);
}

void img_update_colormodifiers(img_t *img, int gamma, int silhouetting, bool negate_alpha, int opacity)
{
	double range;
	unsigned int i;
	DATA8 r[256];
	DATA8 g[256];
	DATA8 b[256];
	DATA8 a[256];
	if (img == NULL)
		return;
	img->dirty = true;
	// always begin by resetting.
	imlib_get_color_modifier_tables(r, g, b, a);
	for (i=0;i < 256; i++) {
			r[i] = i;
			g[i] = i;
			b[i] = i;
			a[i] = i;
	}
	// here, we handle the:
	//  * silhouetting
	//  * negate alpha
	//  * opacity
	//  * gamma
	// options.
	//
	// note that we don't take values directly from img, because
	// certain cases require us to ignore the settings on img.

	if (gamma != 0) {
		imlib_set_color_modifier_tables(r, g, b, a);
		range = gamma <= 0 ? 1.0 : GAMMA_MAX - 1.0;
		imlib_modify_color_modifier_gamma(1.0 + gamma * (range / GAMMA_RANGE));
		imlib_get_color_modifier_tables(r, g, b, a);
	}

	if (negate_alpha == 0 && opacity == 6 && silhouetting == 0)
	{
		imlib_set_color_modifier_tables(r, g, b, a);
		return;
	} else {
		int i;
		silhouetting = silhouetting - 1;

		imlib_get_color_modifier_tables(r, g, b, a);
		// silhouetting is the only effect aside from gamma that changes rgb
		if (silhouetting != -1) {
			for (i=0;i < 256; i++) {
				r[i] = SILHOUETTE_COLOR[silhouetting][0];
				g[i] = SILHOUETTE_COLOR[silhouetting][1];
				b[i] = SILHOUETTE_COLOR[silhouetting][2];
			}
		}
		// negate_alpha is applied before opacity
		for (i=0;i < 256; i++) {
			a[i] = ((negate_alpha ? 255 - i: i ) * opacity) / 6;
		}

	}
	imlib_set_color_modifier_tables(r, g, b, a);
}

void img_update_colormodifiers_current(img_t *img) {
	img_update_colormodifiers(img, img->gamma, img->silhouetting, img->negate_alpha, img->opacity);
}

void img_update_colormodifiers_none(img_t *img) {
	img_update_colormodifiers(img, 0, 0, false, 6);
}

void img_cycle_silhouetting(img_t *img)
{
	if (img == NULL)
		return;

	img->silhouetting = img->silhouetting + 1;
	// XXX needs testing.
	if (img->silhouetting > ARRLEN(SILHOUETTE_COLOR))
	    img->silhouetting = 0;
	img_update_colormodifiers_current(img);
	img->tile.dirty_cache = 1;
	//img_tiles_recache(img, 1);
}

void img_toggle_negalpha(img_t *img)
{
	if (img == NULL)
		return;
	img->negate_alpha = !img->negate_alpha;
	img_update_colormodifiers_current(img);
	img->tile.dirty_cache = true;
	//img_tiles_recache(img, 1);
}

void img_cycle_tiling(img_t *img)
{
	if (img == NULL)
		return;
	img->tile.mode = img->tile.mode + 1;
	if (img->tile.mode > 4)
		img->tile.mode = 0;
	if (img->tile.mode > 0)
	{
	    _img_update_tiling_layout(img, img->tile.mode - 1);
	    img_tiles_recache(img, 1);
	}
	if (img->tile.mode < 2)
		img->checkpan = true;
	img->dirty = true;
}


// XXX hack

extern tns_t tns;

bool img_cycle_opacity(img_t *img)
{
	int new_opacity;
	if (img == NULL)
		return false;
	new_opacity = img->opacity + 1;
	if (new_opacity > 6)
		new_opacity = 1;
	if (new_opacity != img->opacity){
		img->opacity = new_opacity;
		img_update_colormodifiers_current(img);
	    img->tile.dirty_cache = 1;
//		img_tiles_recache(img, 1);
	    return true;
	}
	return false;
}



bool img_change_gamma(img_t *img, int d)
{
	/* d < 0: decrease gamma
	 * d = 0: reset gamma
	 * d > 0: increase gamma
	 */
	int gamma;

	if (img == NULL)
		return false;

	if (d == 0)
		gamma = 0;
	else
		gamma = MIN(MAX(img->gamma + d, -GAMMA_RANGE), GAMMA_RANGE);

	if (img->gamma != gamma) {
		img->gamma = gamma;
		img_update_colormodifiers_current(img);
		img->tile.dirty_cache = 1;
		img_tiles_recache(img, 1);
		return true;
	} else {
		return false;
	}
}

bool img_frame_goto(img_t *img, int n)
{
	if (img == NULL || img->im == NULL)
		return false;
	if (n < 0 || n >= img->multi.cnt || n == img->multi.sel)
		return false;

	img->multi.sel = n;
	img->im = img->multi.frames[n].im;

	imlib_context_set_image(img->im);
	img->w = imlib_image_get_width();
	img->h = imlib_image_get_height();
	img->checkpan = true;
	img->dirty = true;

	return true;
}

bool img_frame_navigate(img_t *img, int d)
{
	if (img == NULL|| img->im == NULL || img->multi.cnt == 0 || d == 0)
		return false;

	d += img->multi.sel;
	if (d < 0)
		d = 0;
	else if (d >= img->multi.cnt)
		d = img->multi.cnt - 1;

	return img_frame_goto(img, d);
}

bool img_frame_animate(img_t *img)
{
	if (img == NULL || img->im == NULL || img->multi.cnt == 0)
		return false;

	if (img->multi.sel + 1 >= img->multi.cnt)
		img_frame_goto(img, 0);
	else
		img_frame_goto(img, img->multi.sel + 1);
	img->dirty = true;
	return true;
}

