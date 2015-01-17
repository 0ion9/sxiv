#ifdef _WINDOW_CONFIG

/* default window dimensions (overwritten via -g option): */
enum {
	WIN_WIDTH  = 800,
	WIN_HEIGHT = 600
};

/* bar font:
 * (see X(7) section "FONT NAMES" for valid values)
 */
static const char * const BAR_FONT = // "-misc-fixed-*-*-*-*-12-*-*-*-*-*-*-*";
"-*-proggytiny-*-*-*-*-10-*-*-*-*-*-*-*";

/* colors:
 * (see X(7) section "COLOR NAMES" for valid values)
 */
static const char * const WIN_BG_COLOR = "#555555";
static const char * const WIN_FS_COLOR = "#000000";
static const char * const SEL_COLOR    = "#EEEEEE";
static const char * const BAR_BG_COLOR = "#222222";
static const char * const BAR_FG_COLOR = "#EEEEEE";

#endif
#ifdef _IMAGE_CONFIG

/* levels (in percent) to use when zooming via '-' and '+':
 * (first/last value is used as min/max zoom level)
 */
static const float zoom_levels[] = {
	 12.5,  25.0,  50.0,  75.0,
	100.0, 150.0, 200.0, 400.0, 800.0,
	1600.0, 2400.0
};

/* default slideshow delay (in msec, overwritten via -S option): */
/* this is 90 seconds / 1m30s */
enum { SLIDESHOW_DELAY = 900 };

/* default settings for multi-frame gif images: */
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

/* if false, pixelate images at zoom level != 100%,
 * toggled with 'a' key binding
 */
static const bool ANTI_ALIAS = false;

/* if true, use a checkerboard background for alpha layer,
 * toggled with 'A' key binding
 */
static const bool ALPHA_LAYER = false;

#endif
#ifdef _THUMBS_CONFIG

/* thumbnail sizes in pixels (width == height): */
static const int thumb_sizes[] = { 32, 64, 80, 144, 192 };

/* zoom levels usable when zooming >= max thumbnail size
   leave 100.0 as-is, unless you want to alter the base zoom of thumbnails when <= max thumbnail size.

   Note that unlike zoom_levels, values are -integers-.
*/
static const int thumbnail_zoom_levels[] = { 100, 200, 250, 300, 400 };

/* pixelize if thumbnail zoom factor > this value.
   values <= 100 have no effect.
 */
static const int THUMBNAIL_PIXELIZE_AT = 201;

#endif
#ifdef _MAPPINGS_CONFIG

/* keyboard mappings for image and thumbnail mode: */
/* Still needs simplification. "There should be one and preferably only one way to do it." */
static const keymap_t keys[] = {
	/* modifiers    key               function              argument */
        { 0,            XK_q,             g_quit,               None },
        { 0,            XK_Return,        g_switch_mode,        None },
        { 0,            XK_f,             g_toggle_fullscreen,  None },
        { 0,            XK_b,             g_toggle_bar,         None },
//	{ 0,            XK_plus,          g_zoom,               +1 },
	{ 0,            XK_KP_Add,        g_zoom,               +1 },
//	{ 0,            XK_minus,         g_zoom,               -1 },
	{ 0,            XK_KP_Subtract,   g_zoom,               -1 },
	{ 0,            XK_R,             t_reload_all,         None },
	{ ShiftMask,    XK_F5,            t_reload_all,         None },
	{ 0,            XK_Delete,        g_remove_image,      None },

	//{ 0,            XK_n,             i_navigate,           +1 },
	{ 0,            XK_KP_Right,      i_navigate,           +1 },
	{ 0,            XK_space,         i_navigate,           +1 },
	{ 0,            XK_KP_Begin,      i_navigate,           +1 },
	{ 0,            XK_p,             i_navigate,           -1 },
// From recent rebase. Interesting -- is this 'move to first image' in thumbnail view?
	// { 0,            XK_p,             i_scroll_to_edge,     (DIR_LEFT | DIR_UP) },
	{ 0,            XK_KP_Left,       i_navigate,           -1 },
	{ 0,            XK_BackSpace,     i_navigate,           -1 },
	{ 0,            XK_bracketright,  i_navigate,           +10 },
	{ 0,            XK_bracketleft,   i_navigate,           -10 },
	{ 0,            XK_KP_Prior,      i_navigate,           +10 },
	{ 0,            XK_KP_Home,       i_navigate,           -10 },

	{ 0,            XF86XK_Save,      i_alternate,          None },
	{ 0,            XK_braceleft,     g_change_gamma,       -1 },
	{ 0,            XK_braceright,    g_change_gamma,       +1 },
	{ 0,            XK_G,             g_change_gamma,        0 },
	// this one doesn't seem to work. Does it need to be KP_4 instead of KP_Left?
	{ 0          ,  XK_KP_4,          g_first,              None },
	{ Mod1Mask   ,  XK_Left,          g_reorder_image,      -9999999 },
	{ Mod1Mask   ,  XK_Right,         g_reorder_image,      9999999 },
	{ Mod1Mask|ControlMask, XK_Left, g_reorder_marked_images,  -1 },
	{ Mod1Mask|ControlMask, XK_Right, g_reorder_marked_images, 1 },
	{ 0,            XK_KP_Begin,      g_n_or_last,          None },

// these don't work, after the animation system change.
//	{ ControlMask,  XK_n,             i_navigate_frame,     +1 },
//	{ ControlMask,  XK_p,             i_navigate_frame,     -1 },
//	{ 0          ,  XK_KP_Next,       i_navigate_frame,     +1 },
//	{ 0          ,  XK_KP_End,        i_navigate_frame,     -1 },
	{ ControlMask,  XK_space,         i_toggle_animation,   None },
	{ 0,            XK_Left,          i_scroll,             DIR_LEFT },
	{ 0,            XK_Down,          i_scroll,             DIR_DOWN },
	{ 0,            XK_Up,            i_scroll,             DIR_UP },
	{ 0,            XK_Right,         i_scroll,             DIR_RIGHT },
	{ 0          ,  XK_KP_Down,       i_toggle_animation,   None },
	{ 0          ,  XK_KP_Enter,      i_toggle_animation,   None },
// marking an image should also move to the next image -- see https://github.com/muennich/sxiv/issues/172
// Note the order of entries here is significant -- we don't want to navigate to next before the toggle is processed.
	{ 0,            XK_m,             g_toggle_image_mark, None },
	{ 0,            XK_m,             i_navigate,           +1 },
	{ 0,            XK_m,             t_move_sel,           DIR_RIGHT },
	{ ControlMask,  XK_m,             g_reverse_marks,     None },
	{ 0,            XK_M,             g_unmark_all,     None },
// .. but we can also use the alternate if we don't want this behaviour.
	{ 0,            XK_KP_Divide,     g_toggle_image_mark, None },
	{ ControlMask,  XK_KP_Multiply,   g_reverse_marks,     None },
	{ ShiftMask,    XK_KP_Multiply,   g_unmark_all,     None },
	{ 0,            XK_KP_Insert,     g_navigate_marked,   -1 },
	{ 0,            XK_KP_Delete,     g_navigate_marked,   +1 },
	{ 0,            XK_Left,          t_move_sel,          DIR_LEFT },
	{ 0,            XK_Down,          t_move_sel,          DIR_DOWN },
	{ 0,            XK_Up,            t_move_sel,          DIR_UP },
	{ 0,            XK_Right,         t_move_sel,          DIR_RIGHT },
	{ ControlMask,  XK_Left,          g_scroll_screen,     DIR_LEFT },
	{ ControlMask,  XK_Down,          g_scroll_screen,     DIR_DOWN },
	{ ControlMask,  XK_Up,            g_scroll_screen,     DIR_UP },
	{ ControlMask,  XK_Right,         g_scroll_screen,     DIR_RIGHT },
	{ ShiftMask,    XK_Left,          i_scroll_to_edge,     DIR_LEFT },
	{ ShiftMask,    XK_Down,          i_scroll_to_edge,     DIR_DOWN },
	{ ShiftMask,    XK_Up,            i_scroll_to_edge,     DIR_UP },
	{ ShiftMask,    XK_Right,         i_scroll_to_edge,     DIR_RIGHT },
	{ 0,            XK_equal,         i_set_zoom,           100 },
	{ 0,            XK_w,             i_fit_to_win,         SCALE_DOWN },
	{ 0,            XK_W,             i_fit_to_win,         SCALE_FIT },
	{ 0,            XK_e,             i_fit_to_win,         SCALE_WIDTH },
	{ 0,            XK_E,             i_fit_to_win,         SCALE_HEIGHT },
	{ 0,            XK_comma,         i_rotate,             DEGREE_270 },
	{ 0,            XK_period,        i_rotate,             DEGREE_90 },
	{ 0,            XK_greater,       i_rotate,             DEGREE_180 },
	{ 0,            XK_backslash,     i_flip,               FLIP_HORIZONTAL },
	{ 0,            XK_minus,         i_flip,               FLIP_VERTICAL },
	{ 0,            XK_s,             i_slideshow,          None },
	// { 0,            XK_F6,            i_slideshow,          None },

	{ ControlMask,  XK_F9,            i_toggle_mouse_pos,   None },
	{ 0,            XK_F9,            i_toggle_antialias,   None },
	{ ShiftMask,    XK_F9,            i_toggle_alpha,      None },

};

/* mouse button mappings for image mode: */
static const button_t buttons[] = {
	/* modifiers    button            function              argument */
	{ 0,            2,                i_navigate,           +1 },
	{ 0,            3,                i_navigate,           -1 },
	{ 0,            1,                i_drag,               None },
	{ 0,            4,                i_scroll,             DIR_UP },
	{ 0,            5,                i_scroll,             DIR_DOWN },
	{ ShiftMask,    4,                i_scroll,             DIR_LEFT },
	{ ShiftMask,    5,                i_scroll,             DIR_RIGHT },
	{ 0,            6,                i_scroll,             DIR_LEFT },
	{ 0,            7,                i_scroll,             DIR_RIGHT },
	{ ControlMask,  4,                g_zoom,               +1 },
	{ ControlMask,  5,                g_zoom,               -1 },
};

#endif
