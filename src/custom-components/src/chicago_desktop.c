/*
 * chicago-desktop: Native Wayland Windows 95 Desktop Surface & Icon Manager
 * Uses GTK3 + gtk-layer-shell + Cairo + GIO GFileMonitor (zero polling)
 * License: GPL-3.0+
 */

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gtk-layer-shell/gtk-layer-shell.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include <cairo.h>
#include <pango/pangocairo.h>
#include <math.h>
#include "chicago_common.h"

#define MAX_DESKTOP_ICONS 64
#define ICON_CELL_W 96
#define ICON_CELL_H 82
#define ICON_TOP_MARGIN 12
#define ICON_LEFT_MARGIN 12
#define ICON_IMG_SIZE 32

typedef struct {
    char title[128];
    char icon_name[128];
    char command[512];
    gboolean is_recycle_bin;
    int x;
    int y;
    GdkPixbuf *pixbuf;
} DesktopIcon;

typedef struct {
    DesktopIcon icons[MAX_DESKTOP_ICONS];
    int icon_count;
    int selected_index;
    gboolean dragging_rubberband;
    double drag_start_x;
    double drag_start_y;
    double drag_cur_x;
    double drag_cur_y;
    GdkRGBA bg_color;
    GdkPixbuf *wallpaper_pb;
    GtkWidget *primary_area;
    GFileMonitor *desktop_monitor;
    GFileMonitor *trash_monitor;
} DesktopState;

static DesktopState g_desk = {0};

static gboolean is_trash_non_empty(void) {
    char trash_path[512];
    snprintf(trash_path, sizeof(trash_path), "%s/.local/share/Trash/files", g_get_home_dir());
    GDir *dir = g_dir_open(trash_path, 0, NULL);
    if (!dir) return FALSE;
    const char *name = g_dir_read_name(dir);
    gboolean has_files = (name != NULL);
    g_dir_close(dir);
    return has_files;
}

static void empty_recycle_bin(void) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "rm -rf \"%s/.local/share/Trash/files/\"* \"%s/.local/share/Trash/info/\"* 2>/dev/null",
             g_get_home_dir(), g_get_home_dir());
    chicago_spawn_command(cmd);
}

static void add_desktop_icon(const char *title, const char *icon_name, const char *cmd, gboolean is_trash) {
    if (g_desk.icon_count >= MAX_DESKTOP_ICONS) return;
    int idx = g_desk.icon_count++;
    DesktopIcon *ic = &g_desk.icons[idx];
    memset(ic, 0, sizeof(*ic));
    g_strlcpy(ic->title, title, sizeof(ic->title));
    g_strlcpy(ic->icon_name, icon_name, sizeof(ic->icon_name));
    g_strlcpy(ic->command, cmd, sizeof(ic->command));
    ic->is_recycle_bin = is_trash;
    ic->pixbuf = chicago_load_icon(icon_name, ICON_IMG_SIZE);
}

static void layout_icons_grid(int max_height) {
    if (max_height < 300) max_height = 900;
    int rows_per_col = (max_height - 48) / ICON_CELL_H;
    if (rows_per_col < 1) rows_per_col = 6;

    for (int i = 0; i < g_desk.icon_count; i++) {
        int col = i / rows_per_col;
        int row = i % rows_per_col;
        g_desk.icons[i].x = ICON_LEFT_MARGIN + col * ICON_CELL_W;
        g_desk.icons[i].y = ICON_TOP_MARGIN + row * ICON_CELL_H;
    }
}

static void rebuild_desktop_icons(void) {
    for (int i = 0; i < g_desk.icon_count; i++) {
        if (g_desk.icons[i].pixbuf) {
            g_object_unref(g_desk.icons[i].pixbuf);
            g_desk.icons[i].pixbuf = NULL;
        }
    }
    g_desk.icon_count = 0;

    const char *home = g_get_home_dir();
    char cmd_buf[2560];

    /* 1. My Computer */
    snprintf(cmd_buf, sizeof(cmd_buf), "nautilus / 2>/dev/null || xdg-open /");
    add_desktop_icon("My Computer", "computer", cmd_buf, FALSE);

    /* 2. My Documents */
    snprintf(cmd_buf, sizeof(cmd_buf), "nautilus \"%s/Documents\" 2>/dev/null || xdg-open \"%s/Documents\"", home, home);
    add_desktop_icon("My Documents", "folder-documents", cmd_buf, FALSE);

    /* 3. Network Neighborhood */
    add_desktop_icon("Network Neighborhood", "network-workgroup", "chicago-control-panel --tab network", FALSE);

    /* 4. Recycle Bin */
    gboolean trash_full = is_trash_non_empty();
    snprintf(cmd_buf, sizeof(cmd_buf),
             "mkdir -p \"%s/.local/share/Trash/files\" && nautilus \"%s/.local/share/Trash/files\"",
             home, home);
    add_desktop_icon("Recycle Bin", trash_full ? "user-trash-full" : "user-trash", cmd_buf, TRUE);

    /* 5. MS-DOS Prompt */
    snprintf(cmd_buf, sizeof(cmd_buf),
             "kitty --title \"MS-DOS Prompt\" -o background=#000000 -o foreground=#c0c0c0 -o cursor=#c0c0c0");
    add_desktop_icon("MS-DOS Prompt", "utilities-terminal", cmd_buf, FALSE);

    /* 6. Control Panel */
    add_desktop_icon("Control Panel", "preferences-system", "chicago-control-panel", FALSE);

    /* 7. Load items from ~/Desktop */
    char desktop_dir[512];
    snprintf(desktop_dir, sizeof(desktop_dir), "%s/Desktop", home);
    GDir *dir = g_dir_open(desktop_dir, 0, NULL);
    if (dir) {
        const char *entry;
        while ((entry = g_dir_read_name(dir)) != NULL) {
            if (entry[0] == '.') continue;
            char full_path[1024];
            snprintf(full_path, sizeof(full_path), "%s/%s", desktop_dir, entry);
            if (g_str_has_suffix(entry, ".desktop")) {
                GDesktopAppInfo *app = g_desktop_app_info_new_from_filename(full_path);
                if (app) {
                    const char *name = g_app_info_get_display_name(G_APP_INFO(app));
                    GIcon *gicon = g_app_info_get_icon(G_APP_INFO(app));
                    char *icon_str = gicon ? g_icon_to_string(gicon) : g_strdup("application-x-executable");
                    snprintf(cmd_buf, sizeof(cmd_buf), "gtk-launch \"%s\" 2>/dev/null || xdg-open \"%s\"",
                             entry, full_path);
                    add_desktop_icon(name ? name : entry, icon_str ? icon_str : "application-x-executable", cmd_buf, FALSE);
                    g_free(icon_str);
                    g_object_unref(app);
                }
            } else if (g_file_test(full_path, G_FILE_TEST_IS_DIR)) {
                snprintf(cmd_buf, sizeof(cmd_buf), "nautilus \"%s\" 2>/dev/null || xdg-open \"%s\"", full_path, full_path);
                add_desktop_icon(entry, "folder", cmd_buf, FALSE);
            } else {
                snprintf(cmd_buf, sizeof(cmd_buf), "xdg-open \"%s\"", full_path);
                add_desktop_icon(entry, "text-x-generic", cmd_buf, FALSE);
            }
        }
        g_dir_close(dir);
    }

    int alloc_h = 900;
    if (g_desk.primary_area) {
        int h = gtk_widget_get_allocated_height(g_desk.primary_area);
        if (h > 200) alloc_h = h;
    }
    layout_icons_grid(alloc_h);
    if (g_desk.primary_area) {
        gtk_widget_queue_draw(g_desk.primary_area);
    }
}

static int icon_at_point(double x, double y) {
    for (int i = 0; i < g_desk.icon_count; i++) {
        DesktopIcon *ic = &g_desk.icons[i];
        if (x >= ic->x && x <= ic->x + ICON_CELL_W - 4 &&
            y >= ic->y && y <= ic->y + ICON_CELL_H - 4) {
            return i;
        }
    }
    return -1;
}

static gboolean on_desktop_draw(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    gboolean is_primary = GPOINTER_TO_INT(user_data);
    int w = gtk_widget_get_allocated_width(widget);
    int h = gtk_widget_get_allocated_height(widget);

    /* 1. Draw solid Windows 95 Teal (#008080) or configured background */
    cairo_set_source_rgb(cr, g_desk.bg_color.red, g_desk.bg_color.green, g_desk.bg_color.blue);
    cairo_paint(cr);

    /* Optional wallpaper */
    if (g_desk.wallpaper_pb) {
        GdkPixbuf *scaled = gdk_pixbuf_scale_simple(g_desk.wallpaper_pb, w, h, GDK_INTERP_BILINEAR);
        if (scaled) {
            gdk_cairo_set_source_pixbuf(cr, scaled, 0, 0);
            cairo_paint(cr);
            g_object_unref(scaled);
        }
    }

    if (!is_primary) return FALSE;

    /* 2. Draw desktop icons on primary monitor */
    PangoLayout *layout = gtk_widget_create_pango_layout(widget, "");
    PangoFontDescription *font_desc = pango_font_description_from_string("Sans 8.5");
    pango_layout_set_font_description(layout, font_desc);
    pango_layout_set_width(layout, (ICON_CELL_W - 6) * PANGO_SCALE);
    pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
    pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);

    for (int i = 0; i < g_desk.icon_count; i++) {
        DesktopIcon *ic = &g_desk.icons[i];
        int img_x = ic->x + (ICON_CELL_W - ICON_IMG_SIZE) / 2;
        int img_y = ic->y + 4;

        if (ic->pixbuf) {
            gdk_cairo_set_source_pixbuf(cr, ic->pixbuf, img_x, img_y);
            cairo_paint(cr);

            /* Classic Win95 navy tint when icon is selected */
            if (i == g_desk.selected_index) {
                cairo_set_source_rgba(cr, 0.0, 0.0, 0.5, 0.35);
                cairo_rectangle(cr, img_x, img_y, ICON_IMG_SIZE, ICON_IMG_SIZE);
                cairo_fill(cr);
            }
        }

        pango_layout_set_text(layout, ic->title, -1);
        int text_w = 0, text_h = 0;
        pango_layout_get_pixel_size(layout, &text_w, &text_h);
        if (text_h > 32) text_h = 32;

        int lbl_x = ic->x + (ICON_CELL_W - text_w) / 2;
        int lbl_y = img_y + ICON_IMG_SIZE + 4;

        if (i == g_desk.selected_index) {
            /* Classic Win95 #000080 navy label box + dotted focus border */
            cairo_set_source_rgb(cr, 0.0, 0.0, 0.502);
            cairo_rectangle(cr, lbl_x - 2, lbl_y - 1, text_w + 4, text_h + 2);
            cairo_fill(cr);

            double dashes[] = { 1.0, 1.0 };
            cairo_save(cr);
            cairo_set_source_rgb(cr, 1.0, 1.0, 0.5);
            cairo_set_line_width(cr, 1.0);
            cairo_set_dash(cr, dashes, 2, 0.0);
            cairo_rectangle(cr, lbl_x - 1.5, lbl_y - 0.5, text_w + 3, text_h + 1);
            cairo_stroke(cr);
            cairo_restore(cr);
        }

        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        cairo_move_to(cr, ic->x + 3, lbl_y);
        pango_cairo_show_layout(cr, layout);
    }

    pango_font_description_free(font_desc);
    g_object_unref(layout);

    /* 3. Draw rubber-band dotted selection box if dragging */
    if (g_desk.dragging_rubberband) {
        double rx = fmin(g_desk.drag_start_x, g_desk.drag_cur_x);
        double ry = fmin(g_desk.drag_start_y, g_desk.drag_cur_y);
        double rw = fabs(g_desk.drag_cur_x - g_desk.drag_start_x);
        double rh = fabs(g_desk.drag_cur_y - g_desk.drag_start_y);

        double dashes[] = { 1.0, 1.0 };
        cairo_save(cr);
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        cairo_set_line_width(cr, 1.0);
        cairo_set_dash(cr, dashes, 2, 0.0);
        cairo_rectangle(cr, floor(rx) + 0.5, floor(ry) + 0.5, floor(rw), floor(rh));
        cairo_stroke(cr);
        cairo_restore(cr);
    }

    return FALSE;
}

static void on_menu_open_icon(GtkMenuItem *item, gpointer user_data) {
    (void)item;
    int idx = GPOINTER_TO_INT(user_data);
    if (idx >= 0 && idx < g_desk.icon_count) {
        chicago_spawn_command(g_desk.icons[idx].command);
    }
}

static void on_menu_empty_trash(GtkMenuItem *item, gpointer user_data) {
    (void)item;
    (void)user_data;
    empty_recycle_bin();
    rebuild_desktop_icons();
}

static int compare_icons_by_name(const void *a, const void *b) {
    const DesktopIcon *ia = (const DesktopIcon *)a;
    const DesktopIcon *ib = (const DesktopIcon *)b;
    return g_ascii_strcasecmp(ia->title, ib->title);
}

static void on_menu_arrange_by_name(GtkMenuItem *item, gpointer user_data) {
    (void)item;
    (void)user_data;
    if (g_desk.icon_count > 1) {
        qsort(g_desk.icons, g_desk.icon_count, sizeof(DesktopIcon), compare_icons_by_name);
        int alloc_h = g_desk.primary_area ? gtk_widget_get_allocated_height(g_desk.primary_area) : 900;
        layout_icons_grid(alloc_h);
        if (g_desk.primary_area) gtk_widget_queue_draw(g_desk.primary_area);
    }
}

static void on_menu_refresh(GtkMenuItem *item, gpointer user_data) {
    (void)item;
    (void)user_data;
    rebuild_desktop_icons();
}

static void on_menu_new_folder(GtkMenuItem *item, gpointer user_data) {
    (void)item;
    (void)user_data;
    char path[512];
    for (int n = 1; n < 50; n++) {
        if (n == 1)
            snprintf(path, sizeof(path), "%s/Desktop/New Folder", g_get_home_dir());
        else
            snprintf(path, sizeof(path), "%s/Desktop/New Folder (%d)", g_get_home_dir(), n);
        if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
            g_mkdir_with_parents(path, 0755);
            break;
        }
    }
    rebuild_desktop_icons();
}

static void on_menu_new_text_doc(GtkMenuItem *item, gpointer user_data) {
    (void)item;
    (void)user_data;
    char path[512];
    for (int n = 1; n < 50; n++) {
        if (n == 1)
            snprintf(path, sizeof(path), "%s/Desktop/New Text Document.txt", g_get_home_dir());
        else
            snprintf(path, sizeof(path), "%s/Desktop/New Text Document (%d).txt", g_get_home_dir(), n);
        if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
            g_file_set_contents(path, "", 0, NULL);
            break;
        }
    }
    rebuild_desktop_icons();
}

static void on_menu_cmd(GtkMenuItem *item, gpointer user_data) {
    (void)item;
    chicago_spawn_command((const char *)user_data);
}

static void show_icon_context_menu(GdkEventButton *event, int idx) {
    GtkWidget *menu = gtk_menu_new();
    DesktopIcon *ic = &g_desk.icons[idx];

    GtkWidget *open_item = gtk_menu_item_new_with_label("Open");
    g_signal_connect(open_item, "activate", G_CALLBACK(on_menu_open_icon), GINT_TO_POINTER(idx));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), open_item);

    if (ic->is_recycle_bin) {
        GtkWidget *empty_item = gtk_menu_item_new_with_label("Empty Recycle Bin");
        gtk_widget_set_sensitive(empty_item, is_trash_non_empty());
        g_signal_connect(empty_item, "activate", G_CALLBACK(on_menu_empty_trash), NULL);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), empty_item);
    }

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

    GtkWidget *prop_item = gtk_menu_item_new_with_label("Properties");
    g_signal_connect(prop_item, "activate", G_CALLBACK(on_menu_cmd), (gpointer)"chicago-control-panel --tab system");
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), prop_item);

    gtk_widget_show_all(menu);
    gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)event);
}

static void show_desktop_context_menu(GdkEventButton *event) {
    GtkWidget *menu = gtk_menu_new();

    GtkWidget *arrange = gtk_menu_item_new_with_label("Arrange Icons by Name");
    g_signal_connect(arrange, "activate", G_CALLBACK(on_menu_arrange_by_name), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), arrange);

    GtkWidget *lineup = gtk_menu_item_new_with_label("Line Up Icons");
    g_signal_connect(lineup, "activate", G_CALLBACK(on_menu_refresh), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), lineup);

    GtkWidget *refresh = gtk_menu_item_new_with_label("Refresh");
    g_signal_connect(refresh, "activate", G_CALLBACK(on_menu_refresh), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), refresh);

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

    /* New -> Submenu */
    GtkWidget *new_item = gtk_menu_item_new_with_label("New");
    GtkWidget *new_sub = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(new_item), new_sub);

    GtkWidget *new_folder = gtk_menu_item_new_with_label("Folder");
    g_signal_connect(new_folder, "activate", G_CALLBACK(on_menu_new_folder), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(new_sub), new_folder);

    GtkWidget *new_txt = gtk_menu_item_new_with_label("Text Document");
    g_signal_connect(new_txt, "activate", G_CALLBACK(on_menu_new_text_doc), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(new_sub), new_txt);

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), new_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

    GtkWidget *dos = gtk_menu_item_new_with_label("MS-DOS Prompt Here");
    g_signal_connect(dos, "activate", G_CALLBACK(on_menu_cmd),
                     (gpointer)"kitty --title \"MS-DOS Prompt\" -o background=#000000 -o foreground=#c0c0c0");
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), dos);

    GtkWidget *run = gtk_menu_item_new_with_label("Run...");
    g_signal_connect(run, "activate", G_CALLBACK(on_menu_cmd), (gpointer)"chicago-control-panel --run");
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), run);

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

    GtkWidget *props = gtk_menu_item_new_with_label("Properties");
    g_signal_connect(props, "activate", G_CALLBACK(on_menu_cmd), (gpointer)"chicago-control-panel --tab display");
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), props);

    gtk_widget_show_all(menu);
    gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)event);
}

static gboolean on_button_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    gboolean is_primary = GPOINTER_TO_INT(user_data);
    gtk_widget_grab_focus(widget);

    int hit = is_primary ? icon_at_point(event->x, event->y) : -1;

    if (event->button == 1) {
        if (event->type == GDK_2BUTTON_PRESS && hit >= 0) {
            chicago_spawn_command(g_desk.icons[hit].command);
            return TRUE;
        }
        g_desk.selected_index = hit;
        if (hit < 0 && is_primary) {
            g_desk.dragging_rubberband = TRUE;
            g_desk.drag_start_x = g_desk.drag_cur_x = event->x;
            g_desk.drag_start_y = g_desk.drag_cur_y = event->y;
        }
        gtk_widget_queue_draw(widget);
        return TRUE;
    } else if (event->button == 3) {
        if (hit >= 0) {
            g_desk.selected_index = hit;
            gtk_widget_queue_draw(widget);
            show_icon_context_menu(event, hit);
        } else {
            g_desk.selected_index = -1;
            gtk_widget_queue_draw(widget);
            show_desktop_context_menu(event);
        }
        return TRUE;
    }
    return FALSE;
}

static gboolean on_button_release(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    (void)user_data;
    if (event->button == 1 && g_desk.dragging_rubberband) {
        g_desk.dragging_rubberband = FALSE;
        gtk_widget_queue_draw(widget);
        return TRUE;
    }
    return FALSE;
}

static gboolean on_motion_notify(GtkWidget *widget, GdkEventMotion *event, gpointer user_data) {
    (void)user_data;
    if (g_desk.dragging_rubberband) {
        g_desk.drag_cur_x = event->x;
        g_desk.drag_cur_y = event->y;
        gtk_widget_queue_draw(widget);
        return TRUE;
    }
    return FALSE;
}

static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    (void)user_data;
    if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter) {
        if (g_desk.selected_index >= 0 && g_desk.selected_index < g_desk.icon_count) {
            chicago_spawn_command(g_desk.icons[g_desk.selected_index].command);
            return TRUE;
        }
    } else if (event->keyval == GDK_KEY_Down) {
        if (g_desk.icon_count > 0) {
            g_desk.selected_index = (g_desk.selected_index + 1) % g_desk.icon_count;
            gtk_widget_queue_draw(widget);
            return TRUE;
        }
    } else if (event->keyval == GDK_KEY_Up) {
        if (g_desk.icon_count > 0) {
            g_desk.selected_index = (g_desk.selected_index - 1 + g_desk.icon_count) % g_desk.icon_count;
            gtk_widget_queue_draw(widget);
            return TRUE;
        }
    } else if (event->keyval == GDK_KEY_F5) {
        rebuild_desktop_icons();
        return TRUE;
    }
    return FALSE;
}

static void on_fs_changed(GFileMonitor *monitor, GFile *file, GFile *other_file,
                          GFileMonitorEvent event_type, gpointer user_data) {
    (void)monitor;
    (void)file;
    (void)other_file;
    (void)user_data;
    if (event_type == G_FILE_MONITOR_EVENT_CREATED ||
        event_type == G_FILE_MONITOR_EVENT_DELETED ||
        event_type == G_FILE_MONITOR_EVENT_MOVED_IN ||
        event_type == G_FILE_MONITOR_EVENT_MOVED_OUT) {
        rebuild_desktop_icons();
    }
}

static void create_desktop_window_for_monitor(GdkMonitor *monitor, gboolean is_primary) {
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Chicago95 Desktop");
    gtk_window_set_decorated(GTK_WINDOW(win), FALSE);

    gtk_layer_init_for_window(GTK_WINDOW(win));
    gtk_layer_set_layer(GTK_WINDOW(win), GTK_LAYER_SHELL_LAYER_BACKGROUND);
    gtk_layer_set_namespace(GTK_WINDOW(win), "chicago-desktop");
    if (monitor) {
        gtk_layer_set_monitor(GTK_WINDOW(win), monitor);
    }
    gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
    gtk_layer_set_exclusive_zone(GTK_WINDOW(win), -1);
    gtk_layer_set_keyboard_mode(GTK_WINDOW(win), GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);

    GtkWidget *area = gtk_drawing_area_new();
    gtk_widget_set_can_focus(area, TRUE);
    gtk_widget_add_events(area,
                          GDK_BUTTON_PRESS_MASK |
                          GDK_BUTTON_RELEASE_MASK |
                          GDK_POINTER_MOTION_MASK |
                          GDK_KEY_PRESS_MASK);

    g_signal_connect(area, "draw", G_CALLBACK(on_desktop_draw), GINT_TO_POINTER(is_primary));
    g_signal_connect(area, "button-press-event", G_CALLBACK(on_button_press), GINT_TO_POINTER(is_primary));
    g_signal_connect(area, "button-release-event", G_CALLBACK(on_button_release), GINT_TO_POINTER(is_primary));
    g_signal_connect(area, "motion-notify-event", G_CALLBACK(on_motion_notify), GINT_TO_POINTER(is_primary));
    g_signal_connect(area, "key-press-event", G_CALLBACK(on_key_press), GINT_TO_POINTER(is_primary));

    if (is_primary) {
        g_desk.primary_area = area;
    }

    gtk_container_add(GTK_CONTAINER(win), area);
    gtk_widget_show_all(win);
}

int main(int argc, char **argv) {
    gtk_init(&argc, &argv);
    chicago_apply_theme();

    g_desk.selected_index = -1;
    /* Default Windows 95 Teal: #008080 */
    gdk_rgba_parse(&g_desk.bg_color, "#008080");

    const char *env_color = g_getenv("CHICAGO_BG_COLOR");
    if (env_color && *env_color) {
        gdk_rgba_parse(&g_desk.bg_color, env_color);
    }

    const char *wallpaper_path = g_getenv("CHICAGO_WALLPAPER");
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--color") == 0 && i + 1 < argc) {
            gdk_rgba_parse(&g_desk.bg_color, argv[++i]);
        } else if (strcmp(argv[i], "--wallpaper") == 0 && i + 1 < argc) {
            wallpaper_path = argv[++i];
        }
    }

    if (wallpaper_path && *wallpaper_path && g_file_test(wallpaper_path, G_FILE_TEST_EXISTS)) {
        g_desk.wallpaper_pb = gdk_pixbuf_new_from_file(wallpaper_path, NULL);
    }

    rebuild_desktop_icons();

    /* Setup event-driven inotify file monitors on ~/Desktop and ~/.local/share/Trash/files */
    char path_buf[512];
    snprintf(path_buf, sizeof(path_buf), "%s/Desktop", g_get_home_dir());
    g_mkdir_with_parents(path_buf, 0755);
    GFile *desk_file = g_file_new_for_path(path_buf);
    g_desk.desktop_monitor = g_file_monitor_directory(desk_file, G_FILE_MONITOR_NONE, NULL, NULL);
    if (g_desk.desktop_monitor) {
        g_signal_connect(g_desk.desktop_monitor, "changed", G_CALLBACK(on_fs_changed), NULL);
    }
    g_object_unref(desk_file);

    snprintf(path_buf, sizeof(path_buf), "%s/.local/share/Trash/files", g_get_home_dir());
    g_mkdir_with_parents(path_buf, 0700);
    GFile *trash_file = g_file_new_for_path(path_buf);
    g_desk.trash_monitor = g_file_monitor_directory(trash_file, G_FILE_MONITOR_NONE, NULL, NULL);
    if (g_desk.trash_monitor) {
        g_signal_connect(g_desk.trash_monitor, "changed", G_CALLBACK(on_fs_changed), NULL);
    }
    g_object_unref(trash_file);

    GdkDisplay *display = gdk_display_get_default();
    int n_monitors = display ? gdk_display_get_n_monitors(display) : 0;
    if (n_monitors > 0) {
        for (int i = 0; i < n_monitors; i++) {
            GdkMonitor *mon = gdk_display_get_monitor(display, i);
            create_desktop_window_for_monitor(mon, i == 0);
        }
    } else {
        create_desktop_window_for_monitor(NULL, TRUE);
    }

    gtk_main();
    return 0;
}
