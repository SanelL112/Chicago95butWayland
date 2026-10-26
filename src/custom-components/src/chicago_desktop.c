/*
 * chicago-desktop: Authentic Windows 95 Wayland Desktop Surface & Icon Manager
 *
 * Features:
 * - Multi-monitor wlr-layer-shell BACKGROUND surfaces (#008080 teal + optional patterns/wallpapers)
 * - Configurable icon monitor placement (All Monitors, or specific monitor via Display Properties)
 * - Authentic Chicago95 32x32 pixel icons + Helvetica 8pt bitmap font labels
 * - Single-click #000080 navy label box + dotted focus border, double-click / Enter launch
 * - Rubber-band selection rectangle & draggable icon positions
 * - Live inotify (GFileMonitor) on ~/Desktop and ~/.local/share/Trash/files
 * - SIGUSR1 live reload of desktop.conf (background color, pattern/wallpaper, icon monitor)
 * - Right-click context menu with "Properties" -> Win95 Display Properties
 */

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gtk-layer-shell/gtk-layer-shell.h>
#include <gio/gio.h>
#include <cairo.h>
#include <glib-unix.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/chicago_common.h"

#define MAX_DESKTOP_ICONS 64
#define MAX_MONITORS      8

typedef struct {
    char title[128];
    char icon_name[128];
    char command[512];
    int x;
    int y;
    gboolean selected;
    gboolean is_trash;
    GdkPixbuf *pixbuf;
} DesktopIcon;

typedef struct {
    GtkWidget *window;
    GtkWidget *drawing_area;
    GdkMonitor *monitor;
    char plug_name[64];
    gboolean show_icons;
} MonitorSurface;

typedef struct {
    MonitorSurface surfaces[MAX_MONITORS];
    int surface_count;

    DesktopIcon icons[MAX_DESKTOP_ICONS];
    int icon_count;
    int selected_index;

    /* Drag & rubber-band state */
    gboolean dragging_icon;
    int drag_icon_idx;
    int drag_offset_x;
    int drag_offset_y;

    gboolean rubber_banding;
    double rb_start_x, rb_start_y;
    double rb_curr_x, rb_curr_y;

    /* Background appearance */
    GdkRGBA bg_color;
    char wallpaper_path[512];
    char wallpaper_mode[32]; /* "none", "tile", "center", "stretch" */
    char icon_monitor[64];   /* "all", "primary", or connector name e.g. "eDP-1", "DP-6" */
    GdkPixbuf *wallpaper_pb;

    GFileMonitor *desktop_monitor;
    GFileMonitor *trash_monitor;
    gboolean trash_has_items;
} DesktopApp;

static DesktopApp g_app;

static gboolean check_trash_non_empty(void) {
    char trash_path[512];
    snprintf(trash_path, sizeof(trash_path), "%s/.local/share/Trash/files", g_get_home_dir());
    GDir *dir = g_dir_open(trash_path, 0, NULL);
    if (!dir) return FALSE;
    const char *name;
    gboolean has_files = FALSE;
    while ((name = g_dir_read_name(dir)) != NULL) {
        if (name[0] != '.') {
            has_files = TRUE;
            break;
        }
    }
    g_dir_close(dir);
    return has_files;
}

static void add_desktop_icon(const char *title, const char *icon_name, const char *cmd, int col, int row, gboolean is_trash) {
    if (g_app.icon_count >= MAX_DESKTOP_ICONS) return;
    DesktopIcon *ic = &g_app.icons[g_app.icon_count++];
    snprintf(ic->title, sizeof(ic->title), "%s", title);
    snprintf(ic->icon_name, sizeof(ic->icon_name), "%s", icon_name);
    snprintf(ic->command, sizeof(ic->command), "%s", cmd);
    ic->x = 16 + col * 86;
    ic->y = 14 + row * 78;
    ic->selected = FALSE;
    ic->is_trash = is_trash;
    ic->pixbuf = chicago_load_icon(icon_name, 32);
}

static void load_desktop_Appearance_config(void) {
    char color_str[64];
    chicago_config_get("bg_color", color_str, sizeof(color_str), "#008080");
    if (!gdk_rgba_parse(&g_app.bg_color, color_str)) {
        gdk_rgba_parse(&g_app.bg_color, "#008080");
    }

    chicago_config_get("wallpaper", g_app.wallpaper_path, sizeof(g_app.wallpaper_path), "");
    chicago_config_get("wallpaper_mode", g_app.wallpaper_mode, sizeof(g_app.wallpaper_mode), "tile");
    chicago_config_get("icon_monitor", g_app.icon_monitor, sizeof(g_app.icon_monitor), "all");

    if (g_app.wallpaper_pb) {
        g_object_unref(g_app.wallpaper_pb);
        g_app.wallpaper_pb = NULL;
    }
    if (g_app.wallpaper_path[0] != '\0' && g_file_test(g_app.wallpaper_path, G_FILE_TEST_EXISTS)) {
        g_app.wallpaper_pb = gdk_pixbuf_new_from_file(g_app.wallpaper_path, NULL);
    }

    for (int i = 0; i < g_app.surface_count; i++) {
        MonitorSurface *ms = &g_app.surfaces[i];
        if (g_strcmp0(g_app.icon_monitor, "all") == 0 || g_app.icon_monitor[0] == '\0') {
            ms->show_icons = TRUE;
        } else if (g_strcmp0(g_app.icon_monitor, "primary") == 0) {
            ms->show_icons = (i == 0);
        } else {
            ms->show_icons = (g_strcmp0(ms->plug_name, g_app.icon_monitor) == 0);
        }
        if (ms->drawing_area) {
            gtk_widget_queue_draw(ms->drawing_area);
        }
    }
}

static void populate_icons(void) {
    for (int i = 0; i < g_app.icon_count; i++) {
        if (g_app.icons[i].pixbuf) {
            g_object_unref(g_app.icons[i].pixbuf);
            g_app.icons[i].pixbuf = NULL;
        }
    }
    g_app.icon_count = 0;
    g_app.selected_index = -1;
    g_app.trash_has_items = check_trash_non_empty();

    const char *home = g_get_home_dir();
    char cmd_buf[1024];

    /* 1. My Computer */
    snprintf(cmd_buf, sizeof(cmd_buf), "thunar / 2>/dev/null || xdg-open /");
    add_desktop_icon("My Computer", "computer", cmd_buf, 0, 0, FALSE);

    /* 2. My Documents */
    snprintf(cmd_buf, sizeof(cmd_buf), "thunar \"%s/Documents\" 2>/dev/null || xdg-open \"%s/Documents\"", home, home);
    add_desktop_icon("My Documents", "folder-documents", cmd_buf, 0, 1, FALSE);

    /* 3. Network Neighborhood */
    add_desktop_icon("Network\nNeighborhood", "network-workgroup", "chicago-control-panel --network", 0, 2, FALSE);

    /* 4. Recycle Bin */
    snprintf(cmd_buf, sizeof(cmd_buf),
             "mkdir -p \"%s/.local/share/Trash/files\" && thunar \"%s/.local/share/Trash/files\"",
             home, home);
    add_desktop_icon("Recycle Bin",
                     g_app.trash_has_items ? "user-trash-full" : "user-trash",
                     cmd_buf, 0, 3, TRUE);

    /* 5. Control Panel */
    add_desktop_icon("Control Panel", "preferences-system", "chicago-control-panel", 0, 4, FALSE);

    /* 6. MS-DOS Prompt */
    snprintf(cmd_buf, sizeof(cmd_buf),
             "kitty -c \"%s/.config/chicago-wayland/kitty/kitty.conf\" --title \"MS-DOS Prompt\"",
             home);
    add_desktop_icon("MS-DOS Prompt", "utilities-terminal", cmd_buf, 0, 5, FALSE);

    /* 7. Display Properties */
    add_desktop_icon("Display\nProperties", "preferences-desktop-display", "chicago-control-panel --display", 0, 6, FALSE);

    /* Scan ~/Desktop for user files/shortcuts */
    char desktop_dir[256];
    snprintf(desktop_dir, sizeof(desktop_dir), "%s/Desktop", home);
    GDir *dir = g_dir_open(desktop_dir, 0, NULL);
    if (dir) {
        const char *fname;
        int col = 1;
        int row = 0;
        while ((fname = g_dir_read_name(dir)) != NULL) {
            if (fname[0] == '.') continue;
            char full_path[480];
            snprintf(full_path, sizeof(full_path), "%s/%.200s", desktop_dir, fname);

            if (g_str_has_suffix(fname, ".desktop")) {
                GKeyFile *kf = g_key_file_new();
                if (g_key_file_load_from_file(kf, full_path, G_KEY_FILE_NONE, NULL)) {
                    char *name = g_key_file_get_locale_string(kf, "Desktop Entry", "Name", NULL, NULL);
                    char *icon = g_key_file_get_string(kf, "Desktop Entry", "Icon", NULL);
                    char *exec = g_key_file_get_string(kf, "Desktop Entry", "Exec", NULL);
                    if (name && exec) {
                        char *p = strstr(exec, " %");
                        if (p) *p = '\0';
                        add_desktop_icon(name, icon ? icon : "application-x-executable", exec, col, row, FALSE);
                        row++;
                        if (row >= 7) { row = 0; col++; }
                    }
                    g_free(name);
                    g_free(icon);
                    g_free(exec);
                }
                g_key_file_free(kf);
            } else {
                gboolean is_dir = g_file_test(full_path, G_FILE_TEST_IS_DIR);
                snprintf(cmd_buf, sizeof(cmd_buf), "thunar \"%s\" 2>/dev/null || xdg-open \"%s\"", full_path, full_path);
                add_desktop_icon(fname, is_dir ? "folder" : "text-x-generic", cmd_buf, col, row, FALSE);
                row++;
                if (row >= 7) { row = 0; col++; }
            }
        }
        g_dir_close(dir);
    }

    for (int i = 0; i < g_app.surface_count; i++) {
        if (g_app.surfaces[i].drawing_area) {
            gtk_widget_queue_draw(g_app.surfaces[i].drawing_area);
        }
    }
}

static void queue_redraw_all(void) {
    for (int i = 0; i < g_app.surface_count; i++) {
        if (g_app.surfaces[i].drawing_area) {
            gtk_widget_queue_draw(g_app.surfaces[i].drawing_area);
        }
    }
}

static int icon_at_point(double x, double y) {
    for (int i = g_app.icon_count - 1; i >= 0; i--) {
        DesktopIcon *ic = &g_app.icons[i];
        if (x >= ic->x - 10 && x <= ic->x + 68 &&
            y >= ic->y - 4 && y <= ic->y + 68) {
            return i;
        }
    }
    return -1;
}

static gboolean on_draw_desktop(GtkWidget *widget, cairo_t *cr, gpointer data) {
    MonitorSurface *ms = (MonitorSurface *)data;
    int width = gtk_widget_get_allocated_width(widget);
    int height = gtk_widget_get_allocated_height(widget);

    /* 1. Fill base desktop color (Classic Windows 95 Teal #008080 by default) */
    cairo_set_source_rgb(cr, g_app.bg_color.red, g_app.bg_color.green, g_app.bg_color.blue);
    cairo_paint(cr);

    /* 2. Optional Pattern or Wallpaper */
    if (g_app.wallpaper_pb && g_strcmp0(g_app.wallpaper_mode, "none") != 0) {
        int pw = gdk_pixbuf_get_width(g_app.wallpaper_pb);
        int ph = gdk_pixbuf_get_height(g_app.wallpaper_pb);
        if (pw > 0 && ph > 0) {
            if (g_strcmp0(g_app.wallpaper_mode, "center") == 0) {
                int ox = (width - pw) / 2;
                int oy = (height - ph) / 2;
                gdk_cairo_set_source_pixbuf(cr, g_app.wallpaper_pb, ox, oy);
                cairo_paint(cr);
            } else if (g_strcmp0(g_app.wallpaper_mode, "stretch") == 0) {
                GdkPixbuf *scaled = gdk_pixbuf_scale_simple(g_app.wallpaper_pb, width, height, GDK_INTERP_NEAREST);
                if (scaled) {
                    gdk_cairo_set_source_pixbuf(cr, scaled, 0, 0);
                    cairo_paint(cr);
                    g_object_unref(scaled);
                }
            } else {
                /* Tile mode (used for classic Win95 patterns and tiled wallpapers) */
                gdk_cairo_set_source_pixbuf(cr, g_app.wallpaper_pb, 0, 0);
                cairo_pattern_t *pat = cairo_get_source(cr);
                cairo_pattern_set_extend(pat, CAIRO_EXTEND_REPEAT);
                cairo_rectangle(cr, 0, 0, width, height);
                cairo_fill(cr);
            }
        }
    }

    if (!ms->show_icons) {
        return FALSE;
    }

    /* 3. Draw Desktop Icons with pixel-crisp Helvetica 8pt bitmap font */
    PangoLayout *layout = gtk_widget_create_pango_layout(widget, NULL);
    PangoFontDescription *font_desc = pango_font_description_from_string("Helvetica 8");
    pango_layout_set_font_description(layout, font_desc);
    pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
    pango_layout_set_width(layout, 76 * PANGO_SCALE);
    pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);

    for (int i = 0; i < g_app.icon_count; i++) {
        DesktopIcon *ic = &g_app.icons[i];

        if (ic->pixbuf) {
            int icon_x = ic->x + 12;
            int icon_y = ic->y;
            gdk_cairo_set_source_pixbuf(cr, ic->pixbuf, icon_x, icon_y);
            cairo_paint(cr);

            if (ic->selected) {
                /* Classic Win95 navy selection tint on icon */
                cairo_set_source_rgba(cr, 0.0, 0.0, 0.502, 0.40);
                cairo_rectangle(cr, icon_x, icon_y, 32, 32);
                cairo_fill(cr);
            }
        }

        pango_layout_set_text(layout, ic->title, -1);
        int text_w = 0, text_h = 0;
        pango_layout_get_pixel_size(layout, &text_w, &text_h);

        int label_x = ic->x + 28 - (text_w / 2);
        int label_y = ic->y + 36;

        if (ic->selected) {
            /* Classic Windows 95 #000080 Navy selection box + dotted focus border */
            cairo_set_source_rgb(cr, 0.0, 0.0, 0.502);
            cairo_rectangle(cr, label_x - 3, label_y - 1, text_w + 6, text_h + 2);
            cairo_fill(cr);

            cairo_save(cr);
            double dashes[] = {1.0, 1.0};
            cairo_set_dash(cr, dashes, 2, 0);
            cairo_set_line_width(cr, 1.0);
            cairo_set_source_rgb(cr, 1.0, 1.0, 0.50);
            cairo_rectangle(cr, label_x - 2.5, label_y - 0.5, text_w + 5, text_h + 1);
            cairo_stroke(cr);
            cairo_restore(cr);
        } else {
            /* Classic Windows 95 opaque background label box */
            cairo_set_source_rgb(cr, g_app.bg_color.red, g_app.bg_color.green, g_app.bg_color.blue);
            cairo_rectangle(cr, label_x - 2, label_y - 1, text_w + 4, text_h + 2);
            cairo_fill(cr);
        }

        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        cairo_move_to(cr, ic->x - 10, label_y);
        pango_cairo_show_layout(cr, layout);
    }

    pango_font_description_free(font_desc);
    g_object_unref(layout);

    /* 4. Rubber-band selection box */
    if (g_app.rubber_banding) {
        double rx = MIN(g_app.rb_start_x, g_app.rb_curr_x);
        double ry = MIN(g_app.rb_start_y, g_app.rb_curr_y);
        double rw = ABS(g_app.rb_curr_x - g_app.rb_start_x);
        double rh = ABS(g_app.rb_curr_y - g_app.rb_start_y);

        cairo_save(cr);
        double dashes[] = {1.0, 1.0};
        cairo_set_dash(cr, dashes, 2, 0);
        cairo_set_line_width(cr, 1.0);
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        cairo_rectangle(cr, rx + 0.5, ry + 0.5, rw, rh);
        cairo_stroke(cr);
        cairo_restore(cr);
    }

    return FALSE;
}

static void on_ctx_open_item(GtkMenuItem *item, gpointer data) {
    (void)item;
    int idx = GPOINTER_TO_INT(data);
    if (idx >= 0 && idx < g_app.icon_count) {
        chicago_spawn_command(g_app.icons[idx].command);
    }
}

static void on_ctx_empty_trash(GtkMenuItem *item, gpointer data) {
    (void)item;
    (void)data;
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "rm -rf \"%s/.local/share/Trash/files/\"* \"%s/.local/share/Trash/info/\"*",
             g_get_home_dir(), g_get_home_dir());
    chicago_spawn_command(cmd);
}

static void on_ctx_new_folder(GtkMenuItem *item, gpointer data) {
    (void)item;
    (void)data;
    char path[512];
    for (int n = 1; n < 100; n++) {
        if (n == 1)
            snprintf(path, sizeof(path), "%s/Desktop/New Folder", g_get_home_dir());
        else
            snprintf(path, sizeof(path), "%s/Desktop/New Folder (%d)", g_get_home_dir(), n);
        if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
            g_mkdir_with_parents(path, 0755);
            break;
        }
    }
    populate_icons();
}

static void on_ctx_new_text_file(GtkMenuItem *item, gpointer data) {
    (void)item;
    (void)data;
    char path[512];
    for (int n = 1; n < 100; n++) {
        if (n == 1)
            snprintf(path, sizeof(path), "%s/Desktop/New Text Document.txt", g_get_home_dir());
        else
            snprintf(path, sizeof(path), "%s/Desktop/New Text Document (%d).txt", g_get_home_dir(), n);
        if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
            g_file_set_contents(path, "", 0, NULL);
            break;
        }
    }
    populate_icons();
}

static void on_ctx_line_up_icons(GtkMenuItem *item, gpointer data) {
    (void)item;
    (void)data;
    for (int i = 0; i < g_app.icon_count; i++) {
        int col = i / 7;
        int row = i % 7;
        g_app.icons[i].x = 16 + col * 86;
        g_app.icons[i].y = 14 + row * 78;
    }
    queue_redraw_all();
}

static void on_ctx_refresh(GtkMenuItem *item, gpointer data) {
    (void)item;
    (void)data;
    load_desktop_Appearance_config();
    populate_icons();
}

static void on_ctx_run_cmd(GtkMenuItem *item, gpointer data) {
    (void)item;
    const char *cmd = (const char *)data;
    chicago_spawn_command(cmd);
}

static void show_desktop_context_menu(GdkEventButton *event, int hit_idx) {
    GtkWidget *menu = gtk_menu_new();
    gtk_style_context_add_class(gtk_widget_get_style_context(menu), "win95-menu");

    if (hit_idx >= 0) {
        GtkWidget *open_mi = gtk_menu_item_new_with_label("Open");
        g_signal_connect(open_mi, "activate", G_CALLBACK(on_ctx_open_item), GINT_TO_POINTER(hit_idx));
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), open_mi);

        GtkWidget *explore_mi = gtk_menu_item_new_with_label("Explore");
        g_signal_connect(explore_mi, "activate", G_CALLBACK(on_ctx_open_item), GINT_TO_POINTER(hit_idx));
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), explore_mi);

        if (g_app.icons[hit_idx].is_trash) {
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
            GtkWidget *empty_mi = gtk_menu_item_new_with_label("Empty Recycle Bin");
            gtk_widget_set_sensitive(empty_mi, g_app.trash_has_items);
            g_signal_connect(empty_mi, "activate", G_CALLBACK(on_ctx_empty_trash), NULL);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), empty_mi);
        }

        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
        GtkWidget *prop_mi = gtk_menu_item_new_with_label("Properties");
        if (g_strcmp0(g_app.icons[hit_idx].title, "My Computer") == 0) {
            g_signal_connect(prop_mi, "activate", G_CALLBACK(on_ctx_run_cmd), (gpointer)"chicago-control-panel --system");
        } else if (strstr(g_app.icons[hit_idx].title, "Network") != NULL) {
            g_signal_connect(prop_mi, "activate", G_CALLBACK(on_ctx_run_cmd), (gpointer)"chicago-control-panel --network");
        } else {
            g_signal_connect(prop_mi, "activate", G_CALLBACK(on_ctx_run_cmd), (gpointer)"chicago-control-panel --display");
        }
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), prop_mi);
    } else {
        /* Standard Windows 95 Desktop Right-Click Menu */
        GtkWidget *arrange_mi = gtk_menu_item_new_with_label("Arrange Icons");
        GtkWidget *arrange_sub = gtk_menu_new();
        GtkWidget *by_name_mi = gtk_menu_item_new_with_label("by Name");
        g_signal_connect(by_name_mi, "activate", G_CALLBACK(on_ctx_refresh), NULL);
        gtk_menu_shell_append(GTK_MENU_SHELL(arrange_sub), by_name_mi);
        GtkWidget *auto_arr_mi = gtk_menu_item_new_with_label("Auto Arrange");
        g_signal_connect(auto_arr_mi, "activate", G_CALLBACK(on_ctx_line_up_icons), NULL);
        gtk_menu_shell_append(GTK_MENU_SHELL(arrange_sub), auto_arr_mi);
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(arrange_mi), arrange_sub);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), arrange_mi);

        GtkWidget *lineup_mi = gtk_menu_item_new_with_label("Line Up Icons");
        g_signal_connect(lineup_mi, "activate", G_CALLBACK(on_ctx_line_up_icons), NULL);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), lineup_mi);

        GtkWidget *refresh_mi = gtk_menu_item_new_with_label("Refresh");
        g_signal_connect(refresh_mi, "activate", G_CALLBACK(on_ctx_refresh), NULL);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), refresh_mi);

        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

        GtkWidget *new_mi = gtk_menu_item_new_with_label("New");
        GtkWidget *new_sub = gtk_menu_new();
        GtkWidget *new_folder_mi = gtk_menu_item_new_with_label("Folder");
        g_signal_connect(new_folder_mi, "activate", G_CALLBACK(on_ctx_new_folder), NULL);
        gtk_menu_shell_append(GTK_MENU_SHELL(new_sub), new_folder_mi);

        GtkWidget *new_txt_mi = gtk_menu_item_new_with_label("Text Document");
        g_signal_connect(new_txt_mi, "activate", G_CALLBACK(on_ctx_new_text_file), NULL);
        gtk_menu_shell_append(GTK_MENU_SHELL(new_sub), new_txt_mi);
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(new_mi), new_sub);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), new_mi);

        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

        GtkWidget *dos_mi = gtk_menu_item_new_with_label("MS-DOS Prompt");
        g_signal_connect(dos_mi, "activate", G_CALLBACK(on_ctx_run_cmd),
                         (gpointer)"kitty -c ~/.config/chicago-wayland/kitty/kitty.conf --title 'MS-DOS Prompt'");
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), dos_mi);

        GtkWidget *cp_mi = gtk_menu_item_new_with_label("Control Panel");
        g_signal_connect(cp_mi, "activate", G_CALLBACK(on_ctx_run_cmd), (gpointer)"chicago-control-panel");
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), cp_mi);

        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

        /* Authentic Windows 95 Desktop -> Properties opens Display Properties */
        GtkWidget *prop_mi = gtk_menu_item_new_with_label("Properties");
        g_signal_connect(prop_mi, "activate", G_CALLBACK(on_ctx_run_cmd),
                         (gpointer)"chicago-control-panel --display");
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), prop_mi);
    }

    gtk_widget_show_all(menu);
    gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)event);
}

static gboolean on_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    MonitorSurface *ms = (MonitorSurface *)data;
    gtk_widget_grab_focus(widget);

    int hit = ms->show_icons ? icon_at_point(event->x, event->y) : -1;

    if (event->button == 1) {
        if (event->type == GDK_2BUTTON_PRESS && hit >= 0) {
            chicago_spawn_command(g_app.icons[hit].command);
            g_app.dragging_icon = FALSE;
            return TRUE;
        }

        for (int i = 0; i < g_app.icon_count; i++) {
            g_app.icons[i].selected = (i == hit);
        }
        g_app.selected_index = hit;

        if (hit >= 0) {
            g_app.dragging_icon = TRUE;
            g_app.drag_icon_idx = hit;
            g_app.drag_offset_x = (int)event->x - g_app.icons[hit].x;
            g_app.drag_offset_y = (int)event->y - g_app.icons[hit].y;
        } else {
            g_app.rubber_banding = TRUE;
            g_app.rb_start_x = g_app.rb_curr_x = event->x;
            g_app.rb_start_y = g_app.rb_curr_y = event->y;
        }
        queue_redraw_all();
        return TRUE;
    } else if (event->button == 3) {
        if (hit >= 0) {
            for (int i = 0; i < g_app.icon_count; i++) {
                g_app.icons[i].selected = (i == hit);
            }
            g_app.selected_index = hit;
            queue_redraw_all();
        }
        show_desktop_context_menu(event, hit);
        return TRUE;
    }

    return FALSE;
}

static gboolean on_button_release(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    (void)widget;
    (void)data;
    if (event->button == 1) {
        g_app.dragging_icon = FALSE;
        if (g_app.rubber_banding) {
            g_app.rubber_banding = FALSE;
            queue_redraw_all();
        }
    }
    return FALSE;
}

static gboolean on_motion_notify(GtkWidget *widget, GdkEventMotion *event, gpointer data) {
    (void)data;
    int w = gtk_widget_get_allocated_width(widget);
    int h = gtk_widget_get_allocated_height(widget);

    if (g_app.dragging_icon && g_app.drag_icon_idx >= 0 && g_app.drag_icon_idx < g_app.icon_count) {
        DesktopIcon *ic = &g_app.icons[g_app.drag_icon_idx];
        ic->x = CLAMP((int)event->x - g_app.drag_offset_x, 8, MAX(8, w - 72));
        ic->y = CLAMP((int)event->y - g_app.drag_offset_y, 8, MAX(8, h - 84));
        queue_redraw_all();
        return TRUE;
    } else if (g_app.rubber_banding) {
        g_app.rb_curr_x = event->x;
        g_app.rb_curr_y = event->y;
        double rx1 = MIN(g_app.rb_start_x, g_app.rb_curr_x);
        double ry1 = MIN(g_app.rb_start_y, g_app.rb_curr_y);
        double rx2 = MAX(g_app.rb_start_x, g_app.rb_curr_x);
        double ry2 = MAX(g_app.rb_start_y, g_app.rb_curr_y);

        for (int i = 0; i < g_app.icon_count; i++) {
            DesktopIcon *ic = &g_app.icons[i];
            gboolean inside = (ic->x + 44 >= rx1 && ic->x <= rx2 &&
                               ic->y + 44 >= ry1 && ic->y <= ry2);
            ic->selected = inside;
        }
        queue_redraw_all();
        return TRUE;
    }
    return FALSE;
}

static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data) {
    (void)widget;
    (void)data;
    if (g_app.icon_count == 0) return FALSE;

    if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter) {
        if (g_app.selected_index >= 0 && g_app.selected_index < g_app.icon_count) {
            chicago_spawn_command(g_app.icons[g_app.selected_index].command);
            return TRUE;
        }
    } else if (event->keyval == GDK_KEY_Down || event->keyval == GDK_KEY_Right) {
        g_app.selected_index = (g_app.selected_index + 1) % g_app.icon_count;
        for (int i = 0; i < g_app.icon_count; i++)
            g_app.icons[i].selected = (i == g_app.selected_index);
        queue_redraw_all();
        return TRUE;
    } else if (event->keyval == GDK_KEY_Up || event->keyval == GDK_KEY_Left) {
        g_app.selected_index = (g_app.selected_index <= 0) ? (g_app.icon_count - 1) : (g_app.selected_index - 1);
        for (int i = 0; i < g_app.icon_count; i++)
            g_app.icons[i].selected = (i == g_app.selected_index);
        queue_redraw_all();
        return TRUE;
    } else if (event->keyval == GDK_KEY_F5) {
        load_desktop_Appearance_config();
        populate_icons();
        return TRUE;
    }
    return FALSE;
}

static void on_fs_changed(GFileMonitor *mon, GFile *file, GFile *other, GFileMonitorEvent ev, gpointer data) {
    (void)mon;
    (void)file;
    (void)other;
    (void)data;
    if (ev == G_FILE_MONITOR_EVENT_CREATED ||
        ev == G_FILE_MONITOR_EVENT_DELETED ||
        ev == G_FILE_MONITOR_EVENT_MOVED_IN ||
        ev == G_FILE_MONITOR_EVENT_MOVED_OUT) {
        populate_icons();
    }
}

static gboolean on_sigusr1_reload(gpointer data) {
    (void)data;
    load_desktop_Appearance_config();
    populate_icons();
    return G_SOURCE_CONTINUE;
}

static void create_surface_for_monitor(GdkMonitor *mon, int idx) {
    if (g_app.surface_count >= MAX_MONITORS) return;
    MonitorSurface *ms = &g_app.surfaces[g_app.surface_count++];
    memset(ms, 0, sizeof(*ms));
    ms->monitor = mon;

    const char *model = gdk_monitor_get_model(mon);
    snprintf(ms->plug_name, sizeof(ms->plug_name), "%s", model ? model : "Monitor");

    ms->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(ms->window), "Chicago95 Desktop");

    if (gtk_layer_is_supported()) {
        gtk_layer_init_for_window(GTK_WINDOW(ms->window));
        gtk_layer_set_layer(GTK_WINDOW(ms->window), GTK_LAYER_SHELL_LAYER_BACKGROUND);
        if (mon) {
            gtk_layer_set_monitor(GTK_WINDOW(ms->window), mon);
        }
        gtk_layer_set_anchor(GTK_WINDOW(ms->window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
        gtk_layer_set_anchor(GTK_WINDOW(ms->window), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
        gtk_layer_set_anchor(GTK_WINDOW(ms->window), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
        gtk_layer_set_anchor(GTK_WINDOW(ms->window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
        gtk_layer_set_exclusive_zone(GTK_WINDOW(ms->window), -1);
        gtk_layer_set_keyboard_mode(GTK_WINDOW(ms->window), GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);
        gtk_layer_set_namespace(GTK_WINDOW(ms->window), "chicago-desktop");
    } else {
        gtk_window_set_default_size(GTK_WINDOW(ms->window), 1024, 768);
    }

    ms->show_icons = (idx == 0 || g_strcmp0(g_app.icon_monitor, "all") == 0);

    ms->drawing_area = gtk_drawing_area_new();
    gtk_widget_set_can_focus(ms->drawing_area, TRUE);
    gtk_widget_add_events(ms->drawing_area,
                          GDK_BUTTON_PRESS_MASK |
                          GDK_BUTTON_RELEASE_MASK |
                          GDK_POINTER_MOTION_MASK |
                          GDK_KEY_PRESS_MASK);

    g_signal_connect(ms->drawing_area, "draw", G_CALLBACK(on_draw_desktop), ms);
    g_signal_connect(ms->drawing_area, "button-press-event", G_CALLBACK(on_button_press), ms);
    g_signal_connect(ms->drawing_area, "button-release-event", G_CALLBACK(on_button_release), ms);
    g_signal_connect(ms->drawing_area, "motion-notify-event", G_CALLBACK(on_motion_notify), ms);
    g_signal_connect(ms->drawing_area, "key-press-event", G_CALLBACK(on_key_press), ms);

    gtk_container_add(GTK_CONTAINER(ms->window), ms->drawing_area);
    gtk_widget_show_all(ms->window);
}

int main(int argc, char **argv) {
    gtk_init(&argc, &argv);
    chicago_apply_theme();

    memset(&g_app, 0, sizeof(g_app));
    g_app.selected_index = -1;

    GdkDisplay *display = gdk_display_get_default();
    int n_monitors = display ? gdk_display_get_n_monitors(display) : 0;
    if (n_monitors > 0) {
        for (int i = 0; i < n_monitors && i < MAX_MONITORS; i++) {
            create_surface_for_monitor(gdk_display_get_monitor(display, i), i);
        }
    } else {
        create_surface_for_monitor(NULL, 0);
    }

    load_desktop_Appearance_config();
    populate_icons();

    /* Event-driven inotify file monitors (zero polling) */
    char desktop_dir[512], trash_dir[512];
    snprintf(desktop_dir, sizeof(desktop_dir), "%s/Desktop", g_get_home_dir());
    snprintf(trash_dir, sizeof(trash_dir), "%s/.local/share/Trash/files", g_get_home_dir());
    g_mkdir_with_parents(desktop_dir, 0755);
    g_mkdir_with_parents(trash_dir, 0700);

    GFile *df = g_file_new_for_path(desktop_dir);
    g_app.desktop_monitor = g_file_monitor_directory(df, G_FILE_MONITOR_NONE, NULL, NULL);
    if (g_app.desktop_monitor) {
        g_signal_connect(g_app.desktop_monitor, "changed", G_CALLBACK(on_fs_changed), NULL);
    }
    g_object_unref(df);

    GFile *tf = g_file_new_for_path(trash_dir);
    g_app.trash_monitor = g_file_monitor_directory(tf, G_FILE_MONITOR_NONE, NULL, NULL);
    if (g_app.trash_monitor) {
        g_signal_connect(g_app.trash_monitor, "changed", G_CALLBACK(on_fs_changed), NULL);
    }
    g_object_unref(tf);

    g_unix_signal_add(SIGUSR1, on_sigusr1_reload, NULL);

    gtk_main();
    return 0;
}
