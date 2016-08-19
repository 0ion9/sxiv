![sxiv](http://muennich.github.com/sxiv/img/logo.png "sxiv")

**Simple X Image Viewer**

The primary goal of sxiv is to create an image viewer, which only has the most
basic features required for fast image viewing (the ones I want). It has vi key
bindings and works nicely with tiling window managers.  Its code base should be
kept small and clean to make it easy for you to dig into it and customize it
for your needs.


Features
--------

* Basic image operations, e.g. zooming, panning, rotating
* Customizable key and mouse button mappings (in *config.h*)
* Thumbnail mode: grid of selectable previews of all images
* Ability to cache thumbnails for fast re-loading
* Basic support for multi-frame images
* Load all frames from GIF files and play GIF animations
* Display image information in status bar


0ion9 Fork notes
-----------------

This version includes a number of enhancements vs vanilla SXIV:

 * **Do not use a prefix-key to execute key handler**. Any keys you want the keyhandler to receive can just be typed directly (providing they are not already consumed by sxiv, like Space)
 * **Change numeric prefixes to be escaped** -- for example, in vanilla SXIV where you could type 30<Space>, in this version you type <Escape>30<Space>.
   This frees up the keys 0-9 for keyhandler use.
 * Keep content of info-bar visible while invoking keyhandler
 * Indicate whether this file is marked, and whether any files are marked, in the info bar
 * In image mode, show only basename of file, not entire path.
 * **'Stronger' thumbnail zooming ('auto zoom to fit' -- thumbnails are scaled up by an integer factor if there is space available)**. Very useful for pixel art and icons.
 * **'Auto' setting for AA** -- equivalent to 'yes, if zooming out, no, if zooming in'. Usually for me this means I never need to touch the AA setting.
   The threshold at which it switches over is configurable in config.h
 * **Commands to move one file, or all marked files, an offset through the filelist (including to end or start)**. Useful for grouping files.
 * **Commands to spatially reorder the filelist** -- ie. swap a file with its up/down/left/right neighbour. Can function as a rough collaging system.
 * **Command to clone a file** -- ie. add another instance of it to the filelist.
 * **Ensure 'zoom to fit' works properly for really huge images**.
 * **The ability to display images as silhouettes, with inverted alpha, and with reduced alpha.**
 * **Tiled display toggle** (repeats image over window surface)

However, note that there is currently an outstanding bug where, if you resize the window smaller than a thumb high/wide, sxiv terminates (and your WM may get confused).
A temporary workaround is to only resize the window in image mode.


Screenshots
-----------

**Image mode:**

![Image](http://muennich.github.com/sxiv/img/image.png "Image mode")

**Thumbnail mode:**

![Thumb](http://muennich.github.com/sxiv/img/thumb.png "Thumb mode")


Installation
------------

sxiv is built using the commands:

    $ make
    # make install

Please note, that the latter one requires root privileges.
By default, sxiv is installed using the prefix "/usr/local", so the full path
of the executable will be "/usr/local/bin/sxiv".

You can install sxiv into a directory of your choice by changing the second
command to:

    # make PREFIX="/your/dir" install

The build-time specific settings of sxiv can be found in the file *config.h*.
Please check and change them, so that they fit your needs.
If the file *config.h* does not already exist, then you have to create it with
the following command:

    $ make config.h


Usage
-----

Please see the [man page](http://muennich.github.com/sxiv/sxiv.1.html) for information on how to use sxiv.

Download & Changelog
--------------------

You can [browse](https://github.com/muennich/sxiv) the source code repository
on GitHub or get a copy using git with the following command:

    git clone https://github.com/muennich/sxiv.git

**Stable releases**

**[v1.3.2](https://github.com/muennich/sxiv/archive/v1.3.2.tar.gz)**
*(December 20, 2015)*

  * external key handler gets file paths on stdin, not as arguments
  * Cache out-of-view thumbnails in the background
  * Apply gamma correction to thumbnails

**[v1.3.1](https://github.com/muennich/sxiv/archive/v1.3.1.tar.gz)**
*(November 16, 2014)*

  * Fixed build error, caused by delayed config.h creation
  * Fixed segfault when run with -c

**[v1.3](https://github.com/muennich/sxiv/archive/v1.3.tar.gz)**
*(October 24, 2014)*

  * Extract thumbnails from EXIF tags (requires libexif)
  * Zoomable thumbnails, supported sizes defined in config.h
  * Fixed build error with giflib version >= 5.1.0

**[v1.2](https://github.com/muennich/sxiv/archive/v1.2.tar.gz)**
*(April 24, 2014)*

  * Added external key handler, called on keys prefixed with `Ctrl-x`
  * New keybinding `{`/`}` to change gamma (by András Mohari)
  * Support for slideshows, enabled with `-S` option & toggled with `s`
  * Added application icon (created by 0ion9)
  * Checkerboard background for alpha layer
  * Option `-o` only prints files marked with `m` key
  * Fixed rotation/flipping of multi-frame images (gifs)

**[v1.1.1](https://github.com/muennich/sxiv/archive/v1.1.1.tar.gz)**
*(June 2, 2013)*

  * Various bug fixes

**[v1.1](https://github.com/muennich/sxiv/archive/v1.1.tar.gz)**
*(March 30, 2013)*

  * Added status bar on bottom of window with customizable content
  * New keyboard shortcuts `\`/`|`: flip image vertically/horizontally
  * New keyboard shortcut `Ctrl-6`: go to last/alternate image
  * Added own EXIF orientation handling, removed dependency on libexif
  * Fixed various bugs

**[v1.0](https://github.com/muennich/sxiv/archive/v1.0.tar.gz)**
*(October 31, 2011)*

  * Support for multi-frame images & GIF animations
  * POSIX compliant (IEEE Std 1003.1-2001)

**[v0.9](https://github.com/muennich/sxiv/archive/v0.9.tar.gz)**
*(August 17, 2011)*

  * Made key and mouse mappings fully configurable in config.h
  * Complete code refactoring

**[v0.8.2](https://github.com/muennich/sxiv/archive/v0.8.2.tar.gz)**
*(June 29, 2011)*

  * POSIX-compliant Makefile; compiles under NetBSD

**[v0.8.1](https://github.com/muennich/sxiv/archive/v0.8.1.tar.gz)**
*(May 8, 2011)*

  * Fixed fullscreen under window managers, which are not fully EWMH-compliant

**[v0.8](https://github.com/muennich/sxiv/archive/v0.8.tar.gz)**
*(April 18, 2011)*

  * Support for thumbnail caching
  * Ability to run external commands (e.g. jpegtran, convert) on current image

**[v0.7](https://github.com/muennich/sxiv/archive/v0.7.tar.gz)**
*(February 26, 2011)*

  * Sort directory entries when using `-r` command line option
  * Hide cursor in image mode
  * Full functional thumbnail mode, use Return key to switch between image and
    thumbnail mode

**[v0.6](https://github.com/muennich/sxiv/archive/v0.6.tar.gz)**
*(February 16, 2011)*

  * Bug fix: Correctly display filenames with umlauts in window title
  * Basic support of thumbnails

**[v0.5](https://github.com/muennich/sxiv/archive/v0.5.tar.gz)**
*(February 6, 2011)*

  * New command line option: `-r`: open all images in given directories
  * New key shortcuts: `w`: resize image to fit into window; `W`: resize window
    to fit to image

**[v0.4](https://github.com/muennich/sxiv/archive/v0.4.tar.gz)**
*(February 1, 2011)*

  * New command line option: `-F`, `-g`: use fixed window dimensions and apply
    a given window geometry
  * New key shortcut: `r`: reload current image

**[v0.3.1](https://github.com/muennich/sxiv/archive/v0.3.1.tar.gz)**
*(January 30, 2011)*

  * Bug fix: Do not set setuid bit on executable when using `make install`
  * Pan image with mouse while pressing middle mouse button

**[v0.3](https://github.com/muennich/sxiv/archive/v0.3.tar.gz)**
*(January 29, 2011)*

  * New command line options: `-d`, `-f`, `-p`, `-s`, `-v`, `-w`, `-Z`, `-z`
  * More mouse mappings: Go to next/previous image with left/right click,
    scroll image with mouse wheel (horizontally if Shift key is pressed),
    zoom image with mouse wheel if Ctrl key is pressed

**[v0.2](https://github.com/muennich/sxiv/archive/v0.2.tar.gz)**
*(January 23, 2011)*

  * Bug fix: Handle window resizes correctly
  * New keyboard shortcuts: `g`/`G`: go to first/last image; `[`/`]`: go 10
    images back/forward
  * Support for mouse wheel zooming (by Dave Reisner)
  * Added fullscreen mode

**[v0.1](https://github.com/muennich/sxiv/archive/v0.1.tar.gz)**
*(January 21, 2011)*

  * Initial release

