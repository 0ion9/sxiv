#define PIXELIZE_AT 199.4999

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
	5.0, 6.125, 9.1875,
	12.5,  25.0,  50.0,  75.0,
	100.0, 200.0, 300.0, 400.0, 800.0,
	1600.0, 2400.0, 16000.0
};

/* default slideshow delay (in 10th of a second, overwritten via -S option): */
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

/* if 0, pixelate images at zoom level != 100%,
 * if 1, antialias images at zoom level != 100%,
 * if 2, pixelate when zoom level >= IMAGE_PIXELIZE_AT ,
 *       antialias otherwise.
 * cycled with 'a' key binding.
 */
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

/* which index of the thumb_sizes array should be selected initially */
static const int default_thumbsize_index = 3;

/* zoom levels usable when zooming >= max thumbnail size
   leave 100.0 as-is, unless you want to alter the base zoom of thumbnails when <= max thumbnail size.

   Note that unlike zoom_levels, values are -integers-.
*/
static const int thumbnail_zoom_levels[] = { 100, 200, 250, 300, 400 };

/* pixelize if thumbnail zoom factor > this value.
   values <= 100 have no effect.
 */
static const int THUMBNAIL_PIXELIZE_AT = PIXELIZE_AT;

#endif
#ifdef _MAPPINGS_CONFIG

/* keyboard mappings for image and thumbnail mode: */
/* Plain Alphanumerics, shifted alphanumerics, control+alphanumerics and function keys are explicitly avoided,
   to allow the key handler script maximum latitude. */
static const keymap_t keys[] = {
	/* modifiers    key               function              argument */
        { ShiftMask,    XK_Escape,        g_quit,               None },
        { ShiftMask,    XF86XK_Close,     g_quit,               None },
        { 0,            XK_Return,        g_switch_mode,        None },
        { ControlMask,  XK_F,             g_toggle_fullscreen,  None },
        { ControlMask,  XK_B,             g_toggle_bar,         None },
	{ 0,            XK_KP_Add,        g_zoom,               +1 },
	{ 0,            XK_plusminus,     g_zoom,               +1 },
	{ 0,            XK_KP_Subtract,   g_zoom,               -1 },
	{ 0,            XK_identical,     g_zoom,               -1 },
	{ ControlMask,  XK_R,             t_reload_all,         None },
	{ 0,            XK_Delete,        g_remove_image,      None },

	{ 0,            XK_KP_Right,      i_navigate,           +1 },
	{ 0,            XK_space,         i_navigate,           +1 },
	{ 0,            XK_KP_Left,       i_navigate,           -1 },
	{ 0,            XK_BackSpace,     i_navigate,           -1 },
	{ 0,            XK_bracketright,  i_navigate,           +10 },
	{ 0,            XK_bracketleft,   i_navigate,           -10 },
	{ 0,            XK_rightt,        i_navigate,           +10 },
	{ 0,            XK_leftt,         i_navigate,           -10 },
	{ 0,            XK_KP_Prior,      i_navigate,           +10 },
	{ 0,            XK_KP_Home,       i_navigate,           -10 },
	{ 0,            XF86XK_Save,      i_alternate,          None },
	{ 0,            XK_braceleft,     g_change_gamma,       -1 },
	{ 0,            XK_braceright,    g_change_gamma,       +1 },
	{ ControlMask,  XK_G,             g_change_gamma,        0 },
	{ 0          ,  XK_S,             g_cycle_silhouetting, None },
	{ 0          ,  XK_T,             g_cycle_opacity,      0 },
	{ 0          ,  XK_W,             i_cycle_tiling,       None },
	{ 0          ,  XK_X,             i_cycle_scalefactor,  None },
	{ ControlMask,  XK_t,             g_toggle_negalpha,    None },
	{ 0          ,  XK_KP_4,          g_first,              None },
	{ 0          ,  XK_C,             g_clone_image,        None },
	{ Mod1Mask   ,  XK_Left,          g_reorder_image,      -9999999 },
	{ Mod1Mask   ,  XK_Right,         g_reorder_image,      9999999 },
	{ ShiftMask  ,  XK_Left,          g_reorder_image,      -16 },
	{ ShiftMask  ,  XK_Right,         g_reorder_image,      16 },
	{ Mod1Mask|ShiftMask,  XK_Left,   g_reorder_image_to_alternate,      -1 },
	{ Mod1Mask|ShiftMask,  XK_Right,  g_reorder_image_to_alternate,      1 },
	{ 0          ,  XK_Pointer_Left,  t_move_and_reorder,   DIR_LEFT },
	{ 0          ,  XK_Pointer_Right, t_move_and_reorder,   DIR_RIGHT },
	{ 0          ,  XK_Pointer_Up,    t_move_and_reorder,   DIR_UP },
	{ 0          ,  XK_Pointer_Down,  t_move_and_reorder,   DIR_DOWN },
	{ Mod1Mask|ControlMask, XK_Left,  g_reorder_marked_images,  -1 },
	{ Mod1Mask|ControlMask, XK_Right, g_reorder_marked_images, 1 },
	// xxx absurd. get a less modifiery shortcut for this
	{ ShiftMask|ControlMask, XK_Left,  g_reorder_marked_images_to_alternate,  -1 },
	{ ShiftMask|ControlMask, XK_Right, g_reorder_marked_images_to_alternate, 1 },
	{ 0,            XK_KP_Begin,      g_n_or_last,          None },
	{ ControlMask,  XK_space,         i_toggle_animation,   None },
	{ 0,            XK_Left,          i_scroll,             DIR_LEFT },
	{ 0,            XK_Down,          i_scroll,             DIR_DOWN },
	{ 0,            XK_Up,            i_scroll,             DIR_UP },
	{ 0,            XK_Right,         i_scroll,             DIR_RIGHT },
	{ 0,            XK_leftarrow,     i_scroll,             DIR_LEFT },
	{ 0,            XK_downarrow,     i_scroll,             DIR_DOWN },
	{ 0,            XK_uparrow,       i_scroll,             DIR_UP },
	{ 0,            XK_rightarrow,    i_scroll,             DIR_RIGHT },
	{ 0          ,  XK_KP_Down,       i_toggle_animation,   None },
	{ 0          ,  XK_KP_Enter,      i_toggle_animation,   None },
// marking an image should also move to the next image -- see https://github.com/muennich/sxiv/issues/172
// Note the order of entries here is significant -- we don't want to navigate to next before the toggle is processed.
	{ 0,            XK_Menu,          g_toggle_image_mark, None },
	{ 0,            XK_Menu,          i_navigate,           +1 },
	{ 0,            XK_Menu,          t_move_sel,           DIR_RIGHT },
	{ ControlMask,  XK_Menu,          g_reverse_marks,     None },
	{ ShiftMask,    XK_Menu,          g_unmark_all,     None },
	{ 0,            XK_KP_Insert,     g_navigate_marked,   -1 },
	{ 0,            XK_KP_Delete,     g_navigate_marked,   +1 },
	{ 0,            XK_Left,          t_move_sel,          DIR_LEFT },
	{ 0,            XK_Down,          t_move_sel,          DIR_DOWN },
	{ 0,            XK_Up,            t_move_sel,          DIR_UP },
	{ 0,            XK_Right,         t_move_sel,          DIR_RIGHT },
	{ 0,            XK_leftarrow,     t_move_sel,          DIR_LEFT },
	{ 0,            XK_downarrow,     t_move_sel,          DIR_DOWN },
	{ 0,            XK_uparrow,       t_move_sel,          DIR_UP },
	{ 0,            XK_rightarrow,    t_move_sel,          DIR_RIGHT },
	{ ControlMask,  XK_Left,          g_scroll_screen,     DIR_LEFT },
	{ ControlMask,  XK_Down,          g_scroll_screen,     DIR_DOWN },
	{ ControlMask,  XK_Up,            g_scroll_screen,     DIR_UP },
	{ ControlMask,  XK_Right,         g_scroll_screen,     DIR_RIGHT },
	{ ControlMask,  XK_leftarrow,     g_scroll_screen,     DIR_LEFT },
	{ ControlMask,  XK_downarrow,     g_scroll_screen,     DIR_DOWN },
	{ ControlMask,  XK_uparrow,       g_scroll_screen,     DIR_UP },
	{ ControlMask,  XK_rightarrow,    g_scroll_screen,     DIR_RIGHT },
	{ ShiftMask,    XK_Left,          i_scroll_to_edge,     DIR_LEFT },
	{ ShiftMask,    XK_Down,          i_scroll_to_edge,     DIR_DOWN },
	{ ShiftMask,    XK_Up,            i_scroll_to_edge,     DIR_UP },
	{ ShiftMask,    XK_Right,         i_scroll_to_edge,     DIR_RIGHT },
	{ ShiftMask,    XK_leftarrow,     i_scroll_to_edge,     DIR_LEFT },
	{ ShiftMask,    XK_downarrow,     i_scroll_to_edge,     DIR_DOWN },
	{ ShiftMask,    XK_uparrow,       i_scroll_to_edge,     DIR_UP },
	{ ShiftMask,    XK_rightarrow,    i_scroll_to_edge,     DIR_RIGHT },
	{ ControlMask,  XK_plus,          i_set_zoom,           100 },
	{ Mod1Mask,     XK_minus,         i_fit_to_win,         SCALE_DOWN },
	{ 0,            XK_underscore,    i_fit_to_win,         SCALE_FIT },
	{ ControlMask,  XK_minus,         i_fit_to_win,         SCALE_WIDTH },
	{ ControlMask,  XK_underscore,    i_fit_to_win,         SCALE_HEIGHT },
	{ ControlMask,  XK_D,             i_fit_to_win,         SCALE_DISTORT },
	{ 0,            XK_comma,         i_rotate,             DEGREE_270 },
	{ 0,            XK_period,        i_rotate,             DEGREE_90 },
	{ 0,            XK_greater,       i_rotate,             DEGREE_180 },
	{ 0,            XK_kana_comma,    i_rotate,             DEGREE_270 },
	{ 0,            XK_kana_fullstop, i_rotate,             DEGREE_90 },
	{ 0,            XK_emopencircle,  i_rotate,             DEGREE_180 },
	{ 0,            XK_backslash,     i_flip,               FLIP_HORIZONTAL },
	{ 0,            XK_minus,         i_flip,               FLIP_VERTICAL },
	{ 0,            XK_exclamdown,    i_flip,               FLIP_HORIZONTAL },
	{ 0,            XK_emdash,        i_flip,               FLIP_VERTICAL },
	{ ControlMask,  XK_S,             i_slideshow,          None },

	{ ControlMask,  XK_P,             i_toggle_mouse_pos,   None },
	{ ControlMask,  XK_A,             i_cycle_antialias,    None },
	{ ControlMask,  XK_T,             i_toggle_alpha,       None },

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
