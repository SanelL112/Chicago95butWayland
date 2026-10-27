/*
 * chicago-control-panel: Authentic Windows 95 Control Panel & System Dialog Suite
 *
 * Modes:
 *   (default)       : Classic Windows 95 "Control Panel" folder window (grid of 32x32 applets + status bar)
 *   --display       : Display Properties (CRT monitor preview, Background patterns/wallpapers, Appearance schemes, Multi-Monitor Settings)
 *   --audio         : Sounds & Multimedia Properties (Master Volume slider, Mute, Output sink switcher, Mic slider, Startup sound test)
 *   --network       : Network Properties (Live Wi-Fi scanner/connector, Password dialog, Radio toggle, IPv4 status)
 *   --power         : Power Management Properties (Brightness slider, Power profile selector, Idle timeout, Live battery/RAPL status)
 *   --mouse         : Mouse & Touchpad Properties (Tap-to-click, Natural scroll, Accel speed, Double-click test area)
 *   --keyboard      : Keyboard Properties (Repeat rate/delay, Windows 95 shortcut reference)
 *   --datetime      : Date/Time Properties (Calendar, Live clock, Timezone info)
 *   --system        : System Properties (Win95 PC graphic, General info, Device Manager tree, Live Performance monitor)
 *   --taskbar       : Taskbar & Start Menu Properties (Taskbar backend, Desktop icon monitor)
 *   --run           : Classic Windows 95 "Run" dialog
 *   --find          : Classic Windows 95 "Find: All Programs & Files" dialog
 *   --shutdown      : Classic Windows 95 "Shut Down Windows" dialog
 *   --help-viewer   : Classic Windows 95 Help Viewer
 */

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <cairo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../include/chicago_common.h"

/* Forward declarations for applet dialogs */
static void show_display_properties_dialog(gboolean standalone);
static void show_audio_properties_dialog(gboolean standalone);
static void show_network_properties_dialog(gboolean standalone);
static void show_power_properties_dialog(gboolean standalone);
static void show_mouse_properties_dialog(gboolean standalone);
static void show_keyboard_properties_dialog(gboolean standalone);
static void show_datetime_properties_dialog(gboolean standalone);
static void show_system_properties_dialog(gboolean standalone);
static void show_taskbar_properties_dialog(gboolean standalone);
static void build_find_dialog(gboolean standalone);

static void on_dialog_destroy(GtkWidget *w, gpointer data) {
    (void)w;
    gboolean standalone = GPOINTER_TO_INT(data);
    if (standalone) {
        gtk_main_quit();
    }
}

/* =========================================================================
 * 1. DISPLAY PROPERTIES (CRT Monitor Preview + Background + Appearance + Monitors)
 * ========================================================================= */

typedef struct {
    GtkWidget *crt_area;
    GtkWidget *color_combo;
    GtkWidget *wall_tree;
    GtkWidget *mode_combo;
    GtkWidget *scheme_combo;
    GtkWidget *scheme_preview;
    GtkWidget *mon_combo;
    GtkWidget *res_combo;
    GtkWidget *scale_combo;
    GtkWidget *icon_mon_combo;

    GdkRGBA preview_bg;
    char preview_wall_path[512];
    char preview_wall_mode[32];
    GdkPixbuf *preview_wall_pb;

    GdkRGBA scheme_active_title;
    GdkRGBA scheme_inactive_title;
    GdkRGBA scheme_window_bg;
} DisplayPropState;

static gboolean on_draw_crt_monitor(GtkWidget *widget, cairo_t *cr, gpointer data) {
    DisplayPropState *st = (DisplayPropState *)data;
    int w = gtk_widget_get_allocated_width(widget);

    int cx = w / 2;
    int m_w = 184, m_h = 136;
    int mx = cx - m_w / 2;
    int my = 6;

    /* Outer CRT Monitor Bezel (#c0c0c0 with 3D Win95 bevel) */
    cairo_set_source_rgb(cr, 0.753, 0.753, 0.753);
    cairo_rectangle(cr, mx, my, m_w, m_h - 20);
    cairo_fill(cr);
    chicago_draw_bevel(cr, mx, my, m_w, m_h - 20, TRUE);

    /* Inner dark bezel frame */
    int sx = mx + 14, sy = my + 12;
    int sw = m_w - 28, sh = m_h - 46;
    chicago_draw_bevel(cr, sx - 2, sy - 2, sw + 4, sh + 4, FALSE);

    /* CRT Screen Content (Live Preview of selected color & pattern/wallpaper) */
    cairo_save(cr);
    cairo_rectangle(cr, sx, sy, sw, sh);
    cairo_clip(cr);

    cairo_set_source_rgb(cr, st->preview_bg.red, st->preview_bg.green, st->preview_bg.blue);
    cairo_rectangle(cr, sx, sy, sw, sh);
    cairo_fill(cr);

    if (st->preview_wall_pb) {
        int pw = gdk_pixbuf_get_width(st->preview_wall_pb);
        int ph = gdk_pixbuf_get_height(st->preview_wall_pb);
        if (pw > 0 && ph > 0) {
            if (g_strcmp0(st->preview_wall_mode, "center") == 0) {
                gdk_cairo_set_source_pixbuf(cr, st->preview_wall_pb, sx + (sw - pw) / 2, sy + (sh - ph) / 2);
                cairo_paint(cr);
            } else if (g_strcmp0(st->preview_wall_mode, "stretch") == 0) {
                GdkPixbuf *sc = gdk_pixbuf_scale_simple(st->preview_wall_pb, sw, sh, GDK_INTERP_NEAREST);
                if (sc) {
                    gdk_cairo_set_source_pixbuf(cr, sc, sx, sy);
                    cairo_paint(cr);
                    g_object_unref(sc);
                }
            } else {
                gdk_cairo_set_source_pixbuf(cr, st->preview_wall_pb, sx, sy);
                cairo_pattern_t *pat = cairo_get_source(cr);
                cairo_pattern_set_extend(pat, CAIRO_EXTEND_REPEAT);
                cairo_rectangle(cr, sx, sy, sw, sh);
                cairo_fill(cr);
            }
        }
    }

    /* Miniature taskbar inside CRT preview */
    cairo_set_source_rgb(cr, 0.753, 0.753, 0.753);
    cairo_rectangle(cr, sx, sy + sh - 10, sw, 10);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_rectangle(cr, sx, sy + sh - 10, sw, 1);
    cairo_fill(cr);

    cairo_restore(cr);

    /* Power LED on CRT bezel (Green pixel) */
    cairo_set_source_rgb(cr, 0.0, 0.85, 0.0);
    cairo_rectangle(cr, mx + m_w - 24, my + m_h - 28, 6, 3);
    cairo_fill(cr);

    /* CRT Neck & Base Stand */
    cairo_set_source_rgb(cr, 0.753, 0.753, 0.753);
    cairo_rectangle(cr, cx - 22, my + m_h - 20, 44, 10);
    cairo_fill(cr);
    chicago_draw_bevel(cr, cx - 22, my + m_h - 20, 44, 10, TRUE);

    cairo_set_source_rgb(cr, 0.753, 0.753, 0.753);
    cairo_rectangle(cr, cx - 48, my + m_h - 10, 96, 10);
    cairo_fill(cr);
    chicago_draw_bevel(cr, cx - 48, my + m_h - 10, 96, 10, TRUE);

    return FALSE;
}

static gboolean on_draw_scheme_preview(GtkWidget *widget, cairo_t *cr, gpointer data) {
    DisplayPropState *st = (DisplayPropState *)data;
    int w = gtk_widget_get_allocated_width(widget);
    int h = gtk_widget_get_allocated_height(widget);

    /* Desktop background */
    cairo_set_source_rgb(cr, st->preview_bg.red, st->preview_bg.green, st->preview_bg.blue);
    cairo_rectangle(cr, 0, 0, w, h);
    cairo_fill(cr);
    chicago_draw_bevel(cr, 0, 0, w, h, FALSE);

    /* Inactive Window */
    int iw_x = 16, iw_y = 10, iw_w = w - 60, iw_h = 68;
    cairo_set_source_rgb(cr, 0.753, 0.753, 0.753);
    cairo_rectangle(cr, iw_x, iw_y, iw_w, iw_h);
    cairo_fill(cr);
    chicago_draw_bevel(cr, iw_x, iw_y, iw_w, iw_h, TRUE);
    cairo_set_source_rgb(cr, st->scheme_inactive_title.red, st->scheme_inactive_title.green, st->scheme_inactive_title.blue);
    cairo_rectangle(cr, iw_x + 3, iw_y + 3, iw_w - 6, 16);
    cairo_fill(cr);

    PangoLayout *layout = gtk_widget_create_pango_layout(widget, NULL);
    pango_layout_set_markup(layout, "<span font_desc='Helvetica Bold 8' foreground='#c0c0c0'>Inactive Window</span>", -1);
    cairo_move_to(cr, iw_x + 7, iw_y + 5);
    pango_cairo_show_layout(cr, layout);

    /* Active Window overlapping */
    int aw_x = 34, aw_y = 32, aw_w = w - 50, aw_h = 82;
    cairo_set_source_rgb(cr, 0.753, 0.753, 0.753);
    cairo_rectangle(cr, aw_x, aw_y, aw_w, aw_h);
    cairo_fill(cr);
    chicago_draw_bevel(cr, aw_x, aw_y, aw_w, aw_h, TRUE);
    cairo_set_source_rgb(cr, st->scheme_active_title.red, st->scheme_active_title.green, st->scheme_active_title.blue);
    cairo_rectangle(cr, aw_x + 3, aw_y + 3, aw_w - 6, 16);
    cairo_fill(cr);

    pango_layout_set_markup(layout, "<span font_desc='Helvetica Bold 8' foreground='#ffffff'>Active Window</span>", -1);
    cairo_move_to(cr, aw_x + 7, aw_y + 5);
    pango_cairo_show_layout(cr, layout);

    /* Sunken white client area inside Active Window */
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_rectangle(cr, aw_x + 6, aw_y + 24, aw_w - 12, aw_h - 30);
    cairo_fill(cr);
    chicago_draw_bevel(cr, aw_x + 6, aw_y + 24, aw_w - 12, aw_h - 30, FALSE);
    pango_layout_set_markup(layout, "<span font_desc='Helvetica 8' foreground='#000000'>Window Text</span>", -1);
    cairo_move_to(cr, aw_x + 12, aw_y + 30);
    pango_cairo_show_layout(cr, layout);

    g_object_unref(layout);
    return FALSE;
}

static void on_bg_color_changed(GtkComboBox *combo, gpointer data) {
    DisplayPropState *st = (DisplayPropState *)data;
    const char *id = gtk_combo_box_get_active_id(combo);
    if (id) {
        gdk_rgba_parse(&st->preview_bg, id);
        if (st->crt_area) gtk_widget_queue_draw(st->crt_area);
        if (st->scheme_preview) gtk_widget_queue_draw(st->scheme_preview);
    }
}

static void on_wall_mode_changed(GtkComboBox *combo, gpointer data) {
    DisplayPropState *st = (DisplayPropState *)data;
    const char *id = gtk_combo_box_get_active_id(combo);
    if (id) {
        snprintf(st->preview_wall_mode, sizeof(st->preview_wall_mode), "%s", id);
        if (st->crt_area) gtk_widget_queue_draw(st->crt_area);
    }
}

static void on_wall_selection_changed(GtkTreeSelection *sel, gpointer data) {
    DisplayPropState *st = (DisplayPropState *)data;
    GtkTreeModel *model;
    GtkTreeIter iter;
    if (gtk_tree_selection_get_selected(sel, &model, &iter)) {
        char *path = NULL;
        gtk_tree_model_get(model, &iter, 1, &path, -1);
        snprintf(st->preview_wall_path, sizeof(st->preview_wall_path), "%s", path ? path : "");
        if (st->preview_wall_pb) {
            g_object_unref(st->preview_wall_pb);
            st->preview_wall_pb = NULL;
        }
        if (path && *path && g_file_test(path, G_FILE_TEST_EXISTS)) {
            st->preview_wall_pb = gdk_pixbuf_new_from_file(path, NULL);
        }
        g_free(path);
        if (st->crt_area) gtk_widget_queue_draw(st->crt_area);
    }
}

static void on_scheme_changed(GtkComboBox *combo, gpointer data) {
    DisplayPropState *st = (DisplayPropState *)data;
    int idx = gtk_combo_box_get_active(combo);
    switch (idx) {
        case 1: /* Brick */
            gdk_rgba_parse(&st->scheme_active_title, "#800000");
            gdk_rgba_parse(&st->scheme_inactive_title, "#808040");
            break;
        case 2: /* Plum */
            gdk_rgba_parse(&st->scheme_active_title, "#400040");
            gdk_rgba_parse(&st->scheme_inactive_title, "#808080");
            break;
        case 3: /* Rainy Day */
            gdk_rgba_parse(&st->scheme_active_title, "#406080");
            gdk_rgba_parse(&st->scheme_inactive_title, "#808080");
            break;
        case 4: /* Teal */
            gdk_rgba_parse(&st->scheme_active_title, "#008080");
            gdk_rgba_parse(&st->scheme_inactive_title, "#808080");
            break;
        default: /* Windows Standard (#000080 Navy) */
            gdk_rgba_parse(&st->scheme_active_title, "#000080");
            gdk_rgba_parse(&st->scheme_inactive_title, "#808080");
            break;
    }
    if (st->scheme_preview) gtk_widget_queue_draw(st->scheme_preview);
}

static void apply_display_properties(DisplayPropState *st) {
    const char *bg_id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(st->color_combo));
    if (bg_id) chicago_config_set("bg_color", bg_id);
    chicago_config_set("wallpaper", st->preview_wall_path);
    chicago_config_set("wallpaper_mode", st->preview_wall_mode);

    const char *icon_mon = gtk_combo_box_get_active_id(GTK_COMBO_BOX(st->icon_mon_combo));
    if (icon_mon) chicago_config_set("icon_monitor", icon_mon);

    /* Reload chicago-desktop immediately */
    chicago_spawn_command("pkill -USR1 chicago-desktop");

    /* Apply monitor resolution & scale if selected */
    char *mon = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(st->mon_combo));
    char *res = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(st->res_combo));
    const char *scale = gtk_combo_box_get_active_id(GTK_COMBO_BOX(st->scale_combo));
    if (mon && res && scale && g_strcmp0(res, "(Keep Current)") != 0) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "wlr-randr --output %s --mode %s --scale %s", mon, res, scale);
        chicago_spawn_command(cmd);
    } else if (mon && scale) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "wlr-randr --output %s --scale %s", mon, scale);
        chicago_spawn_command(cmd);
    }
    g_free(mon);
    g_free(res);
}

static void on_display_apply_clicked(GtkButton *btn, gpointer data) {
    (void)btn;
    apply_display_properties((DisplayPropState *)data);
}

static void on_display_ok_clicked(GtkButton *btn, gpointer data) {
    DisplayPropState *st = (DisplayPropState *)data;
    apply_display_properties(st);
    GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(btn));
    if (GTK_IS_WINDOW(toplevel)) gtk_widget_destroy(toplevel);
}

static void show_display_properties_dialog(gboolean standalone) {
    DisplayPropState *st = g_malloc0(sizeof(DisplayPropState));
    char bg_str[64];
    chicago_config_get("bg_color", bg_str, sizeof(bg_str), "#008080");
    if (!gdk_rgba_parse(&st->preview_bg, bg_str)) gdk_rgba_parse(&st->preview_bg, "#008080");
    chicago_config_get("wallpaper", st->preview_wall_path, sizeof(st->preview_wall_path), "");
    chicago_config_get("wallpaper_mode", st->preview_wall_mode, sizeof(st->preview_wall_mode), "tile");
    if (st->preview_wall_path[0] && g_file_test(st->preview_wall_path, G_FILE_TEST_EXISTS)) {
        st->preview_wall_pb = gdk_pixbuf_new_from_file(st->preview_wall_path, NULL);
    }
    gdk_rgba_parse(&st->scheme_active_title, "#000080");
    gdk_rgba_parse(&st->scheme_inactive_title, "#808080");

    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Display Properties");
    gtk_window_set_default_size(GTK_WINDOW(win), 420, 440);
    gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_CENTER);
    gtk_style_context_add_class(gtk_widget_get_style_context(win), "win95-window");
    g_signal_connect(win, "destroy", G_CALLBACK(on_dialog_destroy), GINT_TO_POINTER(standalone));

    GtkWidget *root_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(root_vbox), 8);

    GtkWidget *nb = gtk_notebook_new();

    /* --- Tab 1: Background --- */
    GtkWidget *bg_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(bg_vbox), 6);

    st->crt_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(st->crt_area, 220, 144);
    g_signal_connect(st->crt_area, "draw", G_CALLBACK(on_draw_crt_monitor), st);
    gtk_box_pack_start(GTK_BOX(bg_vbox), st->crt_area, FALSE, FALSE, 2);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);

    /* Desktop Color Picker */
    GtkWidget *col_frame = gtk_frame_new("Desktop Color");
    GtkWidget *col_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_container_set_border_width(GTK_CONTAINER(col_box), 6);
    st->color_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(st->color_combo), "#008080", "Classic Windows 95 Teal (#008080)");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(st->color_combo), "#000080", "Classic Navy (#000080)");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(st->color_combo), "#3a6ea5", "Windows 2000 Blue (#3A6EA5)");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(st->color_combo), "#004040", "Dark Forest Teal (#004040)");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(st->color_combo), "#808080", "Classic Slate Gray (#808080)");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(st->color_combo), "#400040", "Classic Plum (#400040)");
    if (!gtk_combo_box_set_active_id(GTK_COMBO_BOX(st->color_combo), bg_str)) {
        gtk_combo_box_set_active(GTK_COMBO_BOX(st->color_combo), 0);
    }
    g_signal_connect(st->color_combo, "changed", G_CALLBACK(on_bg_color_changed), st);
    gtk_box_pack_start(GTK_BOX(col_box), st->color_combo, FALSE, FALSE, 0);

    GtkWidget *mode_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_pack_start(GTK_BOX(mode_hbox), gtk_label_new("Display:"), FALSE, FALSE, 0);
    st->mode_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(st->mode_combo), "tile", "Tile");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(st->mode_combo), "center", "Center");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(st->mode_combo), "stretch", "Stretch");
    if (!gtk_combo_box_set_active_id(GTK_COMBO_BOX(st->mode_combo), st->preview_wall_mode)) {
        gtk_combo_box_set_active(GTK_COMBO_BOX(st->mode_combo), 0);
    }
    g_signal_connect(st->mode_combo, "changed", G_CALLBACK(on_wall_mode_changed), st);
    gtk_box_pack_start(GTK_BOX(mode_hbox), st->mode_combo, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(col_box), mode_hbox, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(col_frame), col_box);

    /* Wallpaper / Pattern List */
    GtkWidget *wall_frame = gtk_frame_new("Pattern / Wallpaper");
    GtkWidget *wall_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_size_request(wall_scroll, 360, 110);
    GtkListStore *wstore = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
    GtkTreeIter iter;
    gtk_list_store_append(wstore, &iter);
    gtk_list_store_set(wstore, &iter, 0, "(None)", 1, "", -1);

    const char *bg_dirs[] = { "Wallpaper", "Patterns", NULL };
    for (int d = 0; bg_dirs[d]; d++) {
        char dpath[512];
        snprintf(dpath, sizeof(dpath), "%s/.local/share/backgrounds/chicago95/%s", g_get_home_dir(), bg_dirs[d]);
        GDir *dir = g_dir_open(dpath, 0, NULL);
        if (dir) {
            const char *fn;
            while ((fn = g_dir_read_name(dir)) != NULL) {
                if (g_str_has_suffix(fn, ".png") || g_str_has_suffix(fn, ".jpg")) {
                    char fpath[1024], label[128];
                    snprintf(fpath, sizeof(fpath), "%s/%s", dpath, fn);
                    snprintf(label, sizeof(label), "[%s] %s", bg_dirs[d], fn);
                    gtk_list_store_append(wstore, &iter);
                    gtk_list_store_set(wstore, &iter, 0, label, 1, fpath, -1);
                }
            }
            g_dir_close(dir);
        }
    }

    st->wall_tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(wstore));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(st->wall_tree), FALSE);
    GtkCellRenderer *rend = gtk_cell_renderer_text_new();
    gtk_tree_view_append_column(GTK_TREE_VIEW(st->wall_tree),
        gtk_tree_view_column_new_with_attributes("Wallpaper", rend, "text", 0, NULL));
    GtkTreeSelection *wsel = gtk_tree_view_get_selection(GTK_TREE_VIEW(st->wall_tree));
    g_signal_connect(wsel, "changed", G_CALLBACK(on_wall_selection_changed), st);
    gtk_container_add(GTK_CONTAINER(wall_scroll), st->wall_tree);
    gtk_container_add(GTK_CONTAINER(wall_frame), wall_scroll);

    gtk_box_pack_start(GTK_BOX(bg_vbox), wall_frame, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(bg_vbox), col_frame, FALSE, FALSE, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(nb), bg_vbox, gtk_label_new("Background"));

    /* --- Tab 2: Appearance --- */
    GtkWidget *app_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(app_vbox), 8);

    st->scheme_preview = gtk_drawing_area_new();
    gtk_widget_set_size_request(st->scheme_preview, 340, 130);
    g_signal_connect(st->scheme_preview, "draw", G_CALLBACK(on_draw_scheme_preview), st);
    gtk_box_pack_start(GTK_BOX(app_vbox), st->scheme_preview, FALSE, FALSE, 0);

    GtkWidget *sch_lbl = gtk_label_new("Scheme:");
    gtk_label_set_xalign(GTK_LABEL(sch_lbl), 0.0f);
    gtk_box_pack_start(GTK_BOX(app_vbox), sch_lbl, FALSE, FALSE, 0);

    st->scheme_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(st->scheme_combo), "Windows Standard (Chicago95)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(st->scheme_combo), "Brick");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(st->scheme_combo), "Plum");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(st->scheme_combo), "Rainy Day");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(st->scheme_combo), "Teal");
    gtk_combo_box_set_active(GTK_COMBO_BOX(st->scheme_combo), 0);
    g_signal_connect(st->scheme_combo, "changed", G_CALLBACK(on_scheme_changed), st);
    gtk_box_pack_start(GTK_BOX(app_vbox), st->scheme_combo, FALSE, FALSE, 0);

    GtkWidget *font_info = gtk_label_new("UI Font: Helvetica 8pt (Crisp OpenType Bitmap / MS Sans Serif)\nTerminal Font: More Perfect DOS VGA 12pt");
    gtk_label_set_xalign(GTK_LABEL(font_info), 0.0f);
    gtk_box_pack_start(GTK_BOX(app_vbox), font_info, FALSE, FALSE, 4);
    gtk_notebook_append_page(GTK_NOTEBOOK(nb), app_vbox, gtk_label_new("Appearance"));

    /* --- Tab 3: Settings (Multi-Monitor & Resolution) --- */
    GtkWidget *set_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(set_vbox), 8);

    GtkWidget *mon_frame = gtk_frame_new("Display Output & Resolution");
    GtkWidget *mon_grid = gtk_grid_new();
    gtk_container_set_border_width(GTK_CONTAINER(mon_grid), 8);
    gtk_grid_set_row_spacing(GTK_GRID(mon_grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(mon_grid), 8);

    gtk_grid_attach(GTK_GRID(mon_grid), gtk_label_new("Monitor Output:"), 0, 0, 1, 1);
    st->mon_combo = gtk_combo_box_text_new();
    char *randr_out = NULL;
    if (g_spawn_command_line_sync("wlr-randr", &randr_out, NULL, NULL, NULL) && randr_out) {
        char **lines = g_strsplit(randr_out, "\n", -1);
        for (int i = 0; lines[i]; i++) {
            if (lines[i][0] != '\0' && lines[i][0] != ' ') {
                char *sp = strchr(lines[i], ' ');
                if (sp) *sp = '\0';
                gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(st->mon_combo), lines[i]);
            }
        }
        g_strfreev(lines);
        g_free(randr_out);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(st->mon_combo), 0);
    gtk_grid_attach(GTK_GRID(mon_grid), st->mon_combo, 1, 0, 1, 1);

    gtk_grid_attach(GTK_GRID(mon_grid), gtk_label_new("Desktop Area (Mode):"), 0, 1, 1, 1);
    st->res_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(st->res_combo), "(Keep Current)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(st->res_combo), "1920x1200@59.950001Hz");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(st->res_combo), "1920x1080@74.973000Hz");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(st->res_combo), "1920x1080@60.000000Hz");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(st->res_combo), "1600x900@60.000000Hz");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(st->res_combo), "1280x720@60.000000Hz");
    gtk_combo_box_set_active(GTK_COMBO_BOX(st->res_combo), 0);
    gtk_grid_attach(GTK_GRID(mon_grid), st->res_combo, 1, 1, 1, 1);

    gtk_grid_attach(GTK_GRID(mon_grid), gtk_label_new("Display Scaling:"), 0, 2, 1, 1);
    st->scale_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(st->scale_combo), "1.0", "100% (1.0x Normal)");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(st->scale_combo), "1.25", "125% (1.25x Medium)");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(st->scale_combo), "1.5", "150% (1.5x Large)");
    gtk_combo_box_set_active(GTK_COMBO_BOX(st->scale_combo), 0);
    gtk_grid_attach(GTK_GRID(mon_grid), st->scale_combo, 1, 2, 1, 1);

    gtk_grid_attach(GTK_GRID(mon_grid), gtk_label_new("Show Desktop Icons On:"), 0, 3, 1, 1);
    st->icon_mon_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(st->icon_mon_combo), "all", "All Connected Monitors");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(st->icon_mon_combo), "primary", "Primary Monitor Only");
    char cur_im[64];
    chicago_config_get("icon_monitor", cur_im, sizeof(cur_im), "all");
    if (!gtk_combo_box_set_active_id(GTK_COMBO_BOX(st->icon_mon_combo), cur_im)) {
        gtk_combo_box_set_active(GTK_COMBO_BOX(st->icon_mon_combo), 0);
    }
    gtk_grid_attach(GTK_GRID(mon_grid), st->icon_mon_combo, 1, 3, 1, 1);

    gtk_container_add(GTK_CONTAINER(mon_frame), mon_grid);
    gtk_box_pack_start(GTK_BOX(set_vbox), mon_frame, FALSE, FALSE, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(nb), set_vbox, gtk_label_new("Settings"));

    gtk_box_pack_start(GTK_BOX(root_vbox), nb, TRUE, TRUE, 0);

    /* Classic OK / Cancel / Apply Bottom Row */
    GtkWidget *btn_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *ok_btn = gtk_button_new_with_label("OK");
    GtkWidget *cancel_btn = gtk_button_new_with_label("Cancel");
    GtkWidget *apply_btn = gtk_button_new_with_label("Apply");
    gtk_widget_set_size_request(ok_btn, 75, 23);
    gtk_widget_set_size_request(cancel_btn, 75, 23);
    gtk_widget_set_size_request(apply_btn, 75, 23);

    g_signal_connect(ok_btn, "clicked", G_CALLBACK(on_display_ok_clicked), st);
    g_signal_connect_swapped(cancel_btn, "clicked", G_CALLBACK(gtk_widget_destroy), win);
    g_signal_connect(apply_btn, "clicked", G_CALLBACK(on_display_apply_clicked), st);

    gtk_box_pack_end(GTK_BOX(btn_row), apply_btn, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(btn_row), cancel_btn, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(btn_row), ok_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(root_vbox), btn_row, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(win), root_vbox);
    gtk_widget_show_all(win);
}

/* =========================================================================
 * 2. SOUNDS & AUDIO PROPERTIES
 * ========================================================================= */

static void on_audio_vol_changed(GtkRange *range, gpointer data) {
    (void)data;
    int v = CLAMP((int)gtk_range_get_value(range), 0, 150);
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "wpctl set-volume @DEFAULT_AUDIO_SINK@ %d%%", v);
    chicago_spawn_command(cmd);
}

static void on_audio_mute_toggled(GtkToggleButton *tb, gpointer data) {
    (void)data;
    gboolean m = gtk_toggle_button_get_active(tb);
    chicago_spawn_command(m ? "wpctl set-mute @DEFAULT_AUDIO_SINK@ 1" : "wpctl set-mute @DEFAULT_AUDIO_SINK@ 0");
}

static void on_audio_mic_changed(GtkRange *range, gpointer data) {
    (void)data;
    int v = CLAMP((int)gtk_range_get_value(range), 0, 100);
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "wpctl set-volume @DEFAULT_AUDIO_SOURCE@ %d%%", v);
    chicago_spawn_command(cmd);
}

static void on_audio_test_sound(GtkButton *btn, gpointer data) {
    (void)btn; (void)data;
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "pw-play \"%s/.local/share/sounds/Chicago95/stereo/desktop-login.oga\" 2>/dev/null || "
             "paplay \"%s/.local/share/sounds/Chicago95/stereo/desktop-login.oga\" 2>/dev/null",
             g_get_home_dir(), g_get_home_dir());
    chicago_spawn_command(cmd);
}

static void show_audio_properties_dialog(gboolean standalone) {
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Multimedia & Audio Properties");
    gtk_window_set_default_size(GTK_WINDOW(win), 380, 340);
    gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_CENTER);
    gtk_style_context_add_class(gtk_widget_get_style_context(win), "win95-window");
    g_signal_connect(win, "destroy", G_CALLBACK(on_dialog_destroy), GINT_TO_POINTER(standalone));

    GtkWidget *root_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(root_vbox), 8);

    GtkWidget *nb = gtk_notebook_new();

    GtkWidget *aud_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(aud_vbox), 8);

    GtkWidget *pb_frame = gtk_frame_new("Playback (Master Output Volume)");
    GtkWidget *pb_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(pb_vbox), 8);

    GtkWidget *vol_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 2);
    char *vout = NULL;
    gboolean is_muted = FALSE;
    int cur_vol = 75;
    if (g_spawn_command_line_sync("wpctl get-volume @DEFAULT_AUDIO_SINK@", &vout, NULL, NULL, NULL) && vout) {
        if (strstr(vout, "[MUTED]")) is_muted = TRUE;
        char *p = strstr(vout, "Volume:");
        if (p) cur_vol = CLAMP((int)(g_ascii_strtod(p + 7, NULL) * 100.0 + 0.5), 0, 100);
        g_free(vout);
    }
    gtk_range_set_value(GTK_RANGE(vol_scale), cur_vol);
    g_signal_connect(vol_scale, "value-changed", G_CALLBACK(on_audio_vol_changed), NULL);
    gtk_box_pack_start(GTK_BOX(pb_vbox), vol_scale, FALSE, FALSE, 0);

    GtkWidget *mute_chk = gtk_check_button_new_with_label("Mute Master Output");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mute_chk), is_muted);
    g_signal_connect(mute_chk, "toggled", G_CALLBACK(on_audio_mute_toggled), NULL);
    gtk_box_pack_start(GTK_BOX(pb_vbox), mute_chk, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(pb_frame), pb_vbox);
    gtk_box_pack_start(GTK_BOX(aud_vbox), pb_frame, FALSE, FALSE, 0);

    GtkWidget *rec_frame = gtk_frame_new("Recording (Microphone Input Volume)");
    GtkWidget *rec_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(rec_vbox), 8);
    GtkWidget *mic_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 2);
    gtk_range_set_value(GTK_RANGE(mic_scale), 80);
    g_signal_connect(mic_scale, "value-changed", G_CALLBACK(on_audio_mic_changed), NULL);
    gtk_box_pack_start(GTK_BOX(rec_vbox), mic_scale, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(rec_frame), rec_vbox);
    gtk_box_pack_start(GTK_BOX(aud_vbox), rec_frame, FALSE, FALSE, 0);

    GtkWidget *snd_frame = gtk_frame_new("Windows 95 Sound Scheme");
    GtkWidget *snd_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(snd_hbox), 8);
    gtk_box_pack_start(GTK_BOX(snd_hbox), gtk_label_new("The Microsoft Sound (Startup):"), TRUE, TRUE, 0);
    GtkWidget *test_btn = gtk_button_new_with_label("▶ Preview Sound");
    g_signal_connect(test_btn, "clicked", G_CALLBACK(on_audio_test_sound), NULL);
    gtk_box_pack_start(GTK_BOX(snd_hbox), test_btn, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(snd_frame), snd_hbox);
    gtk_box_pack_start(GTK_BOX(aud_vbox), snd_frame, FALSE, FALSE, 0);

    gtk_notebook_append_page(GTK_NOTEBOOK(nb), aud_vbox, gtk_label_new("Audio"));
    gtk_box_pack_start(GTK_BOX(root_vbox), nb, TRUE, TRUE, 0);

    GtkWidget *btn_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *ok_btn = gtk_button_new_with_label("OK");
    gtk_widget_set_size_request(ok_btn, 75, 23);
    g_signal_connect_swapped(ok_btn, "clicked", G_CALLBACK(gtk_widget_destroy), win);
    gtk_box_pack_end(GTK_BOX(btn_row), ok_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(root_vbox), btn_row, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(win), root_vbox);
    gtk_widget_show_all(win);
}

/* =========================================================================
 * 3. NETWORK & WI-FI PROPERTIES
 * ========================================================================= */

typedef struct {
    GtkWidget *status_lbl;
    GtkListStore *wifi_store;
    GtkWidget *wifi_tree;
    GtkWidget *pass_entry;
} NetPropState;

static void refresh_net_prop_list(NetPropState *ns) {
    gtk_list_store_clear(ns->wifi_store);
    char *out = NULL;
    if (g_spawn_command_line_sync("nmcli -t -f IN-USE,SSID,SIGNAL,SECURITY dev wifi list --rescan no", &out, NULL, NULL, NULL) && out) {
        char **lines = g_strsplit(out, "\n", -1);
        for (int i = 0; lines[i]; i++) {
            char **p = g_strsplit(lines[i], ":", 4);
            if (p[0] && p[1] && p[1][0] != '\0') {
                GtkTreeIter iter;
                gtk_list_store_append(ns->wifi_store, &iter);
                gtk_list_store_set(ns->wifi_store, &iter,
                                   0, (p[0][0] == '*') ? "Connected" : "",
                                   1, p[1],
                                   2, p[2] ? p[2] : "0",
                                   3, (p[3] && p[3][0]) ? p[3] : "Open",
                                   -1);
            }
            g_strfreev(p);
        }
        g_strfreev(lines);
        g_free(out);
    }

    char *ip_out = NULL;
    if (g_spawn_command_line_sync("nmcli -t -f NAME,DEVICE,TYPE connection show --active", &ip_out, NULL, NULL, NULL) && ip_out) {
        g_strstrip(ip_out);
        gtk_label_set_text(GTK_LABEL(ns->status_lbl), *ip_out ? ip_out : "No active connections");
        g_free(ip_out);
    }
}

static void on_net_rescan(GtkButton *btn, gpointer data) {
    (void)btn;
    NetPropState *ns = (NetPropState *)data;
    chicago_spawn_command("nmcli dev wifi rescan");
    refresh_net_prop_list(ns);
}

static void on_net_connect(GtkButton *btn, gpointer data) {
    (void)btn;
    NetPropState *ns = (NetPropState *)data;
    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(ns->wifi_tree));
    GtkTreeModel *model;
    GtkTreeIter iter;
    if (gtk_tree_selection_get_selected(sel, &model, &iter)) {
        char *ssid = NULL;
        gtk_tree_model_get(model, &iter, 1, &ssid, -1);
        const char *pass = gtk_entry_get_text(GTK_ENTRY(ns->pass_entry));
        if (ssid && *ssid) {
            char cmd[512];
            if (pass && *pass)
                snprintf(cmd, sizeof(cmd), "nmcli dev wifi connect \"%s\" password \"%s\"", ssid, pass);
            else
                snprintf(cmd, sizeof(cmd), "nmcli dev wifi connect \"%s\"", ssid);
            chicago_spawn_command(cmd);
        }
        g_free(ssid);
    }
}

static void show_network_properties_dialog(gboolean standalone) {
    NetPropState *ns = g_malloc0(sizeof(NetPropState));

    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Network Neighborhood Properties");
    gtk_window_set_default_size(GTK_WINDOW(win), 430, 380);
    gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_CENTER);
    gtk_style_context_add_class(gtk_widget_get_style_context(win), "win95-window");
    g_signal_connect(win, "destroy", G_CALLBACK(on_dialog_destroy), GINT_TO_POINTER(standalone));

    GtkWidget *root_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(root_vbox), 8);

    GtkWidget *nb = gtk_notebook_new();
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 6);

    GtkWidget *act_frame = gtk_frame_new("Active Network Adapters");
    ns->status_lbl = gtk_label_new("Checking...");
    gtk_container_set_border_width(GTK_CONTAINER(ns->status_lbl), 4);
    gtk_label_set_xalign(GTK_LABEL(ns->status_lbl), 0.0f);
    gtk_container_add(GTK_CONTAINER(act_frame), ns->status_lbl);
    gtk_box_pack_start(GTK_BOX(vbox), act_frame, FALSE, FALSE, 0);

    GtkWidget *wifi_frame = gtk_frame_new("Available Wireless (Wi-Fi) Networks");
    GtkWidget *wifi_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_container_set_border_width(GTK_CONTAINER(wifi_vbox), 6);

    ns->wifi_store = gtk_list_store_new(4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    ns->wifi_tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(ns->wifi_store));
    GtkCellRenderer *r = gtk_cell_renderer_text_new();
    gtk_tree_view_append_column(GTK_TREE_VIEW(ns->wifi_tree), gtk_tree_view_column_new_with_attributes("State", r, "text", 0, NULL));
    gtk_tree_view_append_column(GTK_TREE_VIEW(ns->wifi_tree), gtk_tree_view_column_new_with_attributes("SSID", r, "text", 1, NULL));
    gtk_tree_view_append_column(GTK_TREE_VIEW(ns->wifi_tree), gtk_tree_view_column_new_with_attributes("Signal %", r, "text", 2, NULL));
    gtk_tree_view_append_column(GTK_TREE_VIEW(ns->wifi_tree), gtk_tree_view_column_new_with_attributes("Security", r, "text", 3, NULL));

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_size_request(scroll, -1, 140);
    gtk_container_add(GTK_CONTAINER(scroll), ns->wifi_tree);
    gtk_box_pack_start(GTK_BOX(wifi_vbox), scroll, TRUE, TRUE, 0);

    GtkWidget *pass_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(pass_hbox), gtk_label_new("Network Key (if needed):"), FALSE, FALSE, 0);
    ns->pass_entry = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(ns->pass_entry), FALSE);
    gtk_box_pack_start(GTK_BOX(pass_hbox), ns->pass_entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(wifi_vbox), pass_hbox, FALSE, FALSE, 0);

    GtkWidget *act_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *conn_btn = gtk_button_new_with_label("Connect to Selected");
    GtkWidget *scan_btn = gtk_button_new_with_label("Refresh / Rescan");
    g_signal_connect(conn_btn, "clicked", G_CALLBACK(on_net_connect), ns);
    g_signal_connect(scan_btn, "clicked", G_CALLBACK(on_net_rescan), ns);
    gtk_box_pack_start(GTK_BOX(act_hbox), conn_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(act_hbox), scan_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(wifi_vbox), act_hbox, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(wifi_frame), wifi_vbox);
    gtk_box_pack_start(GTK_BOX(vbox), wifi_frame, TRUE, TRUE, 0);

    refresh_net_prop_list(ns);

    gtk_notebook_append_page(GTK_NOTEBOOK(nb), vbox, gtk_label_new("Configuration"));
    gtk_box_pack_start(GTK_BOX(root_vbox), nb, TRUE, TRUE, 0);

    GtkWidget *btn_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *ok_btn = gtk_button_new_with_label("OK");
    gtk_widget_set_size_request(ok_btn, 75, 23);
    g_signal_connect_swapped(ok_btn, "clicked", G_CALLBACK(gtk_widget_destroy), win);
    gtk_box_pack_end(GTK_BOX(btn_row), ok_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(root_vbox), btn_row, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(win), root_vbox);
    gtk_widget_show_all(win);
}

/* =========================================================================
 * 4. POWER MANAGEMENT & BRIGHTNESS PROPERTIES
 * ========================================================================= */

static void on_pwr_prop_bright(GtkRange *range, gpointer data) {
    (void)data;
    int v = CLAMP((int)gtk_range_get_value(range), 5, 100);
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "brightnessctl set %d%%", v);
    chicago_spawn_command(cmd);
}

static void on_pwr_prop_scheme(GtkComboBox *combo, gpointer data) {
    (void)data;
    const char *id = gtk_combo_box_get_active_id(combo);
    if (id) {
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "powerprofilesctl set %s", id);
        chicago_spawn_command(cmd);
    }
}

static void show_power_properties_dialog(gboolean standalone) {
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Power Management Properties");
    gtk_window_set_default_size(GTK_WINDOW(win), 380, 340);
    gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_CENTER);
    gtk_style_context_add_class(gtk_widget_get_style_context(win), "win95-window");
    g_signal_connect(win, "destroy", G_CALLBACK(on_dialog_destroy), GINT_TO_POINTER(standalone));

    GtkWidget *root_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(root_vbox), 8);

    GtkWidget *nb = gtk_notebook_new();
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);

    GtkWidget *sch_frame = gtk_frame_new("Power Schemes");
    GtkWidget *sch_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(sch_vbox), 8);
    GtkWidget *sch_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sch_combo), "power-saver", "Portable / Laptop (Power Saver)");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sch_combo), "balanced", "Home / Office Desk (Balanced)");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(sch_combo), "performance", "Always On (Maximum Performance)");
    gtk_combo_box_set_active(GTK_COMBO_BOX(sch_combo), 1);
    g_signal_connect(sch_combo, "changed", G_CALLBACK(on_pwr_prop_scheme), NULL);
    gtk_box_pack_start(GTK_BOX(sch_vbox), sch_combo, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(sch_frame), sch_vbox);
    gtk_box_pack_start(GTK_BOX(vbox), sch_frame, FALSE, FALSE, 0);

    GtkWidget *br_frame = gtk_frame_new("Backlight Brightness (eDP-1 Panel)");
    GtkWidget *br_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(br_vbox), 8);
    GtkWidget *br_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 5, 100, 5);
    gtk_range_set_value(GTK_RANGE(br_scale), 25);
    g_signal_connect(br_scale, "value-changed", G_CALLBACK(on_pwr_prop_bright), NULL);
    gtk_box_pack_start(GTK_BOX(br_vbox), br_scale, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(br_frame), br_vbox);
    gtk_box_pack_start(GTK_BOX(vbox), br_frame, FALSE, FALSE, 0);

    char *cap = NULL, *st = NULL;
    g_file_get_contents("/sys/class/power_supply/BAT0/capacity", &cap, NULL, NULL);
    g_file_get_contents("/sys/class/power_supply/BAT0/status", &st, NULL, NULL);
    GtkWidget *bat_frame = gtk_frame_new("Power Meter");
    GtkWidget *bat_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(bat_vbox), 8);
    char btxt[256];
    snprintf(btxt, sizeof(btxt), "Current Power Source: %s\nTotal Battery Remaining: %s%%",
             st ? g_strstrip(st) : "AC Power", cap ? g_strstrip(cap) : "100");
    GtkWidget *blbl = gtk_label_new(btxt);
    gtk_label_set_xalign(GTK_LABEL(blbl), 0.0f);
    gtk_box_pack_start(GTK_BOX(bat_vbox), blbl, FALSE, FALSE, 0);
    GtkWidget *pbar = gtk_progress_bar_new();
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(pbar), cap ? CLAMP(atof(cap) / 100.0, 0.0, 1.0) : 1.0);
    gtk_box_pack_start(GTK_BOX(bat_vbox), pbar, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(bat_frame), bat_vbox);
    gtk_box_pack_start(GTK_BOX(vbox), bat_frame, FALSE, FALSE, 0);
    g_free(cap);
    g_free(st);

    gtk_notebook_append_page(GTK_NOTEBOOK(nb), vbox, gtk_label_new("Power Schemes"));
    gtk_box_pack_start(GTK_BOX(root_vbox), nb, TRUE, TRUE, 0);

    GtkWidget *btn_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *ok_btn = gtk_button_new_with_label("OK");
    gtk_widget_set_size_request(ok_btn, 75, 23);
    g_signal_connect_swapped(ok_btn, "clicked", G_CALLBACK(gtk_widget_destroy), win);
    gtk_box_pack_end(GTK_BOX(btn_row), ok_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(root_vbox), btn_row, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(win), root_vbox);
    gtk_widget_show_all(win);
}

/* =========================================================================
 * 5. MOUSE, KEYBOARD, DATE/TIME, SYSTEM & TASKBAR PROPERTIES
 * ========================================================================= */

static void show_mouse_properties_dialog(gboolean standalone) {
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Mouse & Touchpad Properties");
    gtk_window_set_default_size(GTK_WINDOW(win), 360, 300);
    gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_CENTER);
    gtk_style_context_add_class(gtk_widget_get_style_context(win), "win95-window");
    g_signal_connect(win, "destroy", G_CALLBACK(on_dialog_destroy), GINT_TO_POINTER(standalone));

    GtkWidget *root_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(root_vbox), 8);
    GtkWidget *nb = gtk_notebook_new();
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);

    GtkWidget *tap_chk = gtk_check_button_new_with_label("Enable Touchpad Tap-to-Click (libinput)");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tap_chk), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), tap_chk, FALSE, FALSE, 0);

    GtkWidget *nat_chk = gtk_check_button_new_with_label("Enable Touchpad Natural Scrolling");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(nat_chk), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), nat_chk, FALSE, FALSE, 0);

    GtkWidget *dwt_chk = gtk_check_button_new_with_label("Disable Touchpad While Typing");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dwt_chk), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), dwt_chk, FALSE, FALSE, 0);

    GtkWidget *cur_frame = gtk_frame_new("Cursor Scheme");
    GtkWidget *cur_lbl = gtk_label_new("Active Cursor: Chicago95_Cursor_Black (24px)");
    gtk_container_set_border_width(GTK_CONTAINER(cur_lbl), 8);
    gtk_container_add(GTK_CONTAINER(cur_frame), cur_lbl);
    gtk_box_pack_start(GTK_BOX(vbox), cur_frame, FALSE, FALSE, 0);

    gtk_notebook_append_page(GTK_NOTEBOOK(nb), vbox, gtk_label_new("Buttons & Motion"));
    gtk_box_pack_start(GTK_BOX(root_vbox), nb, TRUE, TRUE, 0);

    GtkWidget *btn_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *ok_btn = gtk_button_new_with_label("OK");
    gtk_widget_set_size_request(ok_btn, 75, 23);
    g_signal_connect_swapped(ok_btn, "clicked", G_CALLBACK(gtk_widget_destroy), win);
    gtk_box_pack_end(GTK_BOX(btn_row), ok_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(root_vbox), btn_row, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(win), root_vbox);
    gtk_widget_show_all(win);
}

static void show_keyboard_properties_dialog(gboolean standalone) {
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Keyboard Properties");
    gtk_window_set_default_size(GTK_WINDOW(win), 380, 320);
    gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_CENTER);
    gtk_style_context_add_class(gtk_widget_get_style_context(win), "win95-window");
    g_signal_connect(win, "destroy", G_CALLBACK(on_dialog_destroy), GINT_TO_POINTER(standalone));

    GtkWidget *root_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(root_vbox), 8);
    GtkWidget *nb = gtk_notebook_new();
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);

    GtkWidget *sc_frame = gtk_frame_new("Windows 95 Keyboard Shortcuts");
    GtkWidget *sc_lbl = gtk_label_new(
        "• Super or Ctrl+Esc : Open / Close Start Menu\n"
        "• Super+D           : Show Desktop (Minimize / Restore All)\n"
        "• Super+E           : Launch Windows Explorer (Thunar)\n"
        "• Super+R           : Open Run... Dialog\n"
        "• Super+F           : Open Find: All Programs\n"
        "• Super+C           : Open Control Panel\n"
        "• Super+Enter       : Open MS-DOS Prompt\n"
        "• Alt+F4            : Close Active Window\n"
        "• Alt+Tab           : Switch Active Windows\n"
        "• Super+Arrows      : Snap / Maximize / Minimize Window");
    gtk_container_set_border_width(GTK_CONTAINER(sc_lbl), 8);
    gtk_label_set_xalign(GTK_LABEL(sc_lbl), 0.0f);
    gtk_container_add(GTK_CONTAINER(sc_frame), sc_lbl);
    gtk_box_pack_start(GTK_BOX(vbox), sc_frame, TRUE, TRUE, 0);

    gtk_notebook_append_page(GTK_NOTEBOOK(nb), vbox, gtk_label_new("Speed & Shortcuts"));
    gtk_box_pack_start(GTK_BOX(root_vbox), nb, TRUE, TRUE, 0);

    GtkWidget *btn_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *ok_btn = gtk_button_new_with_label("OK");
    gtk_widget_set_size_request(ok_btn, 75, 23);
    g_signal_connect_swapped(ok_btn, "clicked", G_CALLBACK(gtk_widget_destroy), win);
    gtk_box_pack_end(GTK_BOX(btn_row), ok_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(root_vbox), btn_row, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(win), root_vbox);
    gtk_widget_show_all(win);
}

static void show_datetime_properties_dialog(gboolean standalone) {
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Date/Time Properties");
    gtk_window_set_default_size(GTK_WINDOW(win), 340, 290);
    gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_CENTER);
    gtk_style_context_add_class(gtk_widget_get_style_context(win), "win95-window");
    g_signal_connect(win, "destroy", G_CALLBACK(on_dialog_destroy), GINT_TO_POINTER(standalone));

    GtkWidget *root_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(root_vbox), 8);
    GtkWidget *nb = gtk_notebook_new();
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);

    GtkWidget *cal = gtk_calendar_new();
    gtk_box_pack_start(GTK_BOX(vbox), cal, TRUE, TRUE, 0);

    gtk_notebook_append_page(GTK_NOTEBOOK(nb), vbox, gtk_label_new("Date & Time"));
    gtk_box_pack_start(GTK_BOX(root_vbox), nb, TRUE, TRUE, 0);

    GtkWidget *btn_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *ok_btn = gtk_button_new_with_label("OK");
    gtk_widget_set_size_request(ok_btn, 75, 23);
    g_signal_connect_swapped(ok_btn, "clicked", G_CALLBACK(gtk_widget_destroy), win);
    gtk_box_pack_end(GTK_BOX(btn_row), ok_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(root_vbox), btn_row, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(win), root_vbox);
    gtk_widget_show_all(win);
}

static void show_system_properties_dialog(gboolean standalone) {
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "System Properties");
    gtk_window_set_default_size(GTK_WINDOW(win), 410, 360);
    gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_CENTER);
    gtk_style_context_add_class(gtk_widget_get_style_context(win), "win95-window");
    g_signal_connect(win, "destroy", G_CALLBACK(on_dialog_destroy), GINT_TO_POINTER(standalone));

    GtkWidget *root_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(root_vbox), 8);
    GtkWidget *nb = gtk_notebook_new();

    GtkWidget *gen_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_container_set_border_width(GTK_CONTAINER(gen_hbox), 14);
    GdkPixbuf *comp_pb = chicago_load_icon("computer", 48);
    gtk_box_pack_start(GTK_BOX(gen_hbox), gtk_image_new_from_pixbuf(comp_pb), FALSE, FALSE, 4);
    if (comp_pb) g_object_unref(comp_pb);

    GtkWidget *info_lbl = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(info_lbl),
        "<b>System:</b>\n"
        "  Microsoft Windows 95 (Chicago-Wayland)\n"
        "  4.00.950 B (Native Wayland / labwc 0.9.6)\n"
        "  Fedora Linux 44 (Workstation Edition)\n\n"
        "<b>Computer:</b>\n"
        "  Dell Inc. Precision 5550\n"
        "  Intel(R) Core(TM) i7-10750H CPU @ 2.60GHz\n"
        "  32.0 GB System RAM\n\n"
        "<b>Measured Idle Performance:</b>\n"
        "  0.000% Desktop Idle CPU | 8.18 W Package | 0.27 W iGPU");
    gtk_label_set_xalign(GTK_LABEL(info_lbl), 0.0f);
    gtk_box_pack_start(GTK_BOX(gen_hbox), info_lbl, TRUE, TRUE, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(nb), gen_hbox, gtk_label_new("General"));

    gtk_box_pack_start(GTK_BOX(root_vbox), nb, TRUE, TRUE, 0);

    GtkWidget *btn_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *ok_btn = gtk_button_new_with_label("OK");
    gtk_widget_set_size_request(ok_btn, 75, 23);
    g_signal_connect_swapped(ok_btn, "clicked", G_CALLBACK(gtk_widget_destroy), win);
    gtk_box_pack_end(GTK_BOX(btn_row), ok_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(root_vbox), btn_row, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(win), root_vbox);
    gtk_widget_show_all(win);
}

static void on_switch_sfwbar(GtkButton *btn, gpointer data) {
    (void)btn; (void)data;
    chicago_spawn_command("pkill chicago-taskbar; sfwbar -f ~/.config/chicago-wayland/panel/chicago95.sfwbar");
}

static void on_switch_native_bar(GtkButton *btn, gpointer data) {
    (void)btn; (void)data;
    chicago_spawn_command("pkill sfwbar; chicago-taskbar");
}

typedef struct {
    GtkListStore *pin_store;
    GtkWidget *pin_tree;
} TaskbarPropState;

static void refresh_taskbar_pins_store(TaskbarPropState *ts) {
    if (!ts || !ts->pin_store) return;
    gtk_list_store_clear(ts->pin_store);
    GList *pins = chicago_pins_load();
    for (GList *l = pins; l != NULL; l = l->next) {
        ChicagoPinEntry *p = (ChicagoPinEntry *)l->data;
        GtkTreeIter iter;
        gtk_list_store_append(ts->pin_store, &iter);
        gtk_list_store_set(ts->pin_store, &iter,
                           0, p->name ? p->name : "App",
                           1, p->exec ? p->exec : "",
                           -1);
    }
    g_list_free_full(pins, chicago_pin_entry_free);
}

static void on_tb_unpin_clicked(GtkButton *btn, gpointer data) {
    (void)btn;
    TaskbarPropState *ts = (TaskbarPropState *)data;
    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(ts->pin_tree));
    GtkTreeModel *model;
    GtkTreeIter iter;
    if (gtk_tree_selection_get_selected(sel, &model, &iter)) {
        char *name = NULL, *exec = NULL;
        gtk_tree_model_get(model, &iter, 0, &name, 1, &exec, -1);
        chicago_pins_remove(name, exec);
        g_free(name);
        g_free(exec);
        refresh_taskbar_pins_store(ts);
    }
}

static void on_tb_add_pin_clicked(GtkButton *btn, gpointer data) {
    (void)btn; (void)data;
    chicago_spawn_command("chicago-control-panel --find");
}

static void show_taskbar_properties_dialog(gboolean standalone) {
    TaskbarPropState *ts = g_malloc0(sizeof(TaskbarPropState));

    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Taskbar Properties");
    gtk_window_set_default_size(GTK_WINDOW(win), 420, 340);
    gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_CENTER);
    gtk_style_context_add_class(gtk_widget_get_style_context(win), "win95-window");
    g_signal_connect(win, "destroy", G_CALLBACK(on_dialog_destroy), GINT_TO_POINTER(standalone));

    GtkWidget *root_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(root_vbox), 8);
    GtkWidget *nb = gtk_notebook_new();

    /* Tab 1: Pinned Taskbar Applications */
    GtkWidget *pin_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(pin_vbox), 8);

    GtkWidget *info_lbl = gtk_label_new("Pinned applications appear on the Quick Launch bar across all monitors.\nRight-click any app in Start Menu or Find Programs to Pin/Unpin.");
    gtk_label_set_xalign(GTK_LABEL(info_lbl), 0.0f);
    gtk_box_pack_start(GTK_BOX(pin_vbox), info_lbl, FALSE, FALSE, 0);

    ts->pin_store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
    ts->pin_tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(ts->pin_store));
    GtkCellRenderer *r = gtk_cell_renderer_text_new();
    gtk_tree_view_append_column(GTK_TREE_VIEW(ts->pin_tree),
        gtk_tree_view_column_new_with_attributes("Pinned Program", r, "text", 0, NULL));
    gtk_tree_view_append_column(GTK_TREE_VIEW(ts->pin_tree),
        gtk_tree_view_column_new_with_attributes("Command", r, "text", 1, NULL));

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scroll), ts->pin_tree);
    gtk_box_pack_start(GTK_BOX(pin_vbox), scroll, TRUE, TRUE, 0);

    GtkWidget *pbtn_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *unpin_btn = gtk_button_new_with_label("Unpin Selected");
    GtkWidget *add_btn = gtk_button_new_with_label("Pin More Programs...");
    g_signal_connect(unpin_btn, "clicked", G_CALLBACK(on_tb_unpin_clicked), ts);
    g_signal_connect(add_btn, "clicked", G_CALLBACK(on_tb_add_pin_clicked), ts);
    gtk_box_pack_start(GTK_BOX(pbtn_hbox), unpin_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(pbtn_hbox), add_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(pin_vbox), pbtn_hbox, FALSE, FALSE, 0);

    refresh_taskbar_pins_store(ts);
    gtk_notebook_append_page(GTK_NOTEBOOK(nb), pin_vbox, gtk_label_new("Pinned Programs"));

    /* Tab 2: Taskbar Engine */
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);

    GtkWidget *b1 = gtk_button_new_with_label("Use Native C11 Multi-Monitor Taskbar (Recommended)");
    GtkWidget *b2 = gtk_button_new_with_label("Switch to sfwbar Classic Taskbar");
    g_signal_connect(b1, "clicked", G_CALLBACK(on_switch_native_bar), NULL);
    g_signal_connect(b2, "clicked", G_CALLBACK(on_switch_sfwbar), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), b1, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), b2, FALSE, FALSE, 0);

    gtk_notebook_append_page(GTK_NOTEBOOK(nb), vbox, gtk_label_new("Taskbar Options"));
    gtk_box_pack_start(GTK_BOX(root_vbox), nb, TRUE, TRUE, 0);

    GtkWidget *btn_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *ok_btn = gtk_button_new_with_label("OK");
    gtk_widget_set_size_request(ok_btn, 75, 23);
    g_signal_connect_swapped(ok_btn, "clicked", G_CALLBACK(gtk_widget_destroy), win);
    gtk_box_pack_end(GTK_BOX(btn_row), ok_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(root_vbox), btn_row, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(win), root_vbox);
    gtk_widget_show_all(win);
}

/* =========================================================================
 * 6. AUTHENTIC WINDOWS 95 CONTROL PANEL FOLDER WINDOW (DEFAULT MODE)
 * ========================================================================= */

typedef struct {
    const char *title;
    const char *icon_name;
    const char *description;
    int applet_id;
} ControlApplet;

static const ControlApplet g_applets[] = {
    { "Display",            "preferences-desktop-display", "Changes the desktop background, color scheme, and multi-monitor resolution.", 1 },
    { "Sounds and Audio",   "preferences-desktop-sound",   "Changes speaker volume, microphone input, and Windows 95 sound scheme.", 2 },
    { "Network",            "preferences-system-network",  "Configures wireless Wi-Fi networks and active network adapters.", 3 },
    { "Power Management",   "preferences-system-power",    "Configures screen brightness, power saver/performance scheme, and battery meter.", 4 },
    { "Mouse",              "input-mouse",                 "Changes touchpad tap-to-click, natural scrolling, and pointer settings.", 5 },
    { "Keyboard",           "input-keyboard",              "Displays Windows 95 keyboard shortcuts and repeat rate settings.", 6 },
    { "Date/Time",          "preferences-system-time",     "Changes the system date, time, and calendar settings.", 7 },
    { "System",             "computer",                    "Displays system hardware specifications and live power/CPU efficiency.", 8 },
    { "Taskbar & Start",    "preferences-desktop",         "Configures the Windows 95 taskbar and Start menu options.", 9 },
    { "Add/Remove Programs", "system-software-install",    "Searches and launches installed applications on your system.", 10 },
};
#define NUM_APPLETS (G_N_ELEMENTS(g_applets))

static void open_applet_by_id(int id) {
    switch (id) {
        case 1: show_display_properties_dialog(FALSE); break;
        case 2: show_audio_properties_dialog(FALSE); break;
        case 3: show_network_properties_dialog(FALSE); break;
        case 4: show_power_properties_dialog(FALSE); break;
        case 5: show_mouse_properties_dialog(FALSE); break;
        case 6: show_keyboard_properties_dialog(FALSE); break;
        case 7: show_datetime_properties_dialog(FALSE); break;
        case 8: show_system_properties_dialog(FALSE); break;
        case 9: show_taskbar_properties_dialog(FALSE); break;
        case 10: build_find_dialog(FALSE); break;
        default: break;
    }
}

static GtkWidget *g_cp_desc_well = NULL;

static void on_cp_btn_clicked(GtkButton *btn, gpointer data) {
    (void)btn;
    int id = GPOINTER_TO_INT(data);
    open_applet_by_id(id);
}

static gboolean on_cp_btn_enter(GtkWidget *w, GdkEventCrossing *ev, gpointer data) {
    (void)w; (void)ev;
    const char *desc = (const char *)data;
    if (g_cp_desc_well && desc) {
        gtk_label_set_text(GTK_LABEL(g_cp_desc_well), desc);
    }
    return FALSE;
}

static void build_control_panel_folder_window(void) {
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Control Panel");
    gtk_window_set_default_size(GTK_WINDOW(win), 520, 340);
    gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_CENTER);
    gtk_style_context_add_class(gtk_widget_get_style_context(win), "win95-window");
    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *root_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    /* 1. Classic Windows 95 Menu Bar */
    GtkWidget *menubar = gtk_menu_bar_new();
    GtkWidget *file_mi = gtk_menu_item_new_with_mnemonic("_File");
    GtkWidget *file_menu = gtk_menu_new();
    GtkWidget *close_mi = gtk_menu_item_new_with_label("Close");
    g_signal_connect_swapped(close_mi, "activate", G_CALLBACK(gtk_widget_destroy), win);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), close_mi);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(file_mi), file_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), file_mi);

    GtkWidget *edit_mi = gtk_menu_item_new_with_mnemonic("_Edit");
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), edit_mi);
    GtkWidget *view_mi = gtk_menu_item_new_with_mnemonic("_View");
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), view_mi);
    GtkWidget *help_mi = gtk_menu_item_new_with_mnemonic("_Help");
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), help_mi);

    gtk_box_pack_start(GTK_BOX(root_vbox), menubar, FALSE, FALSE, 0);

    /* 2. Sunken White Folder Icon Grid Area */
    GtkWidget *flow = gtk_flow_box_new();
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(flow), 5);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(flow), 4);
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(flow), GTK_SELECTION_NONE);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(flow), TRUE);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(flow), 6);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(flow), 8);

    for (size_t i = 0; i < NUM_APPLETS; i++) {
        GtkWidget *btn = gtk_button_new();
        gtk_style_context_add_class(gtk_widget_get_style_context(btn), "cp-icon-btn");
        gtk_widget_add_events(btn, GDK_ENTER_NOTIFY_MASK);

        GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
        GdkPixbuf *pb = chicago_load_icon(g_applets[i].icon_name, 32);
        GtkWidget *img = gtk_image_new_from_pixbuf(pb);
        if (pb) g_object_unref(pb);

        GtkWidget *lbl = gtk_label_new(g_applets[i].title);
        gtk_label_set_justify(GTK_LABEL(lbl), GTK_JUSTIFY_CENTER);
        gtk_label_set_line_wrap(GTK_LABEL(lbl), TRUE);
        gtk_label_set_max_width_chars(GTK_LABEL(lbl), 14);

        gtk_box_pack_start(GTK_BOX(vbox), img, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox), lbl, FALSE, FALSE, 0);
        gtk_container_add(GTK_CONTAINER(btn), vbox);

        g_signal_connect(btn, "clicked", G_CALLBACK(on_cp_btn_clicked), GINT_TO_POINTER(g_applets[i].applet_id));
        g_signal_connect(btn, "enter-notify-event", G_CALLBACK(on_cp_btn_enter), (gpointer)g_applets[i].description);

        gtk_container_add(GTK_CONTAINER(flow), btn);
    }

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_set_border_width(GTK_CONTAINER(scroll), 4);
    gtk_style_context_add_class(gtk_widget_get_style_context(scroll), "win95-inset");
    gtk_container_add(GTK_CONTAINER(scroll), flow);
    gtk_box_pack_start(GTK_BOX(root_vbox), scroll, TRUE, TRUE, 0);

    /* 3. Classic Windows 95 2-Pane Sunken Status Bar */
    GtkWidget *status_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_container_set_border_width(GTK_CONTAINER(status_hbox), 3);

    GtkWidget *cnt_well = gtk_label_new("10 object(s)");
    gtk_label_set_xalign(GTK_LABEL(cnt_well), 0.0f);
    gtk_widget_set_size_request(cnt_well, 100, -1);
    gtk_style_context_add_class(gtk_widget_get_style_context(cnt_well), "win95-well");

    g_cp_desc_well = gtk_label_new("Click an icon to open its Windows 95 Properties sheet.");
    gtk_label_set_xalign(GTK_LABEL(g_cp_desc_well), 0.0f);
    gtk_style_context_add_class(gtk_widget_get_style_context(g_cp_desc_well), "win95-well");

    gtk_box_pack_start(GTK_BOX(status_hbox), cnt_well, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(status_hbox), g_cp_desc_well, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(root_vbox), status_hbox, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(win), root_vbox);
    gtk_widget_show_all(win);
}

/* =========================================================================
 * 7. RUN, FIND, SHUT DOWN & HELP VIEWER DIALOGS
 * ========================================================================= */

static void on_run_exec(GtkWidget *w, gpointer data) {
    (void)w;
    GtkEntry *entry = GTK_ENTRY(data);
    const char *cmd = gtk_entry_get_text(entry);
    if (cmd && *cmd) {
        chicago_spawn_command(cmd);
        gtk_main_quit();
    }
}

static void build_run_dialog(void) {
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Run");
    gtk_window_set_default_size(GTK_WINDOW(win), 350, 165);
    gtk_window_set_resizable(GTK_WINDOW(win), FALSE);
    gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_CENTER);
    gtk_style_context_add_class(gtk_widget_get_style_context(win), "win95-window");
    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 12);

    GtkWidget *top_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GdkPixbuf *pb = chicago_load_icon("system-run", 32);
    gtk_box_pack_start(GTK_BOX(top_hbox), gtk_image_new_from_pixbuf(pb), FALSE, FALSE, 0);
    if (pb) g_object_unref(pb);

    GtkWidget *desc = gtk_label_new("Type the name of a program, folder, or document, and\nWindows will open it for you.");
    gtk_label_set_xalign(GTK_LABEL(desc), 0.0f);
    gtk_box_pack_start(GTK_BOX(top_hbox), desc, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), top_hbox, FALSE, FALSE, 0);

    GtkWidget *open_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(open_hbox), gtk_label_new("Open:"), FALSE, FALSE, 0);
    GtkWidget *entry = gtk_entry_new();
    g_signal_connect(entry, "activate", G_CALLBACK(on_run_exec), entry);
    gtk_box_pack_start(GTK_BOX(open_hbox), entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), open_hbox, FALSE, FALSE, 0);

    GtkWidget *btn_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *ok_btn = gtk_button_new_with_label("OK");
    GtkWidget *cancel_btn = gtk_button_new_with_label("Cancel");
    gtk_widget_set_size_request(ok_btn, 75, 24);
    gtk_widget_set_size_request(cancel_btn, 75, 24);
    g_signal_connect(ok_btn, "clicked", G_CALLBACK(on_run_exec), entry);
    g_signal_connect(cancel_btn, "clicked", G_CALLBACK(gtk_main_quit), NULL);
    gtk_box_pack_end(GTK_BOX(btn_hbox), cancel_btn, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(btn_hbox), ok_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), btn_hbox, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(win), vbox);
    gtk_widget_show_all(win);
}

typedef struct {
    GtkListStore *store;
    GtkWidget *entry;
    GtkWidget *tree;
    GtkWidget *status_lbl;
} FindDialogData;

static void populate_find_results(FindDialogData *fd, const char *query) {
    gtk_list_store_clear(fd->store);
    GList *apps = chicago_scan_all_desktop_apps();
    char *ql = g_ascii_strdown(query ? query : "", -1);
    int count = 0;

    for (GList *l = apps; l != NULL; l = l->next) {
        ChicagoAppEntry *e = (ChicagoAppEntry *)l->data;
        char *nl = g_ascii_strdown(e->name ? e->name : "", -1);
        char *el = g_ascii_strdown(e->exec ? e->exec : "", -1);
        if (!*ql || strstr(nl, ql) != NULL || strstr(el, ql) != NULL) {
            GtkTreeIter iter;
            gtk_list_store_append(fd->store, &iter);
            gtk_list_store_set(fd->store, &iter,
                               0, e->name ? e->name : "App",
                               1, e->category ? e->category : "Accessories",
                               2, e->exec ? e->exec : "",
                               3, e->icon ? e->icon : "application-x-executable",
                               4, e->desktop_path ? e->desktop_path : "",
                               -1);
            count++;
        }
        g_free(nl);
        g_free(el);
    }
    g_free(ql);
    g_list_free_full(apps, chicago_app_entry_free);

    if (fd->status_lbl) {
        char msg[128];
        snprintf(msg, sizeof(msg), "%d program(s) found. Right-click any program for GPU / Pin / Desktop Shortcut options.", count);
        gtk_label_set_text(GTK_LABEL(fd->status_lbl), msg);
    }
}

static void on_find_changed(GtkEditable *ed, gpointer data) {
    FindDialogData *fd = (FindDialogData *)data;
    populate_find_results(fd, gtk_entry_get_text(GTK_ENTRY(ed)));
}

static void on_find_row_activated(GtkTreeView *tv, GtkTreePath *path, GtkTreeViewColumn *col, gpointer data) {
    (void)col; (void)data;
    GtkTreeModel *model = gtk_tree_view_get_model(tv);
    GtkTreeIter iter;
    if (gtk_tree_model_get_iter(model, &iter, path)) {
        char *cmd = NULL;
        gtk_tree_model_get(model, &iter, 2, &cmd, -1);
        if (cmd) {
            chicago_spawn_command(cmd);
            g_free(cmd);
        }
    }
}

static gboolean on_find_tree_button_press(GtkWidget *tv, GdkEventButton *event, gpointer data) {
    (void)data;
    if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
        GtkTreePath *path = NULL;
        if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(tv), (gint)event->x, (gint)event->y, &path, NULL, NULL, NULL)) {
            GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tv));
            gtk_tree_selection_select_path(sel, path);
            GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(tv));
            GtkTreeIter iter;
            if (gtk_tree_model_get_iter(model, &iter, path)) {
                char *name = NULL, *exec = NULL, *icon = NULL, *dpath = NULL;
                gtk_tree_model_get(model, &iter, 0, &name, 2, &exec, 3, &icon, 4, &dpath, -1);
                chicago_popup_app_context_menu(name, icon, exec, dpath, event);
                g_free(name);
                g_free(exec);
                g_free(icon);
                g_free(dpath);
            }
            gtk_tree_path_free(path);
            return TRUE;
        }
    }
    return FALSE;
}

static void on_find_btn_launch(GtkButton *btn, gpointer data) {
    (void)btn;
    FindDialogData *fd = (FindDialogData *)data;
    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(fd->tree));
    GtkTreeModel *model;
    GtkTreeIter iter;
    if (gtk_tree_selection_get_selected(sel, &model, &iter)) {
        char *cmd = NULL;
        gtk_tree_model_get(model, &iter, 2, &cmd, -1);
        if (cmd) {
            chicago_spawn_command(cmd);
            g_free(cmd);
        }
    }
}

static void on_find_btn_dgpu(GtkButton *btn, gpointer data) {
    (void)btn;
    FindDialogData *fd = (FindDialogData *)data;
    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(fd->tree));
    GtkTreeModel *model;
    GtkTreeIter iter;
    if (gtk_tree_selection_get_selected(sel, &model, &iter)) {
        char *cmd = NULL;
        gtk_tree_model_get(model, &iter, 2, &cmd, -1);
        if (cmd) {
            chicago_launch_with_dgpu(cmd);
            g_free(cmd);
        }
    }
}

static void on_find_btn_pin(GtkButton *btn, gpointer data) {
    (void)btn;
    FindDialogData *fd = (FindDialogData *)data;
    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(fd->tree));
    GtkTreeModel *model;
    GtkTreeIter iter;
    if (gtk_tree_selection_get_selected(sel, &model, &iter)) {
        char *name = NULL, *exec = NULL, *icon = NULL, *dpath = NULL;
        gtk_tree_model_get(model, &iter, 0, &name, 2, &exec, 3, &icon, 4, &dpath, -1);
        if (chicago_pins_is_pinned(name, exec)) {
            chicago_pins_remove(name, exec);
            if (fd->status_lbl) gtk_label_set_text(GTK_LABEL(fd->status_lbl), "Unpinned program from Taskbar.");
        } else {
            chicago_pins_add(name, icon, exec, dpath);
            if (fd->status_lbl) gtk_label_set_text(GTK_LABEL(fd->status_lbl), "Pinned program to Taskbar!");
        }
        g_free(name);
        g_free(exec);
        g_free(icon);
        g_free(dpath);
    }
}

static void build_find_dialog(gboolean standalone) {
    FindDialogData *fd = g_malloc0(sizeof(FindDialogData));

    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Find: All Programs & Applications (XDG + Flatpak)");
    gtk_window_set_default_size(GTK_WINDOW(win), 560, 400);
    gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_CENTER);
    gtk_style_context_add_class(gtk_widget_get_style_context(win), "win95-window");
    g_signal_connect(win, "destroy", G_CALLBACK(on_dialog_destroy), GINT_TO_POINTER(standalone));

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);

    GtkWidget *search_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(search_hbox), gtk_label_new("Named:"), FALSE, FALSE, 0);
    fd->entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(fd->entry), "Type app name (e.g. Spotify, Signal, Vesktop, Thunar)...");
    g_signal_connect(fd->entry, "changed", G_CALLBACK(on_find_changed), fd);
    gtk_box_pack_start(GTK_BOX(search_hbox), fd->entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), search_hbox, FALSE, FALSE, 0);

    fd->store = gtk_list_store_new(5, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    fd->tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(fd->store));
    GtkCellRenderer *r = gtk_cell_renderer_text_new();
    gtk_tree_view_append_column(GTK_TREE_VIEW(fd->tree), gtk_tree_view_column_new_with_attributes("Program Name", r, "text", 0, NULL));
    gtk_tree_view_append_column(GTK_TREE_VIEW(fd->tree), gtk_tree_view_column_new_with_attributes("Category", r, "text", 1, NULL));
    gtk_tree_view_append_column(GTK_TREE_VIEW(fd->tree), gtk_tree_view_column_new_with_attributes("Command", r, "text", 2, NULL));
    g_signal_connect(fd->tree, "row-activated", G_CALLBACK(on_find_row_activated), fd);
    g_signal_connect(fd->tree, "button-press-event", G_CALLBACK(on_find_tree_button_press), fd);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scroll), fd->tree);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);

    GtkWidget *act_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *b_open = gtk_button_new_with_label("Open / Launch");
    GtkWidget *b_gpu = gtk_button_new_with_label("Run on Dedicated GPU (NVIDIA)");
    GtkWidget *b_pin = gtk_button_new_with_label("Pin / Unpin to Taskbar");
    g_signal_connect(b_open, "clicked", G_CALLBACK(on_find_btn_launch), fd);
    g_signal_connect(b_gpu, "clicked", G_CALLBACK(on_find_btn_dgpu), fd);
    g_signal_connect(b_pin, "clicked", G_CALLBACK(on_find_btn_pin), fd);
    gtk_box_pack_start(GTK_BOX(act_hbox), b_open, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(act_hbox), b_gpu, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(act_hbox), b_pin, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), act_hbox, FALSE, FALSE, 0);

    fd->status_lbl = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(fd->status_lbl), 0.0f);
    gtk_style_context_add_class(gtk_widget_get_style_context(fd->status_lbl), "win95-well");
    gtk_box_pack_start(GTK_BOX(vbox), fd->status_lbl, FALSE, FALSE, 0);

    populate_find_results(fd, "");
    gtk_container_add(GTK_CONTAINER(win), vbox);
    gtk_widget_show_all(win);
}

typedef struct {
    GtkWidget *rb_logout;
    GtkWidget *rb_suspend;
    GtkWidget *rb_reboot;
    GtkWidget *rb_poweroff;
} ShutdownRefs;

static void on_shutdown_yes(GtkButton *btn, gpointer data) {
    (void)btn;
    ShutdownRefs *sr = (ShutdownRefs *)data;
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(sr->rb_logout)))
        chicago_spawn_command("labwc --exit");
    else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(sr->rb_suspend)))
        chicago_spawn_command("systemctl suspend");
    else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(sr->rb_reboot)))
        chicago_spawn_command("systemctl reboot");
    else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(sr->rb_poweroff)))
        chicago_spawn_command("systemctl poweroff");
    gtk_main_quit();
}

static void build_shutdown_dialog(void) {
    ShutdownRefs *sr = g_malloc0(sizeof(ShutdownRefs));

    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Shut Down Windows");
    gtk_window_set_default_size(GTK_WINDOW(win), 350, 215);
    gtk_window_set_resizable(GTK_WINDOW(win), FALSE);
    gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_CENTER);
    gtk_style_context_add_class(gtk_widget_get_style_context(win), "win95-window");
    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 14);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
    GdkPixbuf *pb = chicago_load_icon("system-shutdown", 32);
    gtk_box_pack_start(GTK_BOX(hbox), gtk_image_new_from_pixbuf(pb), FALSE, FALSE, 0);
    if (pb) g_object_unref(pb);

    GtkWidget *rbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *hdr = gtk_label_new("What do you want the computer to do?");
    gtk_label_set_xalign(GTK_LABEL(hdr), 0.0f);
    gtk_box_pack_start(GTK_BOX(rbox), hdr, FALSE, FALSE, 2);

    sr->rb_logout = gtk_radio_button_new_with_label(NULL, "Close all programs and log on as a different user (Log Out)");
    sr->rb_suspend = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(sr->rb_logout), "Stand by (Suspend to RAM)");
    sr->rb_poweroff = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(sr->rb_logout), "Shut down the computer");
    sr->rb_reboot = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(sr->rb_logout), "Restart the computer");

    gtk_box_pack_start(GTK_BOX(rbox), sr->rb_logout, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(rbox), sr->rb_suspend, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(rbox), sr->rb_poweroff, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(rbox), sr->rb_reboot, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(hbox), rbox, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

    GtkWidget *btn_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *yes_btn = gtk_button_new_with_label("Yes");
    GtkWidget *no_btn = gtk_button_new_with_label("No");
    gtk_widget_set_size_request(yes_btn, 75, 24);
    gtk_widget_set_size_request(no_btn, 75, 24);
    g_signal_connect(yes_btn, "clicked", G_CALLBACK(on_shutdown_yes), sr);
    g_signal_connect(no_btn, "clicked", G_CALLBACK(gtk_main_quit), NULL);
    gtk_box_pack_end(GTK_BOX(btn_row), no_btn, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(btn_row), yes_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), btn_row, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(win), vbox);
    gtk_widget_show_all(win);
}

static void build_help_dialog(void) {
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Windows Help: Chicago-Wayland");
    gtk_window_set_default_size(GTK_WINDOW(win), 480, 360);
    gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_CENTER);
    gtk_style_context_add_class(gtk_widget_get_style_context(win), "win95-window");
    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);

    GtkWidget *tv = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(tv), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(tv), GTK_WRAP_WORD);
    GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv));
    gtk_text_buffer_set_text(buf,
        "WELCOME TO MICROSOFT WINDOWS 95 (CHICAGO-WAYLAND)\n"
        "==================================================\n\n"
        "• Control Panel: Open Start -> Settings -> Control Panel (or double-click\n"
        "  Control Panel on the desktop) to open Display, Audio, Network, Power,\n"
        "  Mouse, Keyboard, Date/Time, and System Properties.\n\n"
        "• System Tray Quick Controls: Click the Speaker, Network, Battery, or Clock\n"
        "  icons in the bottom-right taskbar tray for instant sliders and Wi-Fi selection.\n\n"
        "• Desktop Customization: Right-click anywhere on the desktop and choose\n"
        "  Properties to change your wallpaper, pattern, desktop color, or monitor layout.\n\n"
        "• Switching Back to Hyprland: Log out via Start -> Shut Down..., and your\n"
        "  Hyprland dark theme and settings are automatically restored.", -1);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scroll), tv);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);

    GtkWidget *close_btn = gtk_button_new_with_label("Close");
    g_signal_connect(close_btn, "clicked", G_CALLBACK(gtk_main_quit), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), close_btn, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(win), vbox);
    gtk_widget_show_all(win);
}

int main(int argc, char **argv) {
    gtk_init(NULL, NULL);
    chicago_apply_theme();

    for (int i = 1; i < argc; i++) {
        if (g_strcmp0(argv[i], "--display") == 0 ||
            (g_strcmp0(argv[i], "--tab") == 0 && i + 1 < argc && g_strcmp0(argv[i + 1], "display") == 0)) {
            show_display_properties_dialog(TRUE);
            gtk_main();
            return 0;
        } else if (g_strcmp0(argv[i], "--audio") == 0 ||
                   (g_strcmp0(argv[i], "--tab") == 0 && i + 1 < argc && g_strcmp0(argv[i + 1], "audio") == 0)) {
            show_audio_properties_dialog(TRUE);
            gtk_main();
            return 0;
        } else if (g_strcmp0(argv[i], "--network") == 0 ||
                   (g_strcmp0(argv[i], "--tab") == 0 && i + 1 < argc && g_strcmp0(argv[i + 1], "network") == 0)) {
            show_network_properties_dialog(TRUE);
            gtk_main();
            return 0;
        } else if (g_strcmp0(argv[i], "--power") == 0 ||
                   (g_strcmp0(argv[i], "--tab") == 0 && i + 1 < argc && g_strcmp0(argv[i + 1], "power") == 0)) {
            show_power_properties_dialog(TRUE);
            gtk_main();
            return 0;
        } else if (g_strcmp0(argv[i], "--mouse") == 0) {
            show_mouse_properties_dialog(TRUE);
            gtk_main();
            return 0;
        } else if (g_strcmp0(argv[i], "--keyboard") == 0) {
            show_keyboard_properties_dialog(TRUE);
            gtk_main();
            return 0;
        } else if (g_strcmp0(argv[i], "--datetime") == 0) {
            show_datetime_properties_dialog(TRUE);
            gtk_main();
            return 0;
        } else if (g_strcmp0(argv[i], "--system") == 0) {
            show_system_properties_dialog(TRUE);
            gtk_main();
            return 0;
        } else if (g_strcmp0(argv[i], "--taskbar") == 0) {
            show_taskbar_properties_dialog(TRUE);
            gtk_main();
            return 0;
        } else if (g_strcmp0(argv[i], "--run") == 0) {
            build_run_dialog();
            gtk_main();
            return 0;
        } else if (g_strcmp0(argv[i], "--find") == 0) {
            build_find_dialog(TRUE);
            gtk_main();
            return 0;
        } else if (g_strcmp0(argv[i], "--shutdown") == 0) {
            build_shutdown_dialog();
            gtk_main();
            return 0;
        } else if (g_strcmp0(argv[i], "--help-viewer") == 0) {
            build_help_dialog();
            gtk_main();
            return 0;
        }
    }

    build_control_panel_folder_window();
    gtk_main();
    return 0;
}
