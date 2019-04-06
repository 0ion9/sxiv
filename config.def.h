#define PIXELIZE_AT 199.4999

#ifdef _WINDOW_CONFIG

/* default window dimensions (overwritten via -g option): */
enum {
	WIN_WIDTH  = 800,
	WIN_HEIGHT = 600
};

/* bar font:
 * (see fonts-conf(5) subsection "FONT NAMES" for valid values)
 */
static const char * const BAR_FONT = "unifont:size=10";

/* colors:
 * overwritten by 'background', 'foreground', 'selection' X resource properties.
 * (see X(7) section "COLOR NAMES" for valid values)
 */
static const char * const BG_COLOR = "#808080";
static const char * const FG_COLOR = "#050300";
static const char * const SEL_COLOR = "#66f0ff";
static const char * const BAR_COLOR = "#707070";

#endif
#ifdef _IMAGE_CONFIG

/* levels (in percent) to use when zooming via '-' and '+':
 * (first/last value is used as min/max zoom level)
 */
static const float zoom_levels[] = {
	5.0, 6.125, 9.1875,
	12.5,  25.0,  50.0,  75.0,
	100.0, 200.0, 300.0, 400.0, 800.0,
	1600.0, 2400.0, 16000.0
};

/* default slideshow delay (in 10th of a second, overwritten via -S option): */
/* this is 90 seconds / 1m30s */
enum { SLIDESHOW_DELAY = 900 };

/* default settings for multi-frame gif images: */
/* Not used any more. REMOVE after checking the Autoplay / Loop behaviour with:
   * [ ] unlooping gif
   * [ ] looping gif
   
*/

//enum {
//	GIF_DELAY    = 100, /* delay time (in ms) */
//	GIF_AUTOPLAY = 1,   /* autoplay when loaded [0/1] */
//	GIF_LOOP     = -1    /* loop? [0: no, 1: endless, -1: as specified in file] */
//};

/* gamma correction: the user-visible ranges [-GAMMA_RANGE, 0] and
 * (0, GAMMA_RANGE] are mapped to the ranges [0, 1], and (1, GAMMA_MAX].
 * */
static const double GAMMA_MAX   = 10.0;
static const int    GAMMA_RANGE = 32;

/* command i_scroll pans image 1/PAN_FRACTION of screen width/height */
static const int PAN_FRACTION = 5;

/* if 0, pixelate images at zoom level != 100%,
 * if 1, antialias images at zoom level != 100%,
 * if 2, pixelate when zoom level >= IMAGE_PIXELIZE_AT ,
 *       antialias otherwise.
 * cycled with 'a' key binding. */
static const int ANTI_ALIAS = 2;

/* when antialias = 2, pixelize if zoom factor > this value.
 */
static const float IMAGE_PIXELIZE_AT = (PIXELIZE_AT / 100.0);

/* if true, use a checkerboard background for alpha layer,
 * toggled with 'A' key binding
 */
static const bool ALPHA_LAYER = false;

/* Silhouette rendering colors */
static int SILHOUETTE_COLOR[6][3] = {
   {0, 0, 0},
   {192, 192, 192},
   {255, 255, 255},
   {150, 150, 204},
   {150, 204, 150},
   {204, 150, 150}
};

#endif
#ifdef _THUMBS_CONFIG

/* thumbnail sizes in pixels (width == height): */
static const int thumb_sizes[] = { 32, 64, 80, 144, 192, 384, 480, 576, 768};

/* zoom levels usable when zooming >= max thumbnail size
   leave 100.0 as-is, unless you want to alter the base zoom of thumbnails when <= max thumbnail size.

   Note that unlike zoom_levels, values are -integers-.
*/
static const int thumbnail_zoom_levels[] = { 100, 200, 250, 300, 400 };

/* pixelize if thumbnail zoom factor > this value.
   values <= 100 have no effect.
 */
static const int THUMBNAIL_PIXELIZE_AT = PIXELIZE_AT;

/* thumbnail size at startup, index into thumb_sizes[]: */
static const int THUMB_SIZE = 3;

#endif
#ifdef _MAPPINGS_CONFIG

/* alias for clarity */
#define AltMask Mod1Mask

/* keyboard mappings for image and thumbnail mode: */
/* Plain Alphanumerics, shifted alphanumerics, control+alphanumerics and function keys are explicitly avoided,
   to allow the key handler script maximum latitude. */

/* keyboard layout diagram:

   ^^ f1 f2 f3 f4  f5 f6 f7 f8  f9 fA fB fC  Ps  Sl  Pa

   ~ 1  2  3  4  5  6  7  8  9  0  -  =  <<  In  Ho  Pu  Nl /  *  -
   
   Ta q  w  f  p  g  j  l  u  y  ;  [  ]  \  De  En  Pd  7H 8^ 9U +
                                                                  +
   <<  a  r  s  t  d  h  n  e  i  o  ' Entr              4< 5  6> +

   Shft z  x  c  v  b  k  m  ,  .  /  Shift      ^       1E 2v 3D E
                                                                  n
   Ctl Hy Al    S p a c e     Al Hy Me  Ctl  <   v   >   0Ins  .D t



 */

static const keymap_t keys[] = {
	/* modifiers    key               function              argument */
        { ShiftMask,    XK_Escape,        g_quit,               None },
        { ShiftMask,    XF86XK_Close,     g_quit,               None },


        // 1 2 3 4 5 6 7 8 9 0 -> internal 'score' cmd, using prefix?
        // minus
        // dash
	{ 0,            XK_minus,         i_flip,               FLIP_VERTICAL },
	{ AltMask,      XK_minus,         i_fit_to_win,         SCALE_DOWN },
	{ 0,            XK_emdash,        i_flip,               FLIP_VERTICAL },
	{ ControlMask,  XK_plus,          i_set_zoom,           100 },
	{ AltMask,      XK_equal,         g_toggle_synczoom,    None },
	{ 0,            XK_underscore,    i_fit_to_win,         SCALE_FIT },
	{ ControlMask,  XK_minus,         i_fit_to_win,         SCALE_WIDTH },
	{ ControlMask,  XK_underscore,    i_fit_to_win,         SCALE_HEIGHT },
	{ AltMask,      XK_BackSpace,     i_navigate,           -1 },
        // Ins
        // Home
        // PgUp
        // NumLock
        // Kp_Div
        // KP_Mul
	{ 0,            XK_KP_Subtract,   g_zoom,               -1 },
	{ 0,            XK_identical,     g_zoom,               -1 },


        { 0,            XK_Tab,           g_switch_mode,        None },
        // XXX s-Tab -> fullscreen, c-Tab -> altimage
        // Q
	{ 0          ,  XK_W,             i_cycle_tiling,       None },
        { ControlMask,  XK_F,             g_toggle_fullscreen,  None },
	{ 0,            XK_P,             i_print_mouse_pos,    None },
	{ ControlMask,  XK_P,             i_toggle_mouse_pos,   None },
	{ AltMask,      XK_g,             g_n_or_last,          None },
        // j
        // l
        // u
        // semicolon
	{ 0,            XK_bracketleft,   i_navigate,           -10 },
        { 0,            XK_bracketright,  i_navigate,           +10 },
	{ AltMask,      XK_bracketleft,   i_navigate,           -100 },
        { AltMask,      XK_bracketright,  i_navigate,           +100 },
        { 0,            XK_bracketleft,   g_scroll_screen,      DIR_UP },
        { 0,            XK_bracketright,  g_scroll_screen,      DIR_DOWN },
	{ 0,            XK_leftt,         i_navigate,           -10 },
	{ 0,            XK_rightt,        i_navigate,           +10 },
	{ 0,            XK_braceleft,     g_change_gamma,       -1 },
	{ 0,            XK_braceright,    g_change_gamma,       +1 },
	{ 0,            XK_backslash,     i_flip,               FLIP_HORIZONTAL },
	{ 0,            XK_exclamdown,    i_flip,               FLIP_HORIZONTAL },
	{ 0,            XK_Delete,        g_remove_image,      None },
	{ ControlMask,  XK_Delete,        g_remove_marked,      None },
        // End
        // PgDn
        // KP_AltMask
        // KP_Up
	{ 0,            XK_KP_Prior,      i_navigate,           +10 },
        // XXX thumb mode navigate in pages?
        { 0,            XK_KP_Add,        g_zoom,               +1 },
	{ 0,            XK_plusminus,     g_zoom,               +1 },

	{ 0,            XK_a,             g_zoom,               +2 },
	{ ControlMask,  XK_A,             i_cycle_antialias,    None },
        // XXX with inotify, reloadall may not be needed.
	{ ControlMask,  XK_R,             t_reload_all,         None },
	{ 0,            XK_s,             g_navigate_marked,    -1 },
//	{ 0,            XK_s,             i_navigate,           -10 },
//	{ 0,            XK_s,             g_scroll_screen,      DIR_UP },
	{ 0          ,  XK_S,             g_cycle_silhouetting, None },
	{ ControlMask,  XK_S,             i_slideshow,          None },
        // xxx fallback doesn't always occur correctly.
	{ 0,            XK_t,             g_navigate_marked,    +1 },
//	{ 0,            XK_t,             i_navigate,           +10 },
//	{ 0,            XK_t,             g_scroll_screen,      DIR_DOWN },
	{ ControlMask,  XK_t,             g_toggle_negalpha,    None },
	{ 0          ,  XK_T,             g_cycle_opacity,      0 },
	{ ControlMask,  XK_T,             i_toggle_alpha,       None },
	{ ControlMask,  XK_D,             i_fit_to_win,         SCALE_DISTORT },
        // h
	{ 0,            XK_n,             i_navigate,           -1 },
	{ 0,            XK_e,             i_navigate,           +1 },
	{ AltMask,      XK_e,             i_alternate,          None },
        // i
        // o
        // '
        { 0,            XK_Return,        g_switch_mode,        None },
	{ 0,            XK_KP_Left,       i_navigate,           -1 },
	{ 0,            XK_KP_4,          g_first,              None },
	{ 0,            XK_KP_Begin,      g_n_or_last,          None },
	{ 0,            XK_KP_Right,      i_navigate,           +1 },
	{ 0,            XK_z,             g_zoom,               -2 },
	{ 0,            XK_x,             i_navigate_frame,     -1 },
        { 0,               0,             0,                    0 },
	{ 0,            XK_x,             i_navigate,           -1 },
	{ 0,            XK_x,             t_move_sel,           DIR_LEFT },
	{ 0,            XK_X,             i_cycle_scalefactor,  None },
	{ 0,            XK_c,             i_navigate_frame,     +1 },
        { 0,               0,             0,                    0 },
	{ 0,            XK_c,             i_navigate,           +1 },
	{ 0,            XK_c,             t_move_sel,           DIR_RIGHT },
	{ 0,            XK_C,             g_clone_image,        1 },
        { 0,            XK_B,             g_toggle_bar,         None },
        // k
	{ 0,            XK_m,             g_toggle_image_mark,  None },
	{ 0,            XK_BackSpace,     g_toggle_image_mark,  None },
	{ 0,            XK_BackSpace,     i_navigate,           +1 },
	{ 0,            XK_BackSpace,     t_move_sel,           DIR_RIGHT },
	{ AltMask,      XK_m,             g_mark_range,         None },
	{ 0,            XK_m,             i_navigate,           +1 },
	{ 0,            XK_m,             t_move_sel,           DIR_RIGHT },
/* use C-Menu to do this instead, it's much easier */
//	{ ControlMask,  XK_m,             g_reverse_marks,      None },
	{ 0,            XK_M,             g_unmark_all,         None },
        // ,
	{ 0,            XK_comma,         i_rotate,             DEGREE_270 },
	{ 0,            XK_period,        i_rotate,             DEGREE_90 },
	{ 0,            XK_greater,       i_rotate,             DEGREE_180 },
	{ 0,            XK_kana_comma,    i_rotate,             DEGREE_270 },
	{ 0,            XK_kana_fullstop, i_rotate,             DEGREE_90 },
	{ 0,            XK_emopencircle,  i_rotate,             DEGREE_180 },
        // KP_End?
        // KP_PgDn?
	{ 0          ,  XK_KP_Down,       i_toggle_animation,   None },
	{ 0          ,  XK_KP_Enter,      i_toggle_animation,   None },
	{ ControlMask,  XK_Return,        i_toggle_animation,   None },


        { 0,            XK_space,         i_navigate,           +1 },
        { 0,            XK_space,         t_move_sel,           DIR_DOWN },
        { ShiftMask,    XK_space,         t_move_sel,           DIR_RIGHT },
	{ ControlMask,  XK_space,         i_toggle_animation,   None },
	{ 0,            XK_Menu,          g_toggle_image_mark, None },
	{ 0,            XK_Menu,          i_navigate,           +1 },
	{ 0,            XK_Menu,          t_move_sel,           DIR_RIGHT },
	{ ControlMask,  XK_Menu,          g_reverse_marks,     None },
	{ ShiftMask,    XK_Menu,          g_unmark_all,     None },
	{ 0,            XK_Left,          i_scroll,             DIR_LEFT },
	{ 0,            XK_Left,          t_move_sel,          DIR_LEFT },
	{ ControlMask,  XK_Left,          g_scroll_screen,     DIR_LEFT },
	{ 0,            XK_Pointer_Left,  t_move_and_reorder,   DIR_LEFT },
	{ AltMask,      XK_Left,          t_move_and_reorder,   DIR_LEFT },
	{ AltMask,      XK_Left,          g_reorder_image,      -9999999 },
	{ ShiftMask,    XK_Left,          g_reorder_image,      -16 },
	{ ShiftMask,    XK_Left,          i_scroll_to_edge,     DIR_LEFT },
	{ AltMask|ControlMask, XK_Left,   g_reorder_marked_images,  -1 },
	{ AltMask|ShiftMask,  XK_Left,    g_reorder_image_to_alternate,      -1 },
	{ ShiftMask|ControlMask, XK_Left, g_reorder_marked_images_to_current,  -1 },
	{ ControlMask,  XK_X,             g_reorder_marked_images_to_current,  -1 },
	{ 0,            XK_Down,          i_scroll,             DIR_DOWN },
	{ 0,            XK_Up,            i_scroll,             DIR_UP },
	{ 0,            XK_Pointer_Up,    t_move_and_reorder,   DIR_UP },
	{ AltMask,      XK_Up,            t_move_and_reorder,   DIR_UP },
	{ 0,            XK_Right,         i_scroll,             DIR_RIGHT },
	{ 0,            XK_Right,         t_move_sel,          DIR_RIGHT },
	{ ControlMask,  XK_Right,         g_scroll_screen,     DIR_RIGHT },
	{ ShiftMask,    XK_Right,         i_scroll_to_edge,     DIR_RIGHT },
	{ 0,            XK_Pointer_Right, t_move_and_reorder,   DIR_RIGHT },
	{ AltMask,      XK_Right,         t_move_and_reorder,   DIR_RIGHT },
	{ AltMask,      XK_Right,         g_reorder_image,      9999999 },
	{ ShiftMask,    XK_Right,         g_reorder_image,      16 },
/* XXX instead of abusing 'alternate', implement a separate 'target marker' */
	{ AltMask|ShiftMask,    XK_Right, g_reorder_image_to_alternate,        1 },
	{ AltMask|ControlMask,  XK_Right, g_reorder_marked_images,             1 },
	// xxx absurd. get a less modifiery shortcut for this
	{ ShiftMask|ControlMask, XK_Right, g_reorder_marked_images_to_current, 1 },
	{ ShiftMask,                XK_C, g_reorder_marked_images_to_current,  1 },

	{ 0,            XK_Down,          t_move_sel,          DIR_DOWN },
	{ ControlMask,  XK_Down,          g_scroll_screen,     DIR_DOWN },
	{ ShiftMask,    XK_Down,          i_scroll_to_edge,     DIR_DOWN },
	{ 0,            XK_downarrow,     t_move_sel,          DIR_DOWN },
	{ ControlMask,  XK_downarrow,     g_scroll_screen,     DIR_DOWN },
	{ ShiftMask,    XK_downarrow,     i_scroll_to_edge,     DIR_DOWN },
	{ 0,            XK_Up,            t_move_sel,          DIR_UP },
	{ ControlMask,  XK_Up,            g_scroll_screen,     DIR_UP },
	{ ShiftMask,    XK_Up,            i_scroll_to_edge,     DIR_UP },
	{ 0,            XK_uparrow,       t_move_sel,          DIR_UP },
	{ ControlMask,  XK_uparrow,       g_scroll_screen,     DIR_UP },
	{ ShiftMask,    XK_uparrow,       i_scroll_to_edge,     DIR_UP },

	{ 0,            XK_leftarrow,     i_scroll,             DIR_LEFT },
	{ 0,            XK_leftarrow,     t_move_sel,          DIR_LEFT },
	{ ControlMask,  XK_leftarrow,     g_scroll_screen,     DIR_LEFT },
	{ ShiftMask,    XK_leftarrow,     i_scroll_to_edge,     DIR_LEFT },
	{ 0,            XK_downarrow,     i_scroll,             DIR_DOWN },
	{ 0,            XK_uparrow,       i_scroll,             DIR_UP },
	{ 0,            XK_rightarrow,    i_scroll,             DIR_RIGHT },
	{ ControlMask,  XK_rightarrow,    g_scroll_screen,     DIR_RIGHT },
	{ ShiftMask,    XK_rightarrow,    i_scroll_to_edge,     DIR_RIGHT },
	{ 0,            XK_rightarrow,    t_move_sel,          DIR_RIGHT },
	{ 0,            XK_Pointer_Down,  t_move_and_reorder,   DIR_DOWN },
	{ AltMask,      XK_Down,          t_move_and_reorder,   DIR_DOWN },

	{ 0,            XK_KP_Insert,     g_navigate_marked,   -1 },
	{ 0,            XK_KP_Delete,     g_navigate_marked,   +1 },
	{ 0,    XK_KP_Decimal,     g_navigate_marked,   -10 },
	{ 0,    XK_KP_0,     g_navigate_marked,   +10 },
	{ AltMask,      XK_KP_Insert,     g_navigate_marked,   -1000 },
	{ AltMask,      XK_KP_Delete,     g_navigate_marked,   +1000 },

//////
	{ 0,            XK_KP_Next,       i_navigate,           -10 }

// marking an image should also move to the next image -- see https://github.com/muennich/sxiv/issues/172
// Note the order of entries here is significant -- we don't want to navigate to next before the toggle is processed.

};

/* mouse button mappings for image mode: */
static const button_t buttons[] = {
	/* modifiers    button            function              argument */
	{ 0,            1,                i_cursor_navigate,    None },
	{ 0,            2,                i_drag,               DRAG_ABSOLUTE },
	{ 0,            3,                g_switch_mode,        None },
	{ 0,            4,                g_zoom,               +1 },
	{ 0,            5,                g_zoom,               -1 },
};

#endif
