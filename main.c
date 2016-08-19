/* Copyright 2011-2013 Bert Muennich
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

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#define XK_TECHNICAL
#define XK_PUBLISHING
#define XK_SPECIAL
#include <X11/keysym.h>
#include <X11/keysymdef.h>
#include <X11/XF86keysym.h>
#include <libgen.h>

#include "types.h"
#include "commands.h"
#include "image.h"
#include "options.h"
#include "thumbs.h"
#include "util.h"
#include "window.h"

#define _MAPPINGS_CONFIG
#include "config.h"

typedef struct {
	struct timeval when;
	bool active;
	timeout_f handler;
} timeout_t;

/* timeout handler functions: */
void redraw(void);
void reset_cursor(void);
void animate(void);
void slideshow(void);
void clear_resize(void);

appmode_t mode;
img_t img;
tns_t tns;
win_t win;

fileinfo_t *files;
int filecnt, fileidx;
int alternate;
int markcnt;

int prefix;
bool extprefix;
bool inputting_prefix;

bool resized = false;

typedef struct {
	int err;
	char *cmd;
} extcmd_t;

struct {
  extcmd_t f;
  int fd;
  unsigned int i, lastsep;
  bool open;
} info;

struct {
	extcmd_t f;
	bool warned;
} keyhandler;

timeout_t timeouts[] = {
	{ { 0, 0 }, false, redraw       },
	{ { 0, 0 }, false, reset_cursor },
	{ { 0, 0 }, false, animate      },
	{ { 0, 0 }, false, slideshow    },
	{ { 0, 0 }, false, clear_resize },
};

fileinfo_t *infobuf1 = NULL;
thumb_t *thumbbuf1 = NULL;
int buf1_size = 0;
fileinfo_t *infobuf2 = NULL;
thumb_t *thumbbuf2 = NULL;
int buf2_size = 0;

void cleanup(void)
{
	img_close(&img, false);
	tns_free(&tns);
	win_close(&win);
	if (infobuf1 != NULL)
		free(infobuf1);
	if (infobuf2 != NULL)
		free(infobuf2);
	if (thumbbuf1 != NULL)
		free(thumbbuf1);
	if (thumbbuf2 != NULL)
		free(thumbbuf2);
}

void check_add_file(char *filename, bool given)
{
	char *path;
	const char *bn;

	if (*filename == '\0')
		return;

	if (access(filename, R_OK) < 0 ||
	    (path = realpath(filename, NULL)) == NULL)
	{
		if (given)
			error(0, errno, "%s", filename);
		return;
	}

	if (fileidx == filecnt) {
		filecnt *= 2;
		files = erealloc(files, filecnt * sizeof(*files));
		memset(&files[filecnt/2], 0, filecnt/2 * sizeof(*files));
	}

	files[fileidx].name = estrdup(filename);
	files[fileidx].path = path;
	if ((bn = strrchr(files[fileidx].name , '/')) != NULL && bn[1] != '\0')
		files[fileidx].base = ++bn;
	else
		files[fileidx].base = files[fileidx].name;
	if (given)
		files[fileidx].flags |= FF_WARN;
	fileidx++;
}

// copy astartcount entries from a, followed by bcount entries from b, followed by aendcount entries from a, into dest
// aendcount or astartcount may be 0 (but they may not both be 0)

static void mix_arrays(unsigned char *a, unsigned char *b, unsigned char *dest, size_t size, int astartcount, int bcount, int aendcount, int destmax)
{
	int destindex = 0;

//	if ((destindex + astartcount) > destmax)
//		error("array bounds exceeded (bound: %d, to copy: %d)", destmax, destindex + astartcount);

	if (astartcount > 0)
		memcpy(dest, a, astartcount * size);

	destindex += astartcount;
	dest += (astartcount * size);

//	if ((destindex + bcount) > destmax)
//		error("array bounds exceeded (bound: %d, to copy: %d)", destmax, destindex + bcount);

	if (bcount > 0)
		memcpy(dest, b, bcount * size);

	dest += (bcount * size);

//	if ((destindex + endcount) > destmax)
//		error("array bounds exceeded (bound: %d, to copy: %d)", destmax, destindex + aendcount);

	if (aendcount > 0)
		memcpy(dest, a, aendcount * size);

}

void shift_marked_files_to(int index)
{
	int unmarked_index, marked_index;
	int i;
	if ((buf1_size == 0) || (infobuf1 == NULL) || (thumbbuf1 == NULL) || (buf1_size < (filecnt - markcnt)))
	{
		if (infobuf1 != NULL);
			free(infobuf1);
		if (thumbbuf1 != NULL);
			free(thumbbuf1);
		buf1_size = (filecnt - markcnt);
		if (buf1_size == 0)
			return;
		infobuf1 = (fileinfo_t *) emalloc(sizeof(fileinfo_t) * buf1_size);
		thumbbuf1 = (thumb_t *) emalloc(sizeof(thumb_t) * buf1_size);
	}

	if ((buf2_size == 0) || (infobuf2 == NULL) || (thumbbuf2 == NULL) || (buf2_size < markcnt))
	{
		if (infobuf2 != NULL);
			free(infobuf2);
		if (thumbbuf2 != NULL);
			free(thumbbuf2);
		buf2_size = markcnt;
		if (buf2_size == 0)
			return;
		infobuf2 = (fileinfo_t *) emalloc(sizeof(fileinfo_t) * buf2_size);
		thumbbuf2 = (thumb_t *) emalloc(sizeof(thumb_t) * buf2_size);
	}

	unmarked_index = 0;
	marked_index = 0;

	for (i=0; i<filecnt; i++){
		if (((files + i)->flags & FF_MARK)){
			memcpy(infobuf2 + marked_index, files + i, sizeof(fileinfo_t));
			memcpy(thumbbuf2 + marked_index, tns.thumbs + i, sizeof(thumb_t));
			marked_index++;
		} else {
			memcpy(infobuf1 + unmarked_index, files + i, sizeof(fileinfo_t));
			memcpy(thumbbuf1 + unmarked_index, tns.thumbs + i, sizeof(thumb_t));
			unmarked_index++;
		}
	}

	mix_arrays((unsigned char *)infobuf1, (unsigned char *)infobuf2, (unsigned char *)files, sizeof(fileinfo_t), index, markcnt, filecnt - index, filecnt);
	mix_arrays((unsigned char *)thumbbuf1, (unsigned char *)thumbbuf2, (unsigned char *)tns.thumbs, sizeof(thumb_t), index, markcnt, filecnt - index, filecnt);
}

void shift_marked_files(int direction)
{
	// shift marked files to either the start (-1) or end (1) of the list.

	fileinfo_t *infobuf;
	thumb_t *thumbbuf;
	int i, output_index;
	fileflags_t first, second;

	if (direction == 0)
		return;

	if (markcnt == 0)
		return;

	// adding support for moving to a particular index rather than just the start or end:
	// * index must be normalized (subtract one from final destination index, for each marked file found before original specified index)
	// * instead of copying [NONMARKED][MARKED] or vice versa, we must copy [NONMARKED_start][MARKED][NONMARKED_cont] or vice versa.
	//   to be precise, a normalized destination index of N means that we must copy N entries from nonmarked, followed by all entries from marked,
	//   followed by the remaining entries from nonmarked.


        // at last check, this returned 32, 24
        // meaning that for a filelist of 1000 items, 32000+24000 == 46000 bytes are allocated and freed each time
        // that shift_marked_files() is called.
	warn("sizes: f %d t %d", sizeof(fileinfo_t), sizeof(thumb_t));
	infobuf = (fileinfo_t *) emalloc(sizeof(fileinfo_t) * filecnt);
	thumbbuf = (thumb_t *) emalloc(sizeof(thumb_t) * filecnt);
	// super inefficient. We should keep around the buffers instead.


	if (direction < 0){
		first=FF_MARK;
		second=0;
	}
	else{
		first=0;
		second=FF_MARK;
	}

	output_index = 0;

	for (i=0; i<filecnt; i++){
		if (((files + i)->flags & FF_MARK) == first){
			memcpy(infobuf + output_index, files + i, sizeof(fileinfo_t));
			memcpy(thumbbuf + output_index, tns.thumbs + i, sizeof(thumb_t));
			output_index++;
		}
	}

	for (i=0; i<filecnt; i++){
		if (((files + i)->flags & FF_MARK) == second){
			memcpy(infobuf + output_index, files + i, sizeof(fileinfo_t));
			memcpy(thumbbuf + output_index, tns.thumbs + i, sizeof(thumb_t));
			output_index++;
		}
	}

	if (output_index != filecnt){
		fprintf(stderr,
			"sxiv:shift_marked_files() final output_index looks wrong (%d, expected %d). Terminating.\n",
			output_index, filecnt);
		cleanup();
		exit(EXIT_FAILURE);
	}

	memcpy(tns.thumbs, thumbbuf, sizeof(thumb_t) * filecnt);
	memcpy(files, infobuf, sizeof(fileinfo_t) * filecnt);

	free(infobuf);
	free(thumbbuf);

}

void swap_files(int n, int offset)
{
	fileinfo_t info;
	thumb_t thumb;
	int final_location;
	int actual_offset;

	final_location = n + offset;
	if (final_location < 0)
		final_location = 0;
	if (final_location > (filecnt - 1))
		final_location = filecnt - 1;

	actual_offset = final_location - n;
	if (actual_offset == 0)
		return;
	memcpy(&info, files + n, sizeof(fileinfo_t));
	memcpy(&thumb, tns.thumbs + n, sizeof(*tns.thumbs));
	memcpy(files + n, files + final_location, sizeof(fileinfo_t));
	memcpy(tns.thumbs + n, tns.thumbs + final_location, sizeof(*tns.thumbs));
	memcpy(files + final_location, &info, sizeof(fileinfo_t));
	memcpy(tns.thumbs + final_location, &thumb,sizeof(*tns.thumbs));
}

void shift_file(int n, int offset)
{
	fileinfo_t info;
	thumb_t thumb;
	int final_location;
	int actual_offset;
	int absolute_offset;

	final_location = n + offset;
	if (final_location < 0)
		final_location = 0;
	if (final_location > (filecnt - 1))
		final_location = filecnt - 1;

	actual_offset = final_location - n;
	if (actual_offset == 0)
		return;
	absolute_offset = actual_offset;
	if (actual_offset < 0)
		absolute_offset = actual_offset * -1;
	memcpy(&info, files + n, sizeof(fileinfo_t));

	if (actual_offset < 0) {
		memmove(files + final_location + 1, files + final_location, absolute_offset * sizeof(*files));
		memcpy(files + final_location, &info, sizeof(*files));
		if (tns.thumbs != NULL) {
			memcpy(&thumb, tns.thumbs + n, sizeof(*tns.thumbs));
			memmove(tns.thumbs + final_location + 1, tns.thumbs + final_location, absolute_offset * sizeof(*tns.thumbs));
			memcpy(tns.thumbs + final_location, &thumb, sizeof(*tns.thumbs));
		}
	} else {
		memmove(files + n, files + n + 1, absolute_offset * sizeof(*files));
		memcpy(files + final_location, &info, sizeof(*files));
		if (tns.thumbs != NULL) {
			memcpy(&thumb, tns.thumbs + n, sizeof(*tns.thumbs));
			memmove(tns.thumbs + n, tns.thumbs + n + 1, absolute_offset * sizeof(*tns.thumbs));
			memcpy(tns.thumbs + final_location, &thumb, sizeof(*tns.thumbs));
		}
	}

	if (alternate == n)
		alternate = final_location;
}

void remove_file(int n, bool manual)
{
	if (n < 0 || n >= filecnt)
		return;

	if (filecnt == 1) {
		if (!manual)
			fprintf(stderr, "sxiv: no more files to display, aborting\n");
		exit(manual ? EXIT_SUCCESS : EXIT_FAILURE);
	}
	if (files[n].flags & FF_MARK)
		markcnt--;

	if (files[n].path != files[n].name)
		free((void*) files[n].path);
	free((void*) files[n].name);

	if (n + 1 < filecnt) {
		if (tns.thumbs != NULL) {
			memmove(tns.thumbs + n, tns.thumbs + n + 1, (filecnt - n - 1) *
			        sizeof(*tns.thumbs));
			memset(tns.thumbs + filecnt - 1, 0, sizeof(*tns.thumbs));
		}
		memmove(files + n, files + n + 1, (filecnt - n - 1) * sizeof(*files));
	}
	filecnt--;
	if (fileidx >= filecnt)
		fileidx = filecnt - 1;
	if (n < alternate)
		alternate--;
}

#include <inttypes.h>

#include <stdint.h>

void clone_file(int n)
{
	char *newpath;
	char *newname;
	char *newbase;
	fileinfo_t *newinfo;
	fileinfo_t *oldinfo;
	thumb_t *newthumb = NULL;
	thumb_t *oldthumb = NULL;
	int i;

	if (n < 0 || n >= filecnt)
		return;
	if (files[n].flags & FF_MARK)
		markcnt++;
	fprintf(stderr, "sxiv: cloning index %d\n", n);
	for (i=0; i < filecnt; i++){
		fprintf(stderr, "%d : %s;%s;%s\n", i, files[i].path, files[i].name, files[i].base);
	}
	if (tns.thumbs == NULL)
		tns_init(&tns, files, &filecnt, &fileidx, &win);

	// the shenanigans we get up to with tns -- particularly, duplicating image pointers.. would suggest that
	// this code should crash. However, it doesn't.
	oldthumb = tns.thumbs;
	fprintf(stderr, "sxiv: tns.thumbs = %016" PRIxPTR "\n", (uintptr_t) tns.thumbs);
	fprintf(stderr, "sxiv: oldthumb = %016" PRIxPTR "\n", (uintptr_t) oldthumb);
	oldinfo = files;
	newname = strdup(files[n].name);
	newpath = newname;
	if (files[n].path != files[n].name)
		newpath = strdup(files[n].path);
	newbase = strdup(files[n].base);
	fprintf(stderr, "sxiv: newp,n,b = %016" PRIxPTR " %016" PRIxPTR "%016" PRIxPTR "\n", (uintptr_t) newpath, (uintptr_t) newname, (uintptr_t) newbase);
	newinfo = (fileinfo_t *) emalloc(sizeof(*oldinfo) * (filecnt + 1));
	if (oldthumb != NULL)
		newthumb = (thumb_t *) emalloc(sizeof(*oldthumb) * (filecnt + 1));
	// reallocate memory (files, tns)
	// copy the segment at start as-is
	fprintf(stderr, "sxiv: thumbsize = %d; allthumbs= %d\n", sizeof(*oldthumb), sizeof(*oldthumb) * (filecnt + 1));
	memcpy(newinfo, oldinfo, (n+1) * sizeof(*oldinfo));
	if (oldthumb != NULL)
		memcpy(newthumb, oldthumb, (n+1) * sizeof(*oldthumb));
	// insert the clone entry
	fprintf(stderr, "clone insertion at %d\n", n+1);
	memcpy(newinfo + (n + 1), oldinfo + n, sizeof(*oldinfo));
	if (oldthumb != NULL)
		memcpy(newthumb + (n + 1), oldthumb + n, sizeof(*oldthumb));
	newinfo[n+1].name = newname;
	newinfo[n+1].path = newpath;
	newinfo[n+1].base = newbase;
	if (n < (filecnt - 1)) {
		memcpy(newinfo + (n + 2), files + n + 1, (filecnt - (n + 1)) * sizeof(*files));
		if (oldthumb != NULL)
			memcpy(newthumb + (n + 2), tns.thumbs + n + 1, (filecnt - (n + 1)) * sizeof(*tns.thumbs));
	}
	// copy the segment at end (if any, ie, n + 1 < filecnt)
	// reassign files and tns.thumbs to point at new data
	// free the original data
	free(oldinfo);
	files = newinfo;
	if (oldthumb != NULL) {
		free(oldthumb);
		tns.thumbs = newthumb;
	}
	filecnt++;
	fileidx++;
	// hack-- force reload..
	// seems a little monstrous but it works.
	tns_free(&tns);
	tns_init(&tns, files, &filecnt, &fileidx, &win);
	tns.dirty = true;
	if (n < alternate)
		alternate++;
	for (i=0; i < filecnt; i++){
		fprintf(stderr, "%d : %s;%s;%s\n", i, newinfo[i].path, files[i].name, files[i].base);
	}
}

void set_timeout(timeout_f handler, int time, bool overwrite)
{
	int i;

	for (i = 0; i < ARRLEN(timeouts); i++) {
		if (timeouts[i].handler == handler) {
			if (!timeouts[i].active || overwrite) {
				gettimeofday(&timeouts[i].when, 0);
				TV_ADD_MSEC(&timeouts[i].when, time);
				timeouts[i].active = true;
			}
			return;
		}
	}
}

void reset_timeout(timeout_f handler)
{
	int i;

	for (i = 0; i < ARRLEN(timeouts); i++) {
		if (timeouts[i].handler == handler) {
			timeouts[i].active = false;
			return;
		}
	}
}

bool check_timeouts(struct timeval *t)
{
	int i = 0, tdiff, tmin = -1;
	struct timeval now;

	while (i < ARRLEN(timeouts)) {
		if (timeouts[i].active) {
			gettimeofday(&now, 0);
			tdiff = TV_DIFF(&timeouts[i].when, &now);
			if (tdiff <= 0) {
				timeouts[i].active = false;
				if (timeouts[i].handler != NULL)
					timeouts[i].handler();
				i = tmin = -1;
			} else if (tmin < 0 || tdiff < tmin) {
				tmin = tdiff;
			}
		}
		i++;
	}
	if (tmin > 0 && t != NULL)
		TV_SET_MSEC(t, tmin);
	return tmin > 0;
}

void open_info(void)
{
	static pid_t pid;
	int pfd[2];
	char w[12], h[12];

	if (info.f.err != 0 || info.open || win.bar.h == 0)
		return;
	if (info.fd != -1) {
		close(info.fd);
		kill(pid, SIGTERM);
		info.fd = -1;
	}
	win.bar.l.buf[0] = '\0';

	if (pipe(pfd) < 0)
		return;
	if ((pid = fork()) == 0) {
		close(pfd[0]);
		dup2(pfd[1], 1);
		snprintf(w, sizeof(w), "%d", img.w);
		snprintf(h, sizeof(h), "%d", img.h);
		execl(info.f.cmd, info.f.cmd, files[fileidx].name, w, h, NULL);
		error(EXIT_FAILURE, errno, "exec: %s", info.f.cmd);
	}
	close(pfd[1]);
	if (pid < 0) {
		close(pfd[0]);
	} else {
		fcntl(pfd[0], F_SETFL, O_NONBLOCK);
		info.fd = pfd[0];
		info.i = info.lastsep = 0;
		info.open = true;
	}
}

void read_info(void)
{
	ssize_t i, n;
	char buf[BAR_L_LEN];

	while (true) {
		n = read(info.fd, buf, sizeof(buf));
		if (n < 0 && errno == EAGAIN)
			return;
		else if (n == 0)
			goto end;
		for (i = 0; i < n; i++) {
			if (buf[i] == '\n') {
				if (info.lastsep == 0) {
					win.bar.l.buf[info.i++] = ' ';
					info.lastsep = 1;
				}
			} else {
				win.bar.l.buf[info.i++] = buf[i];
				info.lastsep = 0;
			}
			if (info.i + 1 == win.bar.l.size)
				goto end;
		}
	}
end:
	info.i -= info.lastsep;
	win.bar.l.buf[info.i] = '\0';
	win_draw(&win);
	close(info.fd);
	info.fd = -1;
	while (waitpid(-1, NULL, WNOHANG) > 0);
}

void load_image(int new)
{
	static int current;

	if (new < 0 || new >= filecnt)
		return;

	if (win.xwin != None)
		win_set_cursor(&win, CURSOR_WATCH);
	reset_timeout(slideshow);

	if (new != current)
		alternate = current;

	img_close(&img, false);
	while (!img_load(&img, &files[new])) {
		remove_file(new, false);
		if (new >= filecnt)
			new = filecnt - 1;
		else if (new > 0 && new < fileidx)
			new--;
	}
	files[new].flags &= ~FF_WARN;
	fileidx = current = new;

	info.open = false;
	open_info();

	if (img.multi.cnt > 0 && img.multi.animate)
		set_timeout(animate, img.multi.frames[img.multi.sel].delay, true);
	else
		reset_timeout(animate);
}

void bar_put(win_bar_t *bar, const char *fmt, ...)
{
	size_t len = bar->size - (bar->p - bar->buf), n;
	va_list ap;

	va_start(ap, fmt);
	n = vsnprintf(bar->p, len, fmt, ap);
	bar->p += MIN(len, n);
	va_end(ap);
}

void update_info(void)
{
	unsigned int i, fn, fw;
	char title[256];
	const char * mark;
	bool ow_info;
	win_bar_t *l = &win.bar.l, *r = &win.bar.r;

	/* update window title */
	if (mode == MODE_THUMB) {
		win_set_title(&win, "sxiv");
	} else {
		snprintf(title, sizeof(title), "sxiv - %s", basename((char *)files[fileidx].name));
		win_set_title(&win, title);
	}

	/* update bar contents */
	if (win.bar.h == 0)
		return;
	for (fw = 0, i = filecnt; i > 0; fw++, i /= 10);
	mark = files[fileidx].flags & FF_MARK ? "*" : (markcnt > 0 ? "{}" : "");
	l->p = l->buf;
	r->p = r->buf;
	if (mode == MODE_THUMB) {
		if (tns.loadnext < tns.end) {
			bar_put(l, "Loading... %0*d", fw, tns.loadnext + 1);
			ow_info = false;
		} else if (tns.initnext < filecnt) {
			bar_put(l, "Caching... %0*d", fw, tns.initnext + 1);
			ow_info = false;
		} else {
			ow_info = true;
		}
		if (strlen(mark) > 0 && mark[0] == '*'){
			bar_put(r, "%d", markcnt);
			bar_put(r, "%s %0*d/%d", mark, fw, fileidx + 1, filecnt);
		} else {
			bar_put(r, "%s %0*d/%d", mark, fw, fileidx + 1, filecnt);
		}
	} else {
		if (strlen(mark) > 0)
			bar_put(r, "%s ", mark);
		if (img.ss.on) {
			if (img.ss.delay <= 90000) {
	                        bar_put(r, "%.1fs | ", ((float)img.ss.delay) / 1000.);
	                } else if (img.ss.delay <= (90 * 60 * 1000)) {
	                        bar_put(r, "%.1fm | ", ((float)img.ss.delay) / (60. * 1000.));
	                } else {
	                        bar_put(r, "%.2fh | ", ((float)img.ss.delay) / (60. * 60 * 1000.));
	                }
                }
		if (img.gamma != 0)
			bar_put(r, "G%+d | ", img.gamma);
		if (img.win->mouse.x >= 0 && \
		    img.win->mouse.x < img.w && \
		    img.win->mouse.y >= 0 && \
		    img.win->mouse.y < img.h && \
		    img.show_mouse_pos > 0) {
			if (img.show_mouse_pos == 1){
				bar_put(r, "(%d, %d) | ", img.win->mouse.x, img.win->mouse.y);
			} else {
				bar_put(r, "(%.1f%%, %.1f%%) | ", \
					((float)img.win->mouse.x * 100) / ((float)img.w), \
					((float)img.win->mouse.y * 100) / ((float)img.h));
			}
		}
		bar_put(r, "%3d%% | ", (int) (img.zoom * 100.0));
		if (img.multi.cnt > 0) {
			for (fn = 0, i = img.multi.cnt; i > 0; fn++, i /= 10);
			bar_put(r, "%0*d/%d | ", fn, img.multi.sel + 1, img.multi.cnt);
		}
		bar_put(r, "%0*d/%d", fw, fileidx + 1, filecnt);
		ow_info = info.f.err != 0;
	}
	if (ow_info) {
		fn = strlen(files[fileidx].name);
		if (fn < l->size &&
		    win_textwidth(files[fileidx].name, fn, true) +
		    win_textwidth(r->buf, r->p - r->buf, true) < win.w)
		{
			strncpy(l->buf, files[fileidx].name, l->size);
		} else {
			strncpy(l->buf, files[fileidx].base, l->size);
		}
	}
}

void redraw(void)
{
	int t;

	if (mode == MODE_IMAGE) {
		img_render(&img);
		if (img.ss.on) {
			t = img.ss.delay;
			if (img.multi.cnt > 0 && img.multi.animate)
				t = MAX(t, img.multi.length);
			set_timeout(slideshow, t, false);
		}
	} else {
		tns_render(&tns);
	}
	update_info();
	win_draw(&win);
	reset_timeout(redraw);
	reset_cursor();
}

void reset_cursor(void)
{
	int i;
	cursor_t cursor = CURSOR_NONE;

	if (mode == MODE_IMAGE) {
		for (i = 0; i < ARRLEN(timeouts); i++) {
			if (timeouts[i].handler == reset_cursor) {
				if (timeouts[i].active)
					cursor = CURSOR_ARROW;
				break;
			}
		}
	} else {
		if (tns.loadnext < tns.end || tns.initnext < filecnt)
			cursor = CURSOR_WATCH;
		else
			cursor = CURSOR_ARROW;
	}
	win_set_cursor(&win, cursor);
}

void animate(void)
{
	if (img_frame_animate(&img)) {
		redraw();
		set_timeout(animate, img.multi.frames[img.multi.sel].delay, true);
	}
}

void slideshow(void)
{
	load_image(fileidx + 1 < filecnt ? fileidx + 1 : 0);
	redraw();
}

void clear_resize(void)
{
	resized = false;
}

void run_key_handler(const char *key, unsigned int mask)
{
	pid_t pid;
	FILE *pfs;
	bool marked = mode == MODE_THUMB && markcnt > 0;
	bool changed = false;
	int f, i, pfd[2], status;
	int fcnt = marked ? markcnt : 1;
	char kstr[32], oldbar[BAR_L_LEN];
	struct stat *oldst, st;

	if (keyhandler.f.err != 0) {
		if (!keyhandler.warned) {
			error(0, keyhandler.f.err, "%s", keyhandler.f.cmd);
			keyhandler.warned = true;
		}
		return;
	}
	if (key == NULL)
		return;

	if (pipe(pfd) < 0) {
		error(0, errno, "pipe");
		return;
	}
	if ((pfs = fdopen(pfd[1], "w")) == NULL) {
		error(0, errno, "open pipe");
		close(pfd[0]), close(pfd[1]);
		return;
	}
	oldst = emalloc(fcnt * sizeof(*oldst));

	memcpy(oldbar, win.bar.l.buf, sizeof(oldbar));
	snprintf(win.bar.l.buf, win.bar.l.size, "[KeyHandling..] %s", oldbar);
	win_draw(&win);
	win_set_cursor(&win, CURSOR_WATCH);

	snprintf(kstr, sizeof(kstr), "%s%s%s%s",
	         mask & ControlMask ? "C-" : "",
	         mask & Mod1Mask    ? "M-" : "",
	         mask & ShiftMask   ? "S-" : "", key);

	if ((pid = fork()) == 0) {
		close(pfd[1]);
		dup2(pfd[0], 0);
		execl(keyhandler.f.cmd, keyhandler.f.cmd, kstr, NULL);
		error(EXIT_FAILURE, errno, "exec: %s", keyhandler.f.cmd);
	}
	close(pfd[0]);
	if (pid < 0) {
		error(0, errno, "fork");
		fclose(pfs);
		goto end;
	}

	for (f = i = 0; f < fcnt; i++) {
		if ((marked && (files[i].flags & FF_MARK)) || (!marked && i == fileidx)) {
			stat(files[i].path, &oldst[f]);
			fprintf(pfs, "%s\n", files[i].path);
			f++;
		}
	}
	fclose(pfs);
	waitpid(pid, &status, 0);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
		error(0, 0, "%s: Exited abnormally", keyhandler.f.cmd);

	for (f = i = 0; f < fcnt; i++) {
		if ((marked && (files[i].flags & FF_MARK)) || (!marked && i == fileidx)) {
			if (stat(files[i].path, &st) != 0 ||
				  memcmp(&oldst[f].st_mtime, &st.st_mtime, sizeof(st.st_mtime)) != 0)
			{
				if (tns.thumbs != NULL) {
					tns_unload(&tns, i);
					tns.loadnext = MIN(tns.loadnext, i);
				}
				changed = true;
			}
			f++;
		}
	}
end:
	if (mode == MODE_IMAGE) {
		if (changed) {
			img_close(&img, true);
			load_image(fileidx);
		} else if (info.f.err == 0) {
			info.open = false;
			open_info();
		}
	}
	free(oldst);
	reset_cursor();
	redraw();
}

#define MODMASK(mask) ((mask) & (ShiftMask|ControlMask|Mod1Mask))

void on_keypress(XKeyEvent *kev)
{
	int i;
	unsigned int sh;
	KeySym ksym, shksym;
	char key;
	bool dirty = false;

	if (kev->state & ShiftMask) {
		kev->state &= ~ShiftMask;
		XLookupString(kev, &key, 1, &shksym, NULL);
		kev->state |= ShiftMask;
	}
	XLookupString(kev, &key, 1, &ksym, NULL);
	sh = (kev->state & ShiftMask) && ksym != shksym ? ShiftMask : 0;

	if (IsModifierKey(ksym))
		return;

	if (ksym == XK_Escape && MODMASK(kev->state) == 0){
		inputting_prefix = true;
		prefix = 0;
		return;
	}

	if ((inputting_prefix == true) && (key >= '0' && key <= '9'))
	{
		/* number prefix for commands */
		prefix = prefix * 10 + (int) (key - '0');
		return;
	} else for (i = 0; i < ARRLEN(keys); i++) {
		if (keys[i].ksym == ksym &&
		    MODMASK(keys[i].mask | sh) == MODMASK(kev->state) &&
		    keys[i].cmd >= 0 && keys[i].cmd < CMD_COUNT &&
		    (cmds[keys[i].cmd].mode < 0 || cmds[keys[i].cmd].mode == mode))
		{
			if (cmds[keys[i].cmd].func(keys[i].arg))
				dirty = true;
		}
	}

	if (i == ARRLEN(keys) && (!dirty))
		run_key_handler(XKeysymToString(ksym), kev->state & ~sh);

	if (dirty)
		redraw();

	prefix = 0;
	inputting_prefix = 0;
}

void on_buttonpress(XButtonEvent *bev)
{
	int i, sel;
	bool dirty = false;
	static Time firstclick;

	if (mode == MODE_IMAGE) {
		win_set_cursor(&win, CURSOR_ARROW);
		set_timeout(reset_cursor, TO_CURSOR_HIDE, true);

		for (i = 0; i < ARRLEN(buttons); i++) {
			if (buttons[i].button == bev->button &&
			    MODMASK(buttons[i].mask) == MODMASK(bev->state) &&
			    buttons[i].cmd >= 0 && buttons[i].cmd < CMD_COUNT &&
			    (cmds[buttons[i].cmd].mode < 0 || cmds[buttons[i].cmd].mode == mode))
			{
				if (cmds[buttons[i].cmd].func(buttons[i].arg))
					dirty = true;
			}
		}
		if (dirty)
			redraw();
	} else {
		/* thumbnail mode (hard-coded) */
		switch (bev->button) {
			case Button1:
				if ((sel = tns_translate(&tns, bev->x, bev->y)) >= 0) {
					if (sel != fileidx) {
						tns_highlight(&tns, fileidx, false);
						tns_highlight(&tns, sel, true);
						fileidx = sel;
						firstclick = bev->time;
						redraw();
					} else if (bev->time - firstclick <= TO_DOUBLE_CLICK) {
						mode = MODE_IMAGE;
						set_timeout(reset_cursor, TO_CURSOR_HIDE, true);
						load_image(fileidx);
						redraw();
					} else {
						firstclick = bev->time;
					}
				}
				break;
			case Button2:
			case Button3:
				if ((sel = tns_translate(&tns, bev->x, bev->y)) >= 0) {
					files[sel].flags ^= FF_MARK;
					markcnt += files[sel].flags & FF_MARK ? 1 : -1;
					tns_mark(&tns, sel, !!(files[sel].flags & FF_MARK));
					redraw();
				}
				break;
			case Button4:
			case Button5:
				if (tns_scroll(&tns, bev->button == Button4 ? DIR_UP : DIR_DOWN,
				               (bev->state & ControlMask) != 0))
					redraw();
				break;
		}
	}
	prefix = 0;
}

void run(void)
{
	int xfd;
	fd_set fds;
	struct timeval timeout;
	bool discard, init_thumb, load_thumb, to_set;
	XEvent ev, nextev;

	while (true) {
		to_set = check_timeouts(&timeout);
		init_thumb = mode == MODE_THUMB && tns.initnext < filecnt;
		load_thumb = mode == MODE_THUMB && tns.loadnext < tns.end;

		if ((init_thumb || load_thumb || to_set || info.fd != -1) &&
		    XPending(win.env.dpy) == 0)
		{
			if (load_thumb) {
				set_timeout(redraw, TO_REDRAW_THUMBS, false);
				if (!tns_load(&tns, tns.loadnext, false, false)) {
					remove_file(tns.loadnext, false);
					tns.dirty = true;
				}
				if (tns.loadnext >= tns.end)
					redraw();
			} else if (init_thumb) {
				set_timeout(redraw, TO_REDRAW_THUMBS, false);
				if (!tns_load(&tns, tns.initnext, false, true))
					remove_file(tns.initnext, false);
			} else {
				xfd = ConnectionNumber(win.env.dpy);
				FD_ZERO(&fds);
				FD_SET(xfd, &fds);
				if (info.fd != -1) {
					FD_SET(info.fd, &fds);
					xfd = MAX(xfd, info.fd);
				}
				select(xfd + 1, &fds, 0, 0, to_set ? &timeout : NULL);
				if (info.fd != -1 && FD_ISSET(info.fd, &fds))
					read_info();
			}
			continue;
		}

		do {
			XNextEvent(win.env.dpy, &ev);
			discard = false;
			if (XEventsQueued(win.env.dpy, QueuedAlready) > 0) {
				XPeekEvent(win.env.dpy, &nextev);
				switch (ev.type) {
					case ConfigureNotify:
						discard = ev.type == nextev.type;
						break;
					case KeyPress:
						discard = (nextev.type == KeyPress || nextev.type == KeyRelease)
						          && ev.xkey.keycode == nextev.xkey.keycode;
						break;
				}
			}
		} while (discard);

		switch (ev.type) {
			/* handle events */
			case ButtonPress:
				on_buttonpress(&ev.xbutton);
				break;
			case ClientMessage:
				if ((Atom) ev.xclient.data.l[0] == atoms[ATOM_WM_DELETE_WINDOW])
					return;
				break;
			case ConfigureNotify:
				if (win_configure(&win, &ev.xconfigure)) {
					if (mode == MODE_IMAGE) {
						img.dirty = true;
						img.checkpan = true;
					} else {
						tns.dirty = true;
					}
					if (!resized || win.fullscreen) {
						redraw();
						set_timeout(clear_resize, TO_REDRAW_RESIZE, false);
						resized = true;
					} else {
						set_timeout(redraw, TO_REDRAW_RESIZE, false);
					}
				}
				break;
			case KeyPress:
				on_keypress(&ev.xkey);
				break;
			case MotionNotify:
				if (mode == MODE_IMAGE) {
					img.win->mouse.x=(int)((ev.xmotion.x-img.x)/img.zoom);
					img.win->mouse.y=(int)((ev.xmotion.y-img.y)/img.zoom);
					update_info();
					win_draw_bar(&win);
					XClearWindow(win.env.dpy, win.xwin);
					win_set_cursor(&win, CURSOR_ARROW);
					set_timeout(reset_cursor, TO_CURSOR_HIDE, true);
				}
				break;
		}
	}
}

int fncmp(const void *a, const void *b)
{
	return strcoll(((fileinfo_t*) a)->name, ((fileinfo_t*) b)->name);
}

int main(int argc, char **argv)
{
	int i, start;
	size_t n;
	ssize_t len;
	char *filename;
	const char *homedir, *dsuffix = "";
	struct stat fstats;
	r_dir_t dir;

	signal(SIGPIPE, SIG_IGN);

	parse_options(argc, argv);

	if (options->clean_cache) {
		tns_init(&tns, NULL, NULL, NULL, NULL);
		tns_clean_cache(&tns);
		exit(EXIT_SUCCESS);
	}

	if (options->filecnt == 0 && !options->from_stdin) {
		print_usage();
		exit(EXIT_FAILURE);
	}

	if (options->recursive || options->from_stdin)
		filecnt = 1024;
	else
		filecnt = options->filecnt;

	files = emalloc(filecnt * sizeof(*files));
	memset(files, 0, filecnt * sizeof(*files));
	fileidx = 0;

	if (options->from_stdin) {
		n = 0;
		filename = NULL;
		while ((len = getline(&filename, &n, stdin)) > 0) {
			if (filename[len-1] == '\n')
				filename[len-1] = '\0';
			check_add_file(filename, true);
		}
		free(filename);
	}

	for (i = 0; i < options->filecnt; i++) {
		filename = options->filenames[i];

		if (stat(filename, &fstats) < 0) {
			error(0, errno, "%s", filename);
			continue;
		}
		if (!S_ISDIR(fstats.st_mode)) {
			check_add_file(filename, true);
		} else {
			if (!options->recursive) {
				error(0, 0, "%s: Is a directory", filename);
				continue;
			}
			if (r_opendir(&dir, filename) < 0) {
				error(0, errno, "%s", filename);
				continue;
			}
			start = fileidx;
			while ((filename = r_readdir(&dir)) != NULL) {
				check_add_file(filename, false);
				free((void*) filename);
			}
			r_closedir(&dir);
			if (fileidx - start > 1)
				qsort(files + start, fileidx - start, sizeof(fileinfo_t), fncmp);
		}
	}

	if (fileidx == 0)
		error(EXIT_FAILURE, 0, "No valid image file given, aborting");

	filecnt = fileidx;
	fileidx = options->startnum < filecnt ? options->startnum : 0;

	win_init(&win);
	img_init(&img, &win);

	if ((homedir = getenv("XDG_CONFIG_HOME")) == NULL || homedir[0] == '\0') {
		homedir = getenv("HOME");
		dsuffix = "/.config";
	}
	if (homedir != NULL) {
		extcmd_t *cmd[] = { &info.f, &keyhandler.f };
		const char *name[] = { "image-info", "key-handler" };

		for (i = 0; i < ARRLEN(cmd); i++) {
			n = strlen(homedir) + strlen(dsuffix) + strlen(name[i]) + 12;
			cmd[i]->cmd = (char*) emalloc(n);
			snprintf(cmd[i]->cmd, n, "%s%s/sxiv/exec/%s", homedir, dsuffix, name[i]);
			if (access(cmd[i]->cmd, X_OK) != 0)
				cmd[i]->err = errno;
		}
	} else {
		error(0, 0, "Exec directory not found");
	}
	info.fd = -1;

	if (options->thumb_mode) {
		mode = MODE_THUMB;
		tns_init(&tns, files, &filecnt, &fileidx, &win);
		while (!tns_load(&tns, fileidx, false, false))
			remove_file(fileidx, false);
	} else {
		mode = MODE_IMAGE;
		tns.thumbs = NULL;
		load_image(fileidx);
	}
	win_open(&win);
	win_set_cursor(&win, CURSOR_WATCH);

	atexit(cleanup);

	set_timeout(redraw, 25, false);

	run();

	return 0;
}
