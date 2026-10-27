/*
 * chicago-taskbar: Authentic Windows 95 Multi-Monitor Taskbar, Hover-Cascading Start Menu & Tray
 *
 * Features:
 * - Multi-monitor 28px bottom taskbar (gtk-layer-shell LAYER_TOP, exclusive_zone=28 on every monitor)
 * - Authentic Start button (windows_16x16px.png + bold Helvetica 8pt) + Quick Launch + Window List + Tray
 * - Native zwlr_foreign_toplevel_manager_v1 client syncing window buttons across all monitors
 * - Hover-cascading Start Menu (Programs ▸, Documents ▸, Settings ▸) using layer-shell flyout surfaces
 * - Authentic Chicago95 branding_W95.png vertical sidebar banner
 * - Interactive bottom-right System Tray popups:
 *     1. Volume Control (vertical slider + Mute checkbox + Audio Properties)
 *     2. Network & Wi-Fi (live SSID/IP status + Wi-Fi list + Network Properties)
 *     3. Power & Brightness (battery meter + live brightnessctl slider + power profile + Power Properties)
 *     4. Date/Time Calendar popup
 * - SIGUSR1 toggles Start Menu; SIGUSR2 toggles Show Desktop (minimize/restore all)
 * - Minute-aligned clock timer (0.017 wakeups/sec at idle)
 */

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkwayland.h>
#include <gdk/gdkkeysyms.h>
#include <gtk-layer-shell/gtk-layer-shell.h>
#include <wayland-client.h>
#include <glib-unix.h>
#include <signal.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/chicago_common.h"
#include "wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"

#define MAX_BAR_MONITORS 8

typedef struct {
    struct zwlr_foreign_toplevel_handle_v1 *handle;
    char *title;
    char *app_id;
    gboolean activated;
    gboolean minimized;
    gboolean maximized;
    GtkWidget *buttons[MAX_BAR_MONITORS];
    GtkWidget *labels[MAX_BAR_MONITORS];
    GtkWidget *icons[MAX_BAR_MONITORS];
} ToplevelItem;

typedef ChicagoAppEntry DesktopAppEntry;

typedef struct {
    GdkMonitor *monitor;
    GtkWidget *bar_window;
    GtkWidget *start_button;
    GtkWidget *pins_box;
    GtkWidget *task_box;
    GtkWidget *clock_label;
} BarInstance;

typedef struct {
    BarInstance bars[MAX_BAR_MONITORS];
    int bar_count;
    GdkMonitor *active_popup_monitor;

    /* Hover-cascading Start Menu & Flyout Layer Surfaces */
    GtkWidget *start_menu_win;
    GtkWidget *search_entry;
    GtkWidget *flyout_l1_win;
    GtkWidget *flyout_l1_box;
    GtkWidget *flyout_l2_win;
    GtkWidget *flyout_l2_box;
    gboolean start_menu_visible;
    GdkPixbuf *banner_pb;

    /* System Tray Interactive Popups */
    GtkWidget *vol_popup;
    GtkWidget *vol_scale;
    GtkWidget *vol_mute_chk;
    gboolean vol_updating;

    GtkWidget *net_popup;
    GtkWidget *net_status_lbl;
    GtkWidget *net_wifi_list_box;

    GtkWidget *pwr_popup;
    GtkWidget *pwr_status_lbl;
    GtkWidget *pwr_bright_scale;
    gboolean pwr_updating;

    GtkWidget *cal_popup;

    struct wl_display *wl_display;
    struct wl_registry *wl_registry;
    struct wl_seat *wl_seat;
    struct zwlr_foreign_toplevel_manager_v1 *toplevel_mgr;

    GList *toplevels;
    GList *app_entries;
    GFileMonitor *pins_monitor;
    gboolean desktop_shown_minimized;
} TaskbarApp;

static TaskbarApp g_tb;

static void hide_all_popups(void);
static void set_start_menu_visible(gboolean visible, GdkMonitor *target_mon);

/* =========================================================================
 * System Helpers (Audio, Brightness, Network, Power)
 * ========================================================================= */

static int get_current_volume(gboolean *out_muted) {
    if (out_muted) *out_muted = FALSE;
    char *stdout_str = NULL;
    if (g_spawn_command_line_sync("wpctl get-volume @DEFAULT_AUDIO_SINK@", &stdout_str, NULL, NULL, NULL) && stdout_str) {
        if (strstr(stdout_str, "[MUTED]") && out_muted) *out_muted = TRUE;
        char *p = strstr(stdout_str, "Volume:");
        if (p) {
            double v = g_ascii_strtod(p + 7, NULL);
            g_free(stdout_str);
            return CLAMP((int)(v * 100.0 + 0.5), 0, 100);
        }
        g_free(stdout_str);
    }
    return 75;
}

static int get_current_brightness_pct(void) {
    char *cur_s = NULL, *max_s = NULL;
    int pct = 50;
    if (g_spawn_command_line_sync("brightnessctl get", &cur_s, NULL, NULL, NULL) &&
        g_spawn_command_line_sync("brightnessctl max", &max_s, NULL, NULL, NULL) &&
        cur_s && max_s) {
        double c = g_ascii_strtod(cur_s, NULL);
        double m = g_ascii_strtod(max_s, NULL);
        if (m > 0) pct = CLAMP((int)((c / m) * 100.0 + 0.5), 5, 100);
    }
    g_free(cur_s);
    g_free(max_s);
    return pct;
}

/* =========================================================================
 * Wayland Foreign Toplevel Protocol Implementation
 * ========================================================================= */

static void update_toplevel_widgets(ToplevelItem *item) {
    const char *display_title = (item->title && *item->title) ? item->title :
                                ((item->app_id && *item->app_id) ? item->app_id : "Window");
    const char *icon_name = (item->app_id && *item->app_id) ? item->app_id : "application-x-executable";
    if (g_ascii_strcasecmp(icon_name, "kitty") == 0) {
        icon_name = "utilities-terminal";
    } else if (g_ascii_strcasecmp(icon_name, "thunar") == 0) {
        icon_name = "system-file-manager";
    } else if (g_str_has_prefix(icon_name, "chicago-control-panel")) {
        icon_name = "preferences-system";
    }

    for (int i = 0; i < g_tb.bar_count; i++) {
        if (!item->buttons[i]) continue;
        gtk_label_set_text(GTK_LABEL(item->labels[i]), display_title);
        gtk_widget_set_tooltip_text(item->buttons[i], display_title);

        GdkPixbuf *pb = chicago_load_icon(icon_name, 16);
        if (pb) {
            gtk_image_set_from_pixbuf(GTK_IMAGE(item->icons[i]), pb);
            g_object_unref(pb);
        }

        GtkStyleContext *ctx = gtk_widget_get_style_context(item->buttons[i]);
        if (item->activated && !item->minimized) {
            gtk_style_context_add_class(ctx, "active-task");
        } else {
            gtk_style_context_remove_class(ctx, "active-task");
        }
    }
}

static void on_task_btn_clicked(GtkButton *btn, gpointer data) {
    (void)btn;
    ToplevelItem *item = (ToplevelItem *)data;
    if (!item || !item->handle) return;

    hide_all_popups();

    if (item->minimized) {
        zwlr_foreign_toplevel_handle_v1_unset_minimized(item->handle);
        if (g_tb.wl_seat) {
            zwlr_foreign_toplevel_handle_v1_activate(item->handle, g_tb.wl_seat);
        }
    } else if (item->activated) {
        zwlr_foreign_toplevel_handle_v1_set_minimized(item->handle);
    } else {
        if (g_tb.wl_seat) {
            zwlr_foreign_toplevel_handle_v1_activate(item->handle, g_tb.wl_seat);
        }
    }
    wl_display_flush(g_tb.wl_display);
}

static void on_task_ctx_restore(GtkWidget *btn, gpointer data) {
    (void)btn;
    ToplevelItem *item = (ToplevelItem *)data;
    if (!item || !item->handle) return;
    zwlr_foreign_toplevel_handle_v1_unset_minimized(item->handle);
    zwlr_foreign_toplevel_handle_v1_unset_maximized(item->handle);
    if (g_tb.wl_seat) zwlr_foreign_toplevel_handle_v1_activate(item->handle, g_tb.wl_seat);
    wl_display_flush(g_tb.wl_display);
}

static void on_task_ctx_minimize(GtkWidget *btn, gpointer data) {
    (void)btn;
    ToplevelItem *item = (ToplevelItem *)data;
    if (!item || !item->handle) return;
    zwlr_foreign_toplevel_handle_v1_set_minimized(item->handle);
    wl_display_flush(g_tb.wl_display);
}

static void on_task_ctx_maximize(GtkWidget *btn, gpointer data) {
    (void)btn;
    ToplevelItem *item = (ToplevelItem *)data;
    if (!item || !item->handle) return;
    zwlr_foreign_toplevel_handle_v1_unset_minimized(item->handle);
    zwlr_foreign_toplevel_handle_v1_set_maximized(item->handle);
    wl_display_flush(g_tb.wl_display);
}

static void on_task_ctx_close(GtkWidget *btn, gpointer data) {
    (void)btn;
    ToplevelItem *item = (ToplevelItem *)data;
    if (!item || !item->handle) return;
    zwlr_foreign_toplevel_handle_v1_close(item->handle);
    wl_display_flush(g_tb.wl_display);
}

static DesktopAppEntry *find_matching_app_for_toplevel(ToplevelItem *item) {
    if (!item) return NULL;
    for (GList *l = g_tb.app_entries; l != NULL; l = l->next) {
        DesktopAppEntry *e = (DesktopAppEntry *)l->data;
        if (item->app_id && *item->app_id) {
            if (e->icon && g_ascii_strcasecmp(e->icon, item->app_id) == 0) return e;
            if (e->name && g_ascii_strcasecmp(e->name, item->app_id) == 0) return e;
            if (e->desktop_path && strstr(e->desktop_path, item->app_id) != NULL) return e;
            if (e->exec && strstr(e->exec, item->app_id) != NULL) return e;
        }
        if (item->title && *item->title && e->name && g_ascii_strcasecmp(e->name, item->title) == 0) {
            return e;
        }
    }
    return NULL;
}

static void resolve_toplevel_app_info(ToplevelItem *item, const char **out_name, const char **out_icon, const char **out_exec, const char **out_path) {
    DesktopAppEntry *e = find_matching_app_for_toplevel(item);
    if (e) {
        *out_name = e->name;
        *out_icon = e->icon;
        *out_exec = e->exec;
        *out_path = e->desktop_path;
        return;
    }
    *out_name = (item->title && *item->title) ? item->title : ((item->app_id && *item->app_id) ? item->app_id : "Application");
    *out_icon = (item->app_id && *item->app_id) ? item->app_id : "application-x-executable";
    *out_exec = (item->app_id && *item->app_id) ? item->app_id : "";
    *out_path = "";
}

static void on_task_ctx_toggle_pin(GtkWidget *btn, gpointer data) {
    (void)btn;
    ToplevelItem *item = (ToplevelItem *)data;
    const char *name, *icon, *exec, *path;
    resolve_toplevel_app_info(item, &name, &icon, &exec, &path);
    if (!exec || !*exec) return;
    if (chicago_pins_is_pinned(name, exec)) {
        chicago_pins_remove(name, exec);
    } else {
        chicago_pins_add(name, icon, exec, path);
    }
}

static void on_task_ctx_relaunch_dgpu(GtkWidget *btn, gpointer data) {
    (void)btn;
    ToplevelItem *item = (ToplevelItem *)data;
    const char *name, *icon, *exec, *path;
    resolve_toplevel_app_info(item, &name, &icon, &exec, &path);
    if (exec && *exec) {
        if (item->handle) {
            zwlr_foreign_toplevel_handle_v1_close(item->handle);
            wl_display_flush(g_tb.wl_display);
        }
        chicago_launch_with_dgpu(exec);
    }
}

static void on_task_ctx_desktop_shortcut(GtkWidget *btn, gpointer data) {
    (void)btn;
    ToplevelItem *item = (ToplevelItem *)data;
    const char *name, *icon, *exec, *path;
    resolve_toplevel_app_info(item, &name, &icon, &exec, &path);
    if (exec && *exec) {
        chicago_create_desktop_shortcut(name, icon, exec, FALSE);
    }
}

static void on_task_ctx_properties(GtkWidget *btn, gpointer data) {
    (void)btn;
    ToplevelItem *item = (ToplevelItem *)data;
    const char *name, *icon, *exec, *path;
    resolve_toplevel_app_info(item, &name, &icon, &exec, &path);
    chicago_show_app_properties_dialog(name, icon, exec, path);
}

static gboolean on_task_btn_press(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    ToplevelItem *item = (ToplevelItem *)data;
    if (event->button == 3 && item) {
        hide_all_popups();

        GdkMonitor *mon = NULL;
        int left = 100, edge = 28;
        gboolean anchor_bottom = TRUE;
        chicago_resolve_event_layer_coords(widget, event, &mon, &left, &edge, &anchor_bottom);

        GtkWidget *vbox = NULL;
        GtkWidget *win = chicago_ctx_menu_begin(mon, left, edge, anchor_bottom, &vbox);

        chicago_ctx_menu_add_item(vbox, "Restore", "window-restore", FALSE, G_CALLBACK(on_task_ctx_restore), item);
        chicago_ctx_menu_add_item(vbox, "Minimize", "window-minimize", FALSE, G_CALLBACK(on_task_ctx_minimize), item);
        chicago_ctx_menu_add_item(vbox, "Maximize", "window-maximize", FALSE, G_CALLBACK(on_task_ctx_maximize), item);

        chicago_ctx_menu_add_separator(vbox);

        const char *name, *icon, *exec, *path;
        resolve_toplevel_app_info(item, &name, &icon, &exec, &path);
        gboolean pinned = chicago_pins_is_pinned(name, exec);

        chicago_ctx_menu_add_item(vbox, pinned ? "Unpin from Taskbar" : "Pin to Taskbar",
                                  "preferences-desktop", FALSE, G_CALLBACK(on_task_ctx_toggle_pin), item);
        chicago_ctx_menu_add_item(vbox, "Relaunch on Dedicated GPU (NVIDIA Quadro T1000)",
                                  "preferences-desktop-display", FALSE, G_CALLBACK(on_task_ctx_relaunch_dgpu), item);
        chicago_ctx_menu_add_item(vbox, "Create Shortcut on Desktop",
                                  "user-desktop", FALSE, G_CALLBACK(on_task_ctx_desktop_shortcut), item);
        chicago_ctx_menu_add_item(vbox, "Properties",
                                  "document-properties", FALSE, G_CALLBACK(on_task_ctx_properties), item);

        chicago_ctx_menu_add_separator(vbox);

        chicago_ctx_menu_add_item(vbox, "Close (Alt+F4)", "window-close", TRUE, G_CALLBACK(on_task_ctx_close), item);

        chicago_ctx_menu_show(win, mon, left, edge, anchor_bottom);
        return TRUE;
    }
    return FALSE;
}

static void tl_handle_title(void *data, struct zwlr_foreign_toplevel_handle_v1 *h, const char *title) {
    (void)h;
    ToplevelItem *item = (ToplevelItem *)data;
    g_free(item->title);
    item->title = g_strdup(title ? title : "");
}

static void tl_handle_app_id(void *data, struct zwlr_foreign_toplevel_handle_v1 *h, const char *app_id) {
    (void)h;
    ToplevelItem *item = (ToplevelItem *)data;
    g_free(item->app_id);
    item->app_id = g_strdup(app_id ? app_id : "");
}

static void tl_handle_output_enter(void *data, struct zwlr_foreign_toplevel_handle_v1 *h, struct wl_output *o) {
    (void)data; (void)h; (void)o;
}

static void tl_handle_output_leave(void *data, struct zwlr_foreign_toplevel_handle_v1 *h, struct wl_output *o) {
    (void)data; (void)h; (void)o;
}

static void tl_handle_state(void *data, struct zwlr_foreign_toplevel_handle_v1 *h, struct wl_array *state) {
    (void)h;
    ToplevelItem *item = (ToplevelItem *)data;
    item->activated = FALSE;
    item->minimized = FALSE;
    item->maximized = FALSE;

    uint32_t *entry;
    wl_array_for_each(entry, state) {
        if (*entry == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED)
            item->activated = TRUE;
        else if (*entry == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MINIMIZED)
            item->minimized = TRUE;
        else if (*entry == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MAXIMIZED)
            item->maximized = TRUE;
    }
}

static void tl_handle_done(void *data, struct zwlr_foreign_toplevel_handle_v1 *h) {
    (void)h;
    ToplevelItem *item = (ToplevelItem *)data;
    update_toplevel_widgets(item);
}

static void tl_handle_closed(void *data, struct zwlr_foreign_toplevel_handle_v1 *h) {
    ToplevelItem *item = (ToplevelItem *)data;
    g_tb.toplevels = g_list_remove(g_tb.toplevels, item);
    for (int i = 0; i < g_tb.bar_count; i++) {
        if (item->buttons[i]) {
            gtk_widget_destroy(item->buttons[i]);
            item->buttons[i] = NULL;
        }
    }
    zwlr_foreign_toplevel_handle_v1_destroy(h);
    g_free(item->title);
    g_free(item->app_id);
    g_free(item);
}

static void tl_handle_parent(void *data, struct zwlr_foreign_toplevel_handle_v1 *h, struct zwlr_foreign_toplevel_handle_v1 *p) {
    (void)data; (void)h; (void)p;
}

static const struct zwlr_foreign_toplevel_handle_v1_listener toplevel_handle_listener = {
    .title = tl_handle_title,
    .app_id = tl_handle_app_id,
    .output_enter = tl_handle_output_enter,
    .output_leave = tl_handle_output_leave,
    .state = tl_handle_state,
    .done = tl_handle_done,
    .closed = tl_handle_closed,
    .parent = tl_handle_parent,
};

static void mgr_handle_toplevel(void *data, struct zwlr_foreign_toplevel_manager_v1 *mgr,
                                struct zwlr_foreign_toplevel_handle_v1 *handle) {
    (void)data; (void)mgr;
    ToplevelItem *item = g_malloc0(sizeof(ToplevelItem));
    item->handle = handle;
    item->title = g_strdup("Window");
    item->app_id = g_strdup("application-x-executable");

    for (int i = 0; i < g_tb.bar_count; i++) {
        GtkWidget *btn = gtk_button_new();
        gtk_style_context_add_class(gtk_widget_get_style_context(btn), "task-btn");
        gtk_widget_add_events(btn, GDK_BUTTON_PRESS_MASK);
        gtk_widget_set_size_request(btn, 150, 22);

        GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
        GdkPixbuf *pb = chicago_load_icon("application-x-executable", 16);
        GtkWidget *img = gtk_image_new_from_pixbuf(pb);
        if (pb) g_object_unref(pb);

        GtkWidget *lbl = gtk_label_new("Window");
        gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
        gtk_label_set_ellipsize(GTK_LABEL(lbl), PANGO_ELLIPSIZE_END);
        gtk_label_set_max_width_chars(GTK_LABEL(lbl), 18);

        gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 1);
        gtk_box_pack_start(GTK_BOX(hbox), lbl, TRUE, TRUE, 1);
        gtk_container_add(GTK_CONTAINER(btn), hbox);

        g_signal_connect(btn, "clicked", G_CALLBACK(on_task_btn_clicked), item);
        g_signal_connect(btn, "button-press-event", G_CALLBACK(on_task_btn_press), item);

        gtk_box_pack_start(GTK_BOX(g_tb.bars[i].task_box), btn, FALSE, FALSE, 1);
        gtk_widget_show_all(btn);

        item->buttons[i] = btn;
        item->labels[i] = lbl;
        item->icons[i] = img;
    }

    g_tb.toplevels = g_list_append(g_tb.toplevels, item);
    zwlr_foreign_toplevel_handle_v1_add_listener(handle, &toplevel_handle_listener, item);
}

static void mgr_handle_finished(void *data, struct zwlr_foreign_toplevel_manager_v1 *mgr) {
    (void)data;
    zwlr_foreign_toplevel_manager_v1_destroy(mgr);
    g_tb.toplevel_mgr = NULL;
}

static const struct zwlr_foreign_toplevel_manager_v1_listener toplevel_mgr_listener = {
    .toplevel = mgr_handle_toplevel,
    .finished = mgr_handle_finished,
};

static void registry_global(void *data, struct wl_registry *reg, uint32_t name, const char *interface, uint32_t version) {
    (void)data;
    if (strcmp(interface, zwlr_foreign_toplevel_manager_v1_interface.name) == 0) {
        uint32_t bind_ver = MIN(version, 3u);
        g_tb.toplevel_mgr = wl_registry_bind(reg, name, &zwlr_foreign_toplevel_manager_v1_interface, bind_ver);
        zwlr_foreign_toplevel_manager_v1_add_listener(g_tb.toplevel_mgr, &toplevel_mgr_listener, NULL);
    } else if (strcmp(interface, wl_seat_interface.name) == 0 && !g_tb.wl_seat) {
        g_tb.wl_seat = wl_registry_bind(reg, name, &wl_seat_interface, 1);
    }
}

static void registry_global_remove(void *data, struct wl_registry *reg, uint32_t name) {
    (void)data; (void)reg; (void)name;
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

/* =========================================================================
 * Hover-Cascading Windows 95 Start Menu & Flyout Implementation
 * ========================================================================= */

static void scan_desktop_applications(void) {
    if (g_tb.app_entries) {
        g_list_free_full(g_tb.app_entries, chicago_app_entry_free);
        g_tb.app_entries = NULL;
    }
    g_tb.app_entries = chicago_scan_all_desktop_apps();
}

static void clear_container(GtkWidget *container) {
    GList *children = gtk_container_get_children(GTK_CONTAINER(container));
    for (GList *l = children; l != NULL; l = l->next) {
        gtk_widget_destroy(GTK_WIDGET(l->data));
    }
    g_list_free(children);
}

static void free_closure_str(gpointer data, GClosure *closure) {
    (void)closure;
    g_free(data);
}

static void on_launch_cmd_btn(GtkWidget *btn, gpointer data) {
    (void)btn;
    const char *cmd = (const char *)data;
    hide_all_popups();
    chicago_spawn_command(cmd);
}

typedef struct {
    char *name;
    char *icon;
    char *exec;
    char *desktop_path;
} AppRowCtx;

static void app_row_ctx_free(gpointer data, GClosure *closure) {
    (void)closure;
    AppRowCtx *rc = (AppRowCtx *)data;
    if (!rc) return;
    g_free(rc->name);
    g_free(rc->icon);
    g_free(rc->exec);
    g_free(rc->desktop_path);
    g_free(rc);
}

static gboolean on_app_row_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    AppRowCtx *rc = (AppRowCtx *)data;
    if (event->button == 3 && rc && rc->exec && *rc->exec) {
        chicago_popup_app_context_menu_for_widget(widget, rc->name, rc->icon, rc->exec, rc->desktop_path, event);
        return TRUE;
    }
    return FALSE;
}

static GtkWidget *make_flyout_row_ex(const char *label_text, const char *icon_name, const char *cmd, const char *desktop_path, gboolean has_arrow) {
    GtkWidget *btn = gtk_button_new();
    gtk_style_context_add_class(gtk_widget_get_style_context(btn), "win95-menuitem");
    gtk_widget_add_events(btn, GDK_ENTER_NOTIFY_MASK | GDK_BUTTON_PRESS_MASK);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GdkPixbuf *pb = chicago_load_icon(icon_name, 16);
    GtkWidget *img = gtk_image_new_from_pixbuf(pb);
    if (pb) g_object_unref(pb);

    GtkWidget *lbl = gtk_label_new(label_text);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
    gtk_label_set_max_width_chars(GTK_LABEL(lbl), 28);
    gtk_label_set_ellipsize(GTK_LABEL(lbl), PANGO_ELLIPSIZE_END);

    gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(hbox), lbl, TRUE, TRUE, 2);

    if (has_arrow) {
        GtkWidget *arrow = gtk_label_new("▸");
        gtk_box_pack_end(GTK_BOX(hbox), arrow, FALSE, FALSE, 2);
    }

    gtk_container_add(GTK_CONTAINER(btn), hbox);

    if (cmd) {
        g_signal_connect_data(btn, "clicked", G_CALLBACK(on_launch_cmd_btn),
                              g_strdup(cmd), free_closure_str, 0);
        AppRowCtx *rc = g_malloc0(sizeof(AppRowCtx));
        rc->name = g_strdup(label_text ? label_text : "App");
        rc->icon = g_strdup(icon_name ? icon_name : "application-x-executable");
        rc->exec = g_strdup(cmd);
        rc->desktop_path = g_strdup(desktop_path ? desktop_path : "");
        g_signal_connect_data(btn, "button-press-event", G_CALLBACK(on_app_row_button_press),
                              rc, app_row_ctx_free, 0);
    }
    return btn;
}

static GtkWidget *make_flyout_row(const char *label_text, const char *icon_name, const char *cmd, gboolean has_arrow) {
    return make_flyout_row_ex(label_text, icon_name, cmd, "", has_arrow);
}

/* Hovering a category inside Programs ▸ cascades open Flyout Level 2 */
static gboolean on_hover_program_category(GtkWidget *widget, GdkEventCrossing *event, gpointer data) {
    (void)widget; (void)event;
    if (chicago_is_context_menu_visible()) return FALSE;
    const char *cat_name = (const char *)data;
    if (!g_tb.flyout_l2_win || !g_tb.flyout_l2_box) return FALSE;

    clear_container(g_tb.flyout_l2_box);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scroll, 230, 300);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
    int count = 0;
    gboolean show_all = (g_strcmp0(cat_name, "All Programs") == 0);
    for (GList *l = g_tb.app_entries; l != NULL; l = l->next) {
        DesktopAppEntry *e = (DesktopAppEntry *)l->data;
        if (show_all || g_strcmp0(e->category, cat_name) == 0) {
            GtkWidget *row = make_flyout_row_ex(e->name, e->icon, e->exec, e->desktop_path, FALSE);
            gtk_box_pack_start(GTK_BOX(vbox), row, FALSE, FALSE, 0);
            count++;
        }
    }
    if (count == 0) {
        GtkWidget *empty = gtk_label_new("(Empty)");
        gtk_box_pack_start(GTK_BOX(vbox), empty, FALSE, FALSE, 4);
    }

    gtk_container_add(GTK_CONTAINER(scroll), vbox);
    gtk_box_pack_start(GTK_BOX(g_tb.flyout_l2_box), scroll, TRUE, TRUE, 0);

    if (gtk_layer_is_supported() && g_tb.active_popup_monitor) {
        gtk_layer_set_monitor(GTK_WINDOW(g_tb.flyout_l2_win), g_tb.active_popup_monitor);
        g_object_set_data(G_OBJECT(g_tb.flyout_l2_win), "chicago-monitor", g_tb.active_popup_monitor);
        gtk_layer_set_margin(GTK_WINDOW(g_tb.flyout_l2_win), GTK_LAYER_SHELL_EDGE_LEFT, 398);
        gtk_layer_set_margin(GTK_WINDOW(g_tb.flyout_l2_win), GTK_LAYER_SHELL_EDGE_BOTTOM, 120);
    }
    gtk_widget_show_all(g_tb.flyout_l2_win);
    return FALSE;
}

static gboolean on_hover_close_l2(GtkWidget *widget, GdkEventCrossing *event, gpointer data) {
    (void)widget; (void)event; (void)data;
    if (chicago_is_context_menu_visible()) return FALSE;
    if (g_tb.flyout_l2_win) gtk_widget_hide(g_tb.flyout_l2_win);
    return FALSE;
}

/* Hovering "Programs ▸" in the main Start Menu cascades open Flyout Level 1 */
static gboolean on_hover_start_programs(GtkWidget *widget, GdkEventCrossing *event, gpointer data) {
    (void)widget; (void)event; (void)data;
    if (chicago_is_context_menu_visible()) return FALSE;
    if (!g_tb.flyout_l1_win || !g_tb.flyout_l1_box) return FALSE;
    if (g_tb.flyout_l2_win) gtk_widget_hide(g_tb.flyout_l2_win);

    clear_container(g_tb.flyout_l1_box);

    const char *categories[] = {
        "All Programs",
        "Multimedia", "Internet", "Games", "Development",
        "Graphics", "Office", "System Tools", "Settings", "Accessories", NULL
    };
    const char *cat_icons[] = {
        "folder",
        "applications-multimedia", "applications-internet", "applications-games", "applications-development",
        "applications-graphics", "applications-office", "applications-system", "preferences-system", "applications-accessories", NULL
    };

    for (int i = 0; categories[i] != NULL; i++) {
        GtkWidget *row = make_flyout_row(categories[i], cat_icons[i], NULL, TRUE);
        g_signal_connect(row, "enter-notify-event", G_CALLBACK(on_hover_program_category), (gpointer)categories[i]);
        g_signal_connect(row, "clicked", G_CALLBACK(on_hover_program_category), (gpointer)categories[i]);
        gtk_box_pack_start(GTK_BOX(g_tb.flyout_l1_box), row, FALSE, FALSE, 0);
        if (i == 0) {
            gtk_box_pack_start(GTK_BOX(g_tb.flyout_l1_box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 2);
        }
    }

    gtk_box_pack_start(GTK_BOX(g_tb.flyout_l1_box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 2);

    char dos_cmd[512];
    snprintf(dos_cmd, sizeof(dos_cmd), "kitty -c \"%s/.config/chicago-wayland/kitty/kitty.conf\" --title \"MS-DOS Prompt\"", g_get_home_dir());
    GtkWidget *dos_row = make_flyout_row("MS-DOS Prompt", "utilities-terminal", dos_cmd, FALSE);
    g_signal_connect(dos_row, "enter-notify-event", G_CALLBACK(on_hover_close_l2), NULL);
    gtk_box_pack_start(GTK_BOX(g_tb.flyout_l1_box), dos_row, FALSE, FALSE, 0);

    GtkWidget *exp_row = make_flyout_row("Windows Explorer", "system-file-manager", "thunar", FALSE);
    g_signal_connect(exp_row, "enter-notify-event", G_CALLBACK(on_hover_close_l2), NULL);
    gtk_box_pack_start(GTK_BOX(g_tb.flyout_l1_box), exp_row, FALSE, FALSE, 0);

    if (gtk_layer_is_supported() && g_tb.active_popup_monitor) {
        gtk_layer_set_monitor(GTK_WINDOW(g_tb.flyout_l1_win), g_tb.active_popup_monitor);
        g_object_set_data(G_OBJECT(g_tb.flyout_l1_win), "chicago-monitor", g_tb.active_popup_monitor);
        gtk_layer_set_margin(GTK_WINDOW(g_tb.flyout_l1_win), GTK_LAYER_SHELL_EDGE_LEFT, 214);
        gtk_layer_set_margin(GTK_WINDOW(g_tb.flyout_l1_win), GTK_LAYER_SHELL_EDGE_BOTTOM, 140);
    }
    gtk_widget_show_all(g_tb.flyout_l1_win);
    return FALSE;
}

static void on_start_search_changed(GtkEditable *editable, gpointer data) {
    (void)data;
    const char *q = gtk_entry_get_text(GTK_ENTRY(editable));
    if (!g_tb.flyout_l1_win || !g_tb.flyout_l1_box) return;
    if (g_tb.flyout_l2_win) gtk_widget_hide(g_tb.flyout_l2_win);

    if (!q || !*q) {
        gtk_widget_hide(g_tb.flyout_l1_win);
        return;
    }

    clear_container(g_tb.flyout_l1_box);
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scroll, 230, 260);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
    char *ql = g_ascii_strdown(q, -1);
    int matches = 0;

    for (GList *l = g_tb.app_entries; l != NULL; l = l->next) {
        DesktopAppEntry *e = (DesktopAppEntry *)l->data;
        char *nl = g_ascii_strdown(e->name ? e->name : "", -1);
        char *el = g_ascii_strdown(e->exec ? e->exec : "", -1);
        if (strstr(nl, ql) != NULL || strstr(el, ql) != NULL) {
            GtkWidget *row = make_flyout_row_ex(e->name, e->icon, e->exec, e->desktop_path, FALSE);
            gtk_box_pack_start(GTK_BOX(vbox), row, FALSE, FALSE, 0);
            matches++;
        }
        g_free(nl);
        g_free(el);
    }
    g_free(ql);

    if (matches == 0) {
        GtkWidget *none = gtk_label_new("No matching programs");
        gtk_box_pack_start(GTK_BOX(vbox), none, FALSE, FALSE, 6);
    }

    gtk_container_add(GTK_CONTAINER(scroll), vbox);
    gtk_box_pack_start(GTK_BOX(g_tb.flyout_l1_box), scroll, TRUE, TRUE, 0);

    if (gtk_layer_is_supported() && g_tb.active_popup_monitor) {
        gtk_layer_set_monitor(GTK_WINDOW(g_tb.flyout_l1_win), g_tb.active_popup_monitor);
        g_object_set_data(G_OBJECT(g_tb.flyout_l1_win), "chicago-monitor", g_tb.active_popup_monitor);
        gtk_layer_set_margin(GTK_WINDOW(g_tb.flyout_l1_win), GTK_LAYER_SHELL_EDGE_LEFT, 214);
        gtk_layer_set_margin(GTK_WINDOW(g_tb.flyout_l1_win), GTK_LAYER_SHELL_EDGE_BOTTOM, 140);
    }
    gtk_widget_show_all(g_tb.flyout_l1_win);
}

static void on_start_search_activate(GtkEntry *entry, gpointer data) {
    (void)data;
    const char *q = gtk_entry_get_text(entry);
    if (!q || !*q) return;
    char *ql = g_ascii_strdown(q, -1);
    for (GList *l = g_tb.app_entries; l != NULL; l = l->next) {
        DesktopAppEntry *e = (DesktopAppEntry *)l->data;
        char *nl = g_ascii_strdown(e->name ? e->name : "", -1);
        if (strstr(nl, ql) != NULL) {
            g_free(nl);
            g_free(ql);
            hide_all_popups();
            chicago_spawn_command(e->exec);
            return;
        }
        g_free(nl);
    }
    g_free(ql);
}

/* Hovering "Documents ▸" in the main Start Menu cascades open Flyout Level 1 */
static gboolean on_hover_start_documents(GtkWidget *widget, GdkEventCrossing *event, gpointer data) {
    (void)widget; (void)event; (void)data;
    if (chicago_is_context_menu_visible()) return FALSE;
    if (!g_tb.flyout_l1_win || !g_tb.flyout_l1_box) return FALSE;
    if (g_tb.flyout_l2_win) gtk_widget_hide(g_tb.flyout_l2_win);

    clear_container(g_tb.flyout_l1_box);

    const char *home = g_get_home_dir();
    struct { const char *label; const char *sub; const char *icon; } docs[] = {
        { "My Documents", "Documents", "folder-documents" },
        { "Downloads",    "Downloads", "folder-download" },
        { "Pictures",     "Pictures",  "folder-pictures" },
        { "Desktop",      "Desktop",   "user-desktop" },
        { "Home Folder",  "",          "user-home" },
        { NULL, NULL, NULL }
    };

    for (int i = 0; docs[i].label != NULL; i++) {
        char cmd[512];
        if (docs[i].sub[0] != '\0')
            snprintf(cmd, sizeof(cmd), "thunar \"%s/%s\"", home, docs[i].sub);
        else
            snprintf(cmd, sizeof(cmd), "thunar \"%s\"", home);
        GtkWidget *row = make_flyout_row(docs[i].label, docs[i].icon, cmd, FALSE);
        gtk_box_pack_start(GTK_BOX(g_tb.flyout_l1_box), row, FALSE, FALSE, 0);
    }

    if (gtk_layer_is_supported() && g_tb.active_popup_monitor) {
        gtk_layer_set_monitor(GTK_WINDOW(g_tb.flyout_l1_win), g_tb.active_popup_monitor);
        g_object_set_data(G_OBJECT(g_tb.flyout_l1_win), "chicago-monitor", g_tb.active_popup_monitor);
        gtk_layer_set_margin(GTK_WINDOW(g_tb.flyout_l1_win), GTK_LAYER_SHELL_EDGE_LEFT, 206);
        gtk_layer_set_margin(GTK_WINDOW(g_tb.flyout_l1_win), GTK_LAYER_SHELL_EDGE_BOTTOM, 210);
    }
    gtk_widget_show_all(g_tb.flyout_l1_win);
    return FALSE;
}

/* Hovering "Settings ▸" in the main Start Menu cascades open Flyout Level 1 */
static gboolean on_hover_start_settings(GtkWidget *widget, GdkEventCrossing *event, gpointer data) {
    (void)widget; (void)event; (void)data;
    if (chicago_is_context_menu_visible()) return FALSE;
    if (!g_tb.flyout_l1_win || !g_tb.flyout_l1_box) return FALSE;
    if (g_tb.flyout_l2_win) gtk_widget_hide(g_tb.flyout_l2_win);

    clear_container(g_tb.flyout_l1_box);

    struct { const char *label; const char *icon; const char *cmd; } items[] = {
        { "Control Panel",          "preferences-system",          "chicago-control-panel" },
        { "Display Properties...",  "preferences-desktop-display", "chicago-control-panel --display" },
        { "Sounds and Audio...",    "preferences-desktop-sound",   "chicago-control-panel --audio" },
        { "Network & Wi-Fi...",     "preferences-system-network",  "chicago-control-panel --network" },
        { "Power & Brightness...",  "preferences-system-power",    "chicago-control-panel --power" },
        { "Mouse & Touchpad...",    "input-mouse",                 "chicago-control-panel --mouse" },
        { "Keyboard...",            "input-keyboard",              "chicago-control-panel --keyboard" },
        { "System Properties...",   "computer",                    "chicago-control-panel --system" },
        { "Taskbar & Start Menu...", "preferences-desktop",        "chicago-control-panel --taskbar" },
        { NULL, NULL, NULL }
    };

    for (int i = 0; items[i].label != NULL; i++) {
        GtkWidget *row = make_flyout_row(items[i].label, items[i].icon, items[i].cmd, FALSE);
        gtk_box_pack_start(GTK_BOX(g_tb.flyout_l1_box), row, FALSE, FALSE, 0);
        if (i == 0) {
            gtk_box_pack_start(GTK_BOX(g_tb.flyout_l1_box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 2);
        }
    }

    if (gtk_layer_is_supported() && g_tb.active_popup_monitor) {
        gtk_layer_set_monitor(GTK_WINDOW(g_tb.flyout_l1_win), g_tb.active_popup_monitor);
        g_object_set_data(G_OBJECT(g_tb.flyout_l1_win), "chicago-monitor", g_tb.active_popup_monitor);
        gtk_layer_set_margin(GTK_WINDOW(g_tb.flyout_l1_win), GTK_LAYER_SHELL_EDGE_LEFT, 206);
        gtk_layer_set_margin(GTK_WINDOW(g_tb.flyout_l1_win), GTK_LAYER_SHELL_EDGE_BOTTOM, 140);
    }
    gtk_widget_show_all(g_tb.flyout_l1_win);
    return FALSE;
}

static gboolean on_hover_close_flyouts(GtkWidget *widget, GdkEventCrossing *event, gpointer data) {
    (void)widget; (void)event; (void)data;
    if (chicago_is_context_menu_visible()) return FALSE;
    if (g_tb.flyout_l1_win) gtk_widget_hide(g_tb.flyout_l1_win);
    if (g_tb.flyout_l2_win) gtk_widget_hide(g_tb.flyout_l2_win);
    return FALSE;
}

static gboolean on_draw_start_banner(GtkWidget *widget, cairo_t *cr, gpointer data) {
    (void)data;
    int w = gtk_widget_get_allocated_width(widget);
    int h = gtk_widget_get_allocated_height(widget);

    /* Classic Windows 95 #808080 dark gray vertical strip */
    cairo_set_source_rgb(cr, 0.502, 0.502, 0.502);
    cairo_rectangle(cr, 0, 0, w, h);
    cairo_fill(cr);

    if (g_tb.banner_pb) {
        int pw = gdk_pixbuf_get_width(g_tb.banner_pb);
        int ph = gdk_pixbuf_get_height(g_tb.banner_pb);
        int bx = (w - pw) / 2;
        int by = h - ph - 4;
        gdk_cairo_set_source_pixbuf(cr, g_tb.banner_pb, bx, by);
        cairo_paint(cr);
    } else {
        cairo_save(cr);
        cairo_translate(cr, 4, h - 10);
        cairo_rotate(cr, -G_PI / 2.0);
        PangoLayout *layout = gtk_widget_create_pango_layout(widget, NULL);
        pango_layout_set_markup(layout,
            "<span font_desc='Helvetica Bold 13' foreground='#ffffff'>Windows</span>"
            "<span font_desc='Helvetica 13' foreground='#c0c0c0'> 95</span>", -1);
        cairo_move_to(cr, 0, 0);
        pango_cairo_show_layout(cr, layout);
        g_object_unref(layout);
        cairo_restore(cr);
    }
    return FALSE;
}

static void build_start_menu_and_flyouts(void) {
    char banner_path[512];
    snprintf(banner_path, sizeof(banner_path),
             "%s/.local/share/themes/Chicago95/gtk-3.0/assets/branding_W95.png", g_get_home_dir());
    if (g_file_test(banner_path, G_FILE_TEST_EXISTS)) {
        g_tb.banner_pb = gdk_pixbuf_new_from_file(banner_path, NULL);
    }

    /* 1. Main Start Menu Surface */
    g_tb.start_menu_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(g_tb.start_menu_win), "Start Menu");
    gtk_window_set_decorated(GTK_WINDOW(g_tb.start_menu_win), FALSE);
    gtk_style_context_add_class(gtk_widget_get_style_context(g_tb.start_menu_win), "win95-outset");

    if (gtk_layer_is_supported()) {
        gtk_layer_init_for_window(GTK_WINDOW(g_tb.start_menu_win));
        gtk_layer_set_layer(GTK_WINDOW(g_tb.start_menu_win), GTK_LAYER_SHELL_LAYER_OVERLAY);
        gtk_layer_set_anchor(GTK_WINDOW(g_tb.start_menu_win), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
        gtk_layer_set_anchor(GTK_WINDOW(g_tb.start_menu_win), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
        gtk_layer_set_margin(GTK_WINDOW(g_tb.start_menu_win), GTK_LAYER_SHELL_EDGE_BOTTOM, 28);
        gtk_layer_set_margin(GTK_WINDOW(g_tb.start_menu_win), GTK_LAYER_SHELL_EDGE_LEFT, 2);
        gtk_layer_set_keyboard_mode(GTK_WINDOW(g_tb.start_menu_win), GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);
        gtk_layer_set_namespace(GTK_WINDOW(g_tb.start_menu_win), "chicago-start-menu");
    }

    GtkWidget *outer_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_set_border_width(GTK_CONTAINER(outer_hbox), 3);

    GtkWidget *banner = gtk_drawing_area_new();
    gtk_widget_set_size_request(banner, 25, 310);
    g_signal_connect(banner, "draw", G_CALLBACK(on_draw_start_banner), NULL);
    gtk_box_pack_start(GTK_BOX(outer_hbox), banner, FALSE, FALSE, 0);

    GtkWidget *items_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
    gtk_widget_set_size_request(items_vbox, 188, -1);

    /* Quick Search Box inside Start Menu */
    GtkWidget *search_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_container_set_border_width(GTK_CONTAINER(search_box), 2);
    g_tb.search_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(g_tb.search_entry), "Search programs...");
    g_signal_connect(g_tb.search_entry, "changed", G_CALLBACK(on_start_search_changed), NULL);
    g_signal_connect(g_tb.search_entry, "activate", G_CALLBACK(on_start_search_activate), NULL);
    gtk_box_pack_start(GTK_BOX(search_box), g_tb.search_entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(items_vbox), search_box, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(items_vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 2);

    GtkWidget *prog_btn = make_flyout_row("Programs", "folder", NULL, TRUE);
    g_signal_connect(prog_btn, "enter-notify-event", G_CALLBACK(on_hover_start_programs), NULL);
    g_signal_connect(prog_btn, "clicked", G_CALLBACK(on_hover_start_programs), NULL);
    gtk_box_pack_start(GTK_BOX(items_vbox), prog_btn, FALSE, FALSE, 0);

    GtkWidget *docs_btn = make_flyout_row("Documents", "folder-documents", NULL, TRUE);
    g_signal_connect(docs_btn, "enter-notify-event", G_CALLBACK(on_hover_start_documents), NULL);
    g_signal_connect(docs_btn, "clicked", G_CALLBACK(on_hover_start_documents), NULL);
    gtk_box_pack_start(GTK_BOX(items_vbox), docs_btn, FALSE, FALSE, 0);

    GtkWidget *set_btn = make_flyout_row("Settings", "preferences-system", NULL, TRUE);
    g_signal_connect(set_btn, "enter-notify-event", G_CALLBACK(on_hover_start_settings), NULL);
    g_signal_connect(set_btn, "clicked", G_CALLBACK(on_hover_start_settings), NULL);
    gtk_box_pack_start(GTK_BOX(items_vbox), set_btn, FALSE, FALSE, 0);

    GtkWidget *find_btn = make_flyout_row("Find...", "system-search", "chicago-control-panel --find", FALSE);
    g_signal_connect(find_btn, "enter-notify-event", G_CALLBACK(on_hover_close_flyouts), NULL);
    gtk_box_pack_start(GTK_BOX(items_vbox), find_btn, FALSE, FALSE, 0);

    GtkWidget *help_btn = make_flyout_row("Help", "help-browser", "chicago-control-panel --help-viewer", FALSE);
    g_signal_connect(help_btn, "enter-notify-event", G_CALLBACK(on_hover_close_flyouts), NULL);
    gtk_box_pack_start(GTK_BOX(items_vbox), help_btn, FALSE, FALSE, 0);

    GtkWidget *run_btn = make_flyout_row("Run...", "system-run", "chicago-control-panel --run", FALSE);
    g_signal_connect(run_btn, "enter-notify-event", G_CALLBACK(on_hover_close_flyouts), NULL);
    gtk_box_pack_start(GTK_BOX(items_vbox), run_btn, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(items_vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 2);

    char dos_cmd[512];
    snprintf(dos_cmd, sizeof(dos_cmd), "kitty -c \"%s/.config/chicago-wayland/kitty/kitty.conf\" --title \"MS-DOS Prompt\"", g_get_home_dir());
    GtkWidget *dos_btn = make_flyout_row("MS-DOS Prompt", "utilities-terminal", dos_cmd, FALSE);
    g_signal_connect(dos_btn, "enter-notify-event", G_CALLBACK(on_hover_close_flyouts), NULL);
    gtk_box_pack_start(GTK_BOX(items_vbox), dos_btn, FALSE, FALSE, 0);

    GtkWidget *exp_btn = make_flyout_row("Windows Explorer", "system-file-manager", "thunar", FALSE);
    g_signal_connect(exp_btn, "enter-notify-event", G_CALLBACK(on_hover_close_flyouts), NULL);
    gtk_box_pack_start(GTK_BOX(items_vbox), exp_btn, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(items_vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 2);

    GtkWidget *lock_btn = make_flyout_row("Lock Workstation", "system-lock-screen", "chicago-lock", FALSE);
    g_signal_connect(lock_btn, "enter-notify-event", G_CALLBACK(on_hover_close_flyouts), NULL);
    gtk_box_pack_start(GTK_BOX(items_vbox), lock_btn, FALSE, FALSE, 0);

    GtkWidget *shut_btn = make_flyout_row("Shut Down...", "system-shutdown", "chicago-control-panel --shutdown", FALSE);
    g_signal_connect(shut_btn, "enter-notify-event", G_CALLBACK(on_hover_close_flyouts), NULL);
    gtk_box_pack_start(GTK_BOX(items_vbox), shut_btn, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(outer_hbox), items_vbox, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(g_tb.start_menu_win), outer_hbox);

    /* 2. Flyout Level 1 Surface */
    g_tb.flyout_l1_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_decorated(GTK_WINDOW(g_tb.flyout_l1_win), FALSE);
    gtk_style_context_add_class(gtk_widget_get_style_context(g_tb.flyout_l1_win), "win95-outset");
    if (gtk_layer_is_supported()) {
        gtk_layer_init_for_window(GTK_WINDOW(g_tb.flyout_l1_win));
        gtk_layer_set_layer(GTK_WINDOW(g_tb.flyout_l1_win), GTK_LAYER_SHELL_LAYER_OVERLAY);
        gtk_layer_set_anchor(GTK_WINDOW(g_tb.flyout_l1_win), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
        gtk_layer_set_anchor(GTK_WINDOW(g_tb.flyout_l1_win), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
        gtk_layer_set_namespace(GTK_WINDOW(g_tb.flyout_l1_win), "chicago-start-flyout1");
    }
    g_tb.flyout_l1_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
    gtk_container_set_border_width(GTK_CONTAINER(g_tb.flyout_l1_box), 3);
    gtk_widget_set_size_request(g_tb.flyout_l1_box, 180, -1);
    gtk_container_add(GTK_CONTAINER(g_tb.flyout_l1_win), g_tb.flyout_l1_box);

    /* 3. Flyout Level 2 Surface */
    g_tb.flyout_l2_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_decorated(GTK_WINDOW(g_tb.flyout_l2_win), FALSE);
    gtk_style_context_add_class(gtk_widget_get_style_context(g_tb.flyout_l2_win), "win95-outset");
    if (gtk_layer_is_supported()) {
        gtk_layer_init_for_window(GTK_WINDOW(g_tb.flyout_l2_win));
        gtk_layer_set_layer(GTK_WINDOW(g_tb.flyout_l2_win), GTK_LAYER_SHELL_LAYER_OVERLAY);
        gtk_layer_set_anchor(GTK_WINDOW(g_tb.flyout_l2_win), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
        gtk_layer_set_anchor(GTK_WINDOW(g_tb.flyout_l2_win), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
        gtk_layer_set_namespace(GTK_WINDOW(g_tb.flyout_l2_win), "chicago-start-flyout2");
    }
    g_tb.flyout_l2_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
    gtk_container_set_border_width(GTK_CONTAINER(g_tb.flyout_l2_box), 3);
    gtk_container_add(GTK_CONTAINER(g_tb.flyout_l2_win), g_tb.flyout_l2_box);
}

static void set_start_menu_visible(gboolean visible, GdkMonitor *target_mon) {
    if (!g_tb.start_menu_win) return;
    if (visible) {
        hide_all_popups();
        scan_desktop_applications();
        g_tb.start_menu_visible = TRUE;
        g_tb.active_popup_monitor = target_mon;
        if (g_tb.search_entry) {
            gtk_entry_set_text(GTK_ENTRY(g_tb.search_entry), "");
        }
        if (gtk_layer_is_supported() && target_mon) {
            gtk_layer_set_monitor(GTK_WINDOW(g_tb.start_menu_win), target_mon);
            g_object_set_data(G_OBJECT(g_tb.start_menu_win), "chicago-monitor", target_mon);
        }
        gtk_widget_show_all(g_tb.start_menu_win);
        if (g_tb.search_entry) {
            gtk_widget_grab_focus(g_tb.search_entry);
        }
        for (int i = 0; i < g_tb.bar_count; i++) {
            if (g_tb.bars[i].monitor == target_mon) {
                gtk_style_context_add_class(gtk_widget_get_style_context(g_tb.bars[i].start_button), "win95-btn-active");
            }
        }
    } else {
        g_tb.start_menu_visible = FALSE;
        gtk_widget_hide(g_tb.start_menu_win);
        if (g_tb.flyout_l1_win) gtk_widget_hide(g_tb.flyout_l1_win);
        if (g_tb.flyout_l2_win) gtk_widget_hide(g_tb.flyout_l2_win);
        for (int i = 0; i < g_tb.bar_count; i++) {
            gtk_style_context_remove_class(gtk_widget_get_style_context(g_tb.bars[i].start_button), "win95-btn-active");
        }
    }
}

/* =========================================================================
 * System Tray Interactive Popups (Volume, Network/Wi-Fi, Power/Brightness, Clock)
 * ========================================================================= */

static void hide_all_popups(void) {
    chicago_close_context_menu();
    if (g_tb.start_menu_visible) set_start_menu_visible(FALSE, NULL);
    if (g_tb.vol_popup) gtk_widget_hide(g_tb.vol_popup);
    if (g_tb.net_popup) gtk_widget_hide(g_tb.net_popup);
    if (g_tb.pwr_popup) gtk_widget_hide(g_tb.pwr_popup);
    if (g_tb.cal_popup) gtk_widget_hide(g_tb.cal_popup);
}

static void on_vol_scale_changed(GtkRange *range, gpointer data) {
    (void)data;
    if (g_tb.vol_updating) return;
    int val = (int)gtk_range_get_value(range);
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "wpctl set-volume @DEFAULT_AUDIO_SINK@ %d%%", CLAMP(val, 0, 100));
    chicago_spawn_command(cmd);
}

static void on_vol_mute_toggled(GtkToggleButton *tb, gpointer data) {
    (void)data;
    if (g_tb.vol_updating) return;
    gboolean muted = gtk_toggle_button_get_active(tb);
    chicago_spawn_command(muted ? "wpctl set-mute @DEFAULT_AUDIO_SINK@ 1" : "wpctl set-mute @DEFAULT_AUDIO_SINK@ 0");
}

static void toggle_volume_popup(GdkMonitor *mon) {
    if (!g_tb.vol_popup) {
        g_tb.vol_popup = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_decorated(GTK_WINDOW(g_tb.vol_popup), FALSE);
        gtk_style_context_add_class(gtk_widget_get_style_context(g_tb.vol_popup), "win95-outset");
        if (gtk_layer_is_supported()) {
            gtk_layer_init_for_window(GTK_WINDOW(g_tb.vol_popup));
            gtk_layer_set_layer(GTK_WINDOW(g_tb.vol_popup), GTK_LAYER_SHELL_LAYER_OVERLAY);
            gtk_layer_set_anchor(GTK_WINDOW(g_tb.vol_popup), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
            gtk_layer_set_anchor(GTK_WINDOW(g_tb.vol_popup), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
            gtk_layer_set_margin(GTK_WINDOW(g_tb.vol_popup), GTK_LAYER_SHELL_EDGE_BOTTOM, 28);
            gtk_layer_set_margin(GTK_WINDOW(g_tb.vol_popup), GTK_LAYER_SHELL_EDGE_RIGHT, 40);
            gtk_layer_set_namespace(GTK_WINDOW(g_tb.vol_popup), "chicago-vol-popup");
        }

        GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
        gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);

        GtkWidget *title = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(title), "<b>Volume</b>");
        gtk_box_pack_start(GTK_BOX(vbox), title, FALSE, FALSE, 0);

        g_tb.vol_scale = gtk_scale_new_with_range(GTK_ORIENTATION_VERTICAL, 0, 100, 2);
        gtk_range_set_inverted(GTK_RANGE(g_tb.vol_scale), TRUE);
        gtk_scale_set_value_pos(GTK_SCALE(g_tb.vol_scale), GTK_POS_TOP);
        gtk_widget_set_size_request(g_tb.vol_scale, 44, 130);
        g_signal_connect(g_tb.vol_scale, "value-changed", G_CALLBACK(on_vol_scale_changed), NULL);
        gtk_box_pack_start(GTK_BOX(vbox), g_tb.vol_scale, TRUE, TRUE, 0);

        g_tb.vol_mute_chk = gtk_check_button_new_with_label("Mute");
        g_signal_connect(g_tb.vol_mute_chk, "toggled", G_CALLBACK(on_vol_mute_toggled), NULL);
        gtk_box_pack_start(GTK_BOX(vbox), g_tb.vol_mute_chk, FALSE, FALSE, 0);

        GtkWidget *prop_btn = gtk_button_new_with_label("Audio Properties...");
        g_signal_connect_data(prop_btn, "clicked", G_CALLBACK(on_launch_cmd_btn),
                              g_strdup("chicago-control-panel --audio"), free_closure_str, 0);
        gtk_box_pack_start(GTK_BOX(vbox), prop_btn, FALSE, FALSE, 0);

        gtk_container_add(GTK_CONTAINER(g_tb.vol_popup), vbox);
    }

    if (gtk_widget_get_visible(g_tb.vol_popup)) {
        gtk_widget_hide(g_tb.vol_popup);
    } else {
        hide_all_popups();
        g_tb.vol_updating = TRUE;
        gboolean muted = FALSE;
        int vol = get_current_volume(&muted);
        gtk_range_set_value(GTK_RANGE(g_tb.vol_scale), vol);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_tb.vol_mute_chk), muted);
        g_tb.vol_updating = FALSE;
        if (gtk_layer_is_supported() && mon) {
            gtk_layer_set_monitor(GTK_WINDOW(g_tb.vol_popup), mon);
        }
        gtk_widget_show_all(g_tb.vol_popup);
    }
}

static void on_wifi_connect_clicked(GtkButton *btn, gpointer data) {
    (void)btn;
    const char *ssid = (const char *)data;
    if (!ssid || !*ssid) return;
    hide_all_popups();
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "nmcli device wifi connect \"%s\" || chicago-control-panel --network", ssid);
    chicago_spawn_command(cmd);
}

static void refresh_network_popup_contents(void) {
    if (!g_tb.net_status_lbl || !g_tb.net_wifi_list_box) return;

    char *out = NULL;
    char status_buf[512] = "Status: Disconnected";
    if (g_spawn_command_line_sync("nmcli -t -f NAME,TYPE,DEVICE connection show --active", &out, NULL, NULL, NULL) && out) {
        char **lines = g_strsplit(out, "\n", -1);
        for (int i = 0; lines[i]; i++) {
            if (lines[i][0] && !g_str_has_prefix(lines[i], "lo:")) {
                snprintf(status_buf, sizeof(status_buf), "Active: %s", lines[i]);
                break;
            }
        }
        g_strfreev(lines);
        g_free(out);
    }
    gtk_label_set_text(GTK_LABEL(g_tb.net_status_lbl), status_buf);

    clear_container(g_tb.net_wifi_list_box);
    char *wifi_out = NULL;
    if (g_spawn_command_line_sync("nmcli -t -f IN-USE,SSID,SIGNAL,SECURITY dev wifi list --rescan no", &wifi_out, NULL, NULL, NULL) && wifi_out) {
        char **lines = g_strsplit(wifi_out, "\n", -1);
        int shown = 0;
        for (int i = 0; lines[i] && shown < 8; i++) {
            char **parts = g_strsplit(lines[i], ":", 4);
            if (parts[0] && parts[1] && parts[1][0] != '\0') {
                char row_lbl[256];
                snprintf(row_lbl, sizeof(row_lbl), "%s %s (%s%%)",
                         (parts[0][0] == '*') ? "✓" : "  ",
                         parts[1],
                         parts[2] ? parts[2] : "?");
                GtkWidget *btn = gtk_button_new_with_label(row_lbl);
                gtk_style_context_add_class(gtk_widget_get_style_context(btn), "win95-menuitem");
                g_signal_connect_data(btn, "clicked", G_CALLBACK(on_wifi_connect_clicked),
                                      g_strdup(parts[1]), free_closure_str, 0);
                gtk_box_pack_start(GTK_BOX(g_tb.net_wifi_list_box), btn, FALSE, FALSE, 0);
                shown++;
            }
            g_strfreev(parts);
        }
        g_strfreev(lines);
        g_free(wifi_out);
    }
    gtk_widget_show_all(g_tb.net_wifi_list_box);
}

static void toggle_network_popup(GdkMonitor *mon) {
    if (!g_tb.net_popup) {
        g_tb.net_popup = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_decorated(GTK_WINDOW(g_tb.net_popup), FALSE);
        gtk_style_context_add_class(gtk_widget_get_style_context(g_tb.net_popup), "win95-outset");
        if (gtk_layer_is_supported()) {
            gtk_layer_init_for_window(GTK_WINDOW(g_tb.net_popup));
            gtk_layer_set_layer(GTK_WINDOW(g_tb.net_popup), GTK_LAYER_SHELL_LAYER_OVERLAY);
            gtk_layer_set_anchor(GTK_WINDOW(g_tb.net_popup), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
            gtk_layer_set_anchor(GTK_WINDOW(g_tb.net_popup), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
            gtk_layer_set_margin(GTK_WINDOW(g_tb.net_popup), GTK_LAYER_SHELL_EDGE_BOTTOM, 28);
            gtk_layer_set_margin(GTK_WINDOW(g_tb.net_popup), GTK_LAYER_SHELL_EDGE_RIGHT, 20);
            gtk_layer_set_namespace(GTK_WINDOW(g_tb.net_popup), "chicago-net-popup");
        }

        GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
        gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);
        gtk_widget_set_size_request(vbox, 240, -1);

        GtkWidget *hdr = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(hdr), "<b>Network &amp; Dial-Up Connections</b>");
        gtk_label_set_xalign(GTK_LABEL(hdr), 0.0f);
        gtk_box_pack_start(GTK_BOX(vbox), hdr, FALSE, FALSE, 0);

        g_tb.net_status_lbl = gtk_label_new("Checking network...");
        gtk_label_set_xalign(GTK_LABEL(g_tb.net_status_lbl), 0.0f);
        gtk_box_pack_start(GTK_BOX(vbox), g_tb.net_status_lbl, FALSE, FALSE, 0);

        GtkWidget *well = gtk_frame_new("Available Wireless Networks");
        g_tb.net_wifi_list_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
        gtk_container_set_border_width(GTK_CONTAINER(g_tb.net_wifi_list_box), 4);
        gtk_container_add(GTK_CONTAINER(well), g_tb.net_wifi_list_box);
        gtk_box_pack_start(GTK_BOX(vbox), well, TRUE, TRUE, 0);

        GtkWidget *prop_btn = gtk_button_new_with_label("Network Properties...");
        g_signal_connect_data(prop_btn, "clicked", G_CALLBACK(on_launch_cmd_btn),
                              g_strdup("chicago-control-panel --network"), free_closure_str, 0);
        gtk_box_pack_start(GTK_BOX(vbox), prop_btn, FALSE, FALSE, 0);

        gtk_container_add(GTK_CONTAINER(g_tb.net_popup), vbox);
    }

    if (gtk_widget_get_visible(g_tb.net_popup)) {
        gtk_widget_hide(g_tb.net_popup);
    } else {
        hide_all_popups();
        refresh_network_popup_contents();
        if (gtk_layer_is_supported() && mon) {
            gtk_layer_set_monitor(GTK_WINDOW(g_tb.net_popup), mon);
        }
        gtk_widget_show_all(g_tb.net_popup);
    }
}

static void on_pwr_brightness_changed(GtkRange *range, gpointer data) {
    (void)data;
    if (g_tb.pwr_updating) return;
    int pct = CLAMP((int)gtk_range_get_value(range), 5, 100);
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "brightnessctl set %d%%", pct);
    chicago_spawn_command(cmd);
}

static void on_pwr_profile_clicked(GtkButton *btn, gpointer data) {
    (void)btn;
    const char *prof = (const char *)data;
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "powerprofilesctl set %s", prof);
    chicago_spawn_command(cmd);
}

static void toggle_power_popup(GdkMonitor *mon) {
    if (!g_tb.pwr_popup) {
        g_tb.pwr_popup = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_decorated(GTK_WINDOW(g_tb.pwr_popup), FALSE);
        gtk_style_context_add_class(gtk_widget_get_style_context(g_tb.pwr_popup), "win95-outset");
        if (gtk_layer_is_supported()) {
            gtk_layer_init_for_window(GTK_WINDOW(g_tb.pwr_popup));
            gtk_layer_set_layer(GTK_WINDOW(g_tb.pwr_popup), GTK_LAYER_SHELL_LAYER_OVERLAY);
            gtk_layer_set_anchor(GTK_WINDOW(g_tb.pwr_popup), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
            gtk_layer_set_anchor(GTK_WINDOW(g_tb.pwr_popup), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
            gtk_layer_set_margin(GTK_WINDOW(g_tb.pwr_popup), GTK_LAYER_SHELL_EDGE_BOTTOM, 28);
            gtk_layer_set_margin(GTK_WINDOW(g_tb.pwr_popup), GTK_LAYER_SHELL_EDGE_RIGHT, 12);
            gtk_layer_set_namespace(GTK_WINDOW(g_tb.pwr_popup), "chicago-pwr-popup");
        }

        GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
        gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);
        gtk_widget_set_size_request(vbox, 230, -1);

        GtkWidget *hdr = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(hdr), "<b>Power Meter &amp; Display Brightness</b>");
        gtk_label_set_xalign(GTK_LABEL(hdr), 0.0f);
        gtk_box_pack_start(GTK_BOX(vbox), hdr, FALSE, FALSE, 0);

        g_tb.pwr_status_lbl = gtk_label_new("Battery: Checking...");
        gtk_label_set_xalign(GTK_LABEL(g_tb.pwr_status_lbl), 0.0f);
        gtk_box_pack_start(GTK_BOX(vbox), g_tb.pwr_status_lbl, FALSE, FALSE, 0);

        GtkWidget *br_lbl = gtk_label_new("Screen Brightness (%):");
        gtk_label_set_xalign(GTK_LABEL(br_lbl), 0.0f);
        gtk_box_pack_start(GTK_BOX(vbox), br_lbl, FALSE, FALSE, 0);

        g_tb.pwr_bright_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 5, 100, 5);
        g_signal_connect(g_tb.pwr_bright_scale, "value-changed", G_CALLBACK(on_pwr_brightness_changed), NULL);
        gtk_box_pack_start(GTK_BOX(vbox), g_tb.pwr_bright_scale, FALSE, FALSE, 0);

        GtkWidget *prof_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
        GtkWidget *b1 = gtk_button_new_with_label("Saver");
        GtkWidget *b2 = gtk_button_new_with_label("Balanced");
        GtkWidget *b3 = gtk_button_new_with_label("Perf");
        g_signal_connect(b1, "clicked", G_CALLBACK(on_pwr_profile_clicked), (gpointer)"power-saver");
        g_signal_connect(b2, "clicked", G_CALLBACK(on_pwr_profile_clicked), (gpointer)"balanced");
        g_signal_connect(b3, "clicked", G_CALLBACK(on_pwr_profile_clicked), (gpointer)"performance");
        gtk_box_pack_start(GTK_BOX(prof_box), b1, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(prof_box), b2, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(prof_box), b3, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(vbox), prof_box, FALSE, FALSE, 0);

        GtkWidget *prop_btn = gtk_button_new_with_label("Power Properties...");
        g_signal_connect_data(prop_btn, "clicked", G_CALLBACK(on_launch_cmd_btn),
                              g_strdup("chicago-control-panel --power"), free_closure_str, 0);
        gtk_box_pack_start(GTK_BOX(vbox), prop_btn, FALSE, FALSE, 0);

        gtk_container_add(GTK_CONTAINER(g_tb.pwr_popup), vbox);
    }

    if (gtk_widget_get_visible(g_tb.pwr_popup)) {
        gtk_widget_hide(g_tb.pwr_popup);
    } else {
        hide_all_popups();
        g_tb.pwr_updating = TRUE;
        char *cap = NULL, *st = NULL;
        g_file_get_contents("/sys/class/power_supply/BAT0/capacity", &cap, NULL, NULL);
        g_file_get_contents("/sys/class/power_supply/BAT0/status", &st, NULL, NULL);
        char msg[256];
        snprintf(msg, sizeof(msg), "Power Status: %s%% (%s)",
                 cap ? g_strstrip(cap) : "AC", st ? g_strstrip(st) : "Online");
        gtk_label_set_text(GTK_LABEL(g_tb.pwr_status_lbl), msg);
        g_free(cap);
        g_free(st);
        gtk_range_set_value(GTK_RANGE(g_tb.pwr_bright_scale), get_current_brightness_pct());
        g_tb.pwr_updating = FALSE;

        if (gtk_layer_is_supported() && mon) {
            gtk_layer_set_monitor(GTK_WINDOW(g_tb.pwr_popup), mon);
        }
        gtk_widget_show_all(g_tb.pwr_popup);
    }
}

static void toggle_calendar_popup(GdkMonitor *mon) {
    if (!g_tb.cal_popup) {
        g_tb.cal_popup = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_decorated(GTK_WINDOW(g_tb.cal_popup), FALSE);
        gtk_style_context_add_class(gtk_widget_get_style_context(g_tb.cal_popup), "win95-outset");
        if (gtk_layer_is_supported()) {
            gtk_layer_init_for_window(GTK_WINDOW(g_tb.cal_popup));
            gtk_layer_set_layer(GTK_WINDOW(g_tb.cal_popup), GTK_LAYER_SHELL_LAYER_OVERLAY);
            gtk_layer_set_anchor(GTK_WINDOW(g_tb.cal_popup), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
            gtk_layer_set_anchor(GTK_WINDOW(g_tb.cal_popup), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
            gtk_layer_set_margin(GTK_WINDOW(g_tb.cal_popup), GTK_LAYER_SHELL_EDGE_BOTTOM, 28);
            gtk_layer_set_margin(GTK_WINDOW(g_tb.cal_popup), GTK_LAYER_SHELL_EDGE_RIGHT, 2);
            gtk_layer_set_namespace(GTK_WINDOW(g_tb.cal_popup), "chicago-cal-popup");
        }

        GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
        gtk_container_set_border_width(GTK_CONTAINER(vbox), 6);
        GtkWidget *cal = gtk_calendar_new();
        gtk_box_pack_start(GTK_BOX(vbox), cal, TRUE, TRUE, 0);

        GtkWidget *prop_btn = gtk_button_new_with_label("Date/Time Properties...");
        g_signal_connect_data(prop_btn, "clicked", G_CALLBACK(on_launch_cmd_btn),
                              g_strdup("chicago-control-panel --datetime"), free_closure_str, 0);
        gtk_box_pack_start(GTK_BOX(vbox), prop_btn, FALSE, FALSE, 0);

        gtk_container_add(GTK_CONTAINER(g_tb.cal_popup), vbox);
    }

    if (gtk_widget_get_visible(g_tb.cal_popup)) {
        gtk_widget_hide(g_tb.cal_popup);
    } else {
        hide_all_popups();
        if (gtk_layer_is_supported() && mon) {
            gtk_layer_set_monitor(GTK_WINDOW(g_tb.cal_popup), mon);
        }
        gtk_widget_show_all(g_tb.cal_popup);
    }
}

/* =========================================================================
 * Bar Button Callbacks & Minute-Aligned Clock
 * ========================================================================= */

static void on_start_btn_clicked(GtkButton *btn, gpointer data) {
    (void)btn;
    BarInstance *bi = (BarInstance *)data;
    set_start_menu_visible(!g_tb.start_menu_visible, bi ? bi->monitor : NULL);
}

static void on_tray_vol_clicked(GtkButton *btn, gpointer data) {
    (void)btn;
    BarInstance *bi = (BarInstance *)data;
    toggle_volume_popup(bi ? bi->monitor : NULL);
}

static void on_tray_net_clicked(GtkButton *btn, gpointer data) {
    (void)btn;
    BarInstance *bi = (BarInstance *)data;
    toggle_network_popup(bi ? bi->monitor : NULL);
}

static void on_tray_pwr_clicked(GtkButton *btn, gpointer data) {
    (void)btn;
    BarInstance *bi = (BarInstance *)data;
    toggle_power_popup(bi ? bi->monitor : NULL);
}

static void on_tray_clock_clicked(GtkButton *btn, gpointer data) {
    (void)btn;
    BarInstance *bi = (BarInstance *)data;
    toggle_calendar_popup(bi ? bi->monitor : NULL);
}

static void toggle_show_desktop(void) {
    hide_all_popups();
    if (!g_tb.toplevels) return;

    gboolean any_visible = FALSE;
    for (GList *l = g_tb.toplevels; l != NULL; l = l->next) {
        ToplevelItem *item = (ToplevelItem *)l->data;
        if (!item->minimized) {
            any_visible = TRUE;
            break;
        }
    }

    for (GList *l = g_tb.toplevels; l != NULL; l = l->next) {
        ToplevelItem *item = (ToplevelItem *)l->data;
        if (any_visible) {
            zwlr_foreign_toplevel_handle_v1_set_minimized(item->handle);
        } else {
            zwlr_foreign_toplevel_handle_v1_unset_minimized(item->handle);
        }
    }
    if (g_tb.wl_display) wl_display_flush(g_tb.wl_display);
}

static void on_quick_show_desktop(GtkButton *btn, gpointer data) {
    (void)btn; (void)data;
    toggle_show_desktop();
}

static void schedule_next_minute_tick(void);

static void update_clock_labels(void) {
    time_t now = time(NULL);
    struct tm *lt = localtime(&now);
    if (!lt) return;
    char buf[64], tip[128];
    strftime(buf, sizeof(buf), "%I:%M %p", lt);
    strftime(tip, sizeof(tip), "%A, %B %d, %Y", lt);
    const char *text = (buf[0] == '0') ? (buf + 1) : buf;

    for (int i = 0; i < g_tb.bar_count; i++) {
        if (g_tb.bars[i].clock_label) {
            gtk_label_set_text(GTK_LABEL(g_tb.bars[i].clock_label), text);
            gtk_widget_set_tooltip_text(g_tb.bars[i].clock_label, tip);
        }
    }
}

static gboolean on_clock_minute_tick(gpointer data) {
    (void)data;
    update_clock_labels();
    schedule_next_minute_tick();
    return G_SOURCE_REMOVE;
}

static void schedule_next_minute_tick(void) {
    time_t now = time(NULL);
    struct tm *lt = localtime(&now);
    guint wait_sec = lt ? (guint)(60 - lt->tm_sec) : 60;
    if (wait_sec == 0 || wait_sec > 60) wait_sec = 60;
    g_timeout_add_seconds(wait_sec, on_clock_minute_tick, NULL);
}

static gboolean on_sigusr1(gpointer data) {
    (void)data;
    GdkMonitor *mon = (g_tb.bar_count > 0) ? g_tb.bars[0].monitor : NULL;
    set_start_menu_visible(!g_tb.start_menu_visible, mon);
    return G_SOURCE_CONTINUE;
}

static gboolean on_sigusr2(gpointer data) {
    (void)data;
    toggle_show_desktop();
    return G_SOURCE_CONTINUE;
}

static void on_ctx_tb_run_cmd(GtkWidget *btn, gpointer data) {
    (void)btn;
    const char *cmd = (const char *)data;
    chicago_spawn_command(cmd);
}

static void on_ctx_tb_open_start(GtkWidget *btn, gpointer data) {
    (void)btn;
    BarInstance *bi = (BarInstance *)data;
    set_start_menu_visible(TRUE, bi ? bi->monitor : NULL);
}

static void show_start_button_context_menu(BarInstance *bi, GtkWidget *widget, GdkEventButton *event) {
    hide_all_popups();

    GdkMonitor *mon = bi ? bi->monitor : NULL;
    int left = 2, edge = 28;
    gboolean anchor_bottom = TRUE;
    chicago_resolve_event_layer_coords(widget, event, &mon, &left, &edge, &anchor_bottom);

    GtkWidget *vbox = NULL;
    GtkWidget *win = chicago_ctx_menu_begin(mon, 2, 28, TRUE, &vbox);

    chicago_ctx_menu_add_item(vbox, "Open Start Menu", "folder", TRUE,
                              G_CALLBACK(on_ctx_tb_open_start), bi);
    chicago_ctx_menu_add_item(vbox, "Explore (Windows Explorer)", "system-file-manager", FALSE,
                              G_CALLBACK(on_ctx_tb_run_cmd), (gpointer)"thunar");
    chicago_ctx_menu_add_item(vbox, "Find Programs...", "system-search", FALSE,
                              G_CALLBACK(on_ctx_tb_run_cmd), (gpointer)"chicago-control-panel --find");
    chicago_ctx_menu_add_item(vbox, "Run...", "system-run", FALSE,
                              G_CALLBACK(on_ctx_tb_run_cmd), (gpointer)"chicago-control-panel --run");

    chicago_ctx_menu_add_separator(vbox);

    chicago_ctx_menu_add_item(vbox, "Control Panel", "preferences-system", FALSE,
                              G_CALLBACK(on_ctx_tb_run_cmd), (gpointer)"chicago-control-panel");
    chicago_ctx_menu_add_item(vbox, "MS-DOS Prompt", "utilities-terminal", FALSE,
                              G_CALLBACK(on_ctx_tb_run_cmd),
                              (gpointer)"kitty -c ~/.config/chicago-wayland/kitty/kitty.conf --title 'MS-DOS Prompt'");
    chicago_ctx_menu_add_item(vbox, "Task Manager / System Properties", "computer", FALSE,
                              G_CALLBACK(on_ctx_tb_run_cmd), (gpointer)"chicago-control-panel --system");

    chicago_ctx_menu_add_separator(vbox);

    chicago_ctx_menu_add_item(vbox, "Lock Workstation", "system-lock-screen", FALSE,
                              G_CALLBACK(on_ctx_tb_run_cmd), (gpointer)"chicago-lock");
    chicago_ctx_menu_add_item(vbox, "Shut Down...", "system-shutdown", FALSE,
                              G_CALLBACK(on_ctx_tb_run_cmd), (gpointer)"chicago-control-panel --shutdown");

    chicago_ctx_menu_add_separator(vbox);

    chicago_ctx_menu_add_item(vbox, "Properties", "preferences-desktop", FALSE,
                              G_CALLBACK(on_ctx_tb_run_cmd), (gpointer)"chicago-control-panel --taskbar");

    chicago_ctx_menu_show(win, mon, 2, 28, TRUE);
}

static void show_taskbar_context_menu(BarInstance *bi, GtkWidget *widget, GdkEventButton *event) {
    hide_all_popups();

    GdkMonitor *mon = bi ? bi->monitor : NULL;
    int left = event ? (int)event->x : 120;
    int edge = 28;
    gboolean anchor_bottom = TRUE;
    chicago_resolve_event_layer_coords(widget, event, &mon, &left, &edge, &anchor_bottom);

    GtkWidget *vbox = NULL;
    GtkWidget *win = chicago_ctx_menu_begin(mon, left, 28, TRUE, &vbox);

    chicago_ctx_menu_add_item(vbox, "Show Desktop (Minimize / Restore All)", "user-desktop", TRUE,
                              G_CALLBACK(on_quick_show_desktop), NULL);
    chicago_ctx_menu_add_item(vbox, "Pin a Program to Taskbar...", "system-search", FALSE,
                              G_CALLBACK(on_ctx_tb_run_cmd), (gpointer)"chicago-control-panel --find");
    chicago_ctx_menu_add_item(vbox, "Task Manager / System Properties", "computer", FALSE,
                              G_CALLBACK(on_ctx_tb_run_cmd), (gpointer)"chicago-control-panel --system");

    chicago_ctx_menu_add_separator(vbox);

    chicago_ctx_menu_add_item(vbox, "Adjust Date/Time", "preferences-system-time", FALSE,
                              G_CALLBACK(on_ctx_tb_run_cmd), (gpointer)"chicago-control-panel --datetime");
    chicago_ctx_menu_add_item(vbox, "Volume & Audio Properties", "audio-volume-high", FALSE,
                              G_CALLBACK(on_ctx_tb_run_cmd), (gpointer)"chicago-control-panel --audio");
    chicago_ctx_menu_add_item(vbox, "Network & Wi-Fi Properties", "network-workgroup", FALSE,
                              G_CALLBACK(on_ctx_tb_run_cmd), (gpointer)"chicago-control-panel --network");
    chicago_ctx_menu_add_item(vbox, "Display Properties", "preferences-desktop-display", FALSE,
                              G_CALLBACK(on_ctx_tb_run_cmd), (gpointer)"chicago-control-panel --display");

    chicago_ctx_menu_add_separator(vbox);

    chicago_ctx_menu_add_item(vbox, "Lock Workstation", "system-lock-screen", FALSE,
                              G_CALLBACK(on_ctx_tb_run_cmd), (gpointer)"chicago-lock");

    chicago_ctx_menu_add_separator(vbox);

    chicago_ctx_menu_add_item(vbox, "Properties", "preferences-desktop", TRUE,
                              G_CALLBACK(on_ctx_tb_run_cmd), (gpointer)"chicago-control-panel --taskbar");

    chicago_ctx_menu_show(win, mon, left, 28, TRUE);
}

static gboolean on_start_btn_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    BarInstance *bi = (BarInstance *)data;
    if (event->button == 3) {
        show_start_button_context_menu(bi, widget, event);
        return TRUE;
    }
    return FALSE;
}

static gboolean on_bar_window_button_press(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    BarInstance *bi = (BarInstance *)data;
    if (event->button == 3) {
        show_taskbar_context_menu(bi, widget, event);
        return TRUE;
    } else if (event->button == 1) {
        hide_all_popups();
    }
    return FALSE;
}

static void populate_pins_for_bar(BarInstance *bi) {
    if (!bi || !bi->pins_box) return;
    clear_container(bi->pins_box);

    /* Show Desktop button always first in Quick Launch */
    GtkWidget *sd_btn = gtk_button_new();
    gtk_style_context_add_class(gtk_widget_get_style_context(sd_btn), "quick-btn");
    gtk_widget_add_events(sd_btn, GDK_BUTTON_PRESS_MASK);
    gtk_widget_set_tooltip_text(sd_btn, "Show Desktop (Super+D)");
    GdkPixbuf *sdpb = chicago_load_icon("user-desktop", 16);
    gtk_container_add(GTK_CONTAINER(sd_btn), gtk_image_new_from_pixbuf(sdpb));
    if (sdpb) g_object_unref(sdpb);
    g_signal_connect(sd_btn, "clicked", G_CALLBACK(on_quick_show_desktop), NULL);
    g_signal_connect(sd_btn, "button-press-event", G_CALLBACK(on_bar_window_button_press), bi);
    gtk_box_pack_start(GTK_BOX(bi->pins_box), sd_btn, FALSE, FALSE, 0);

    GList *pins = chicago_pins_load();
    for (GList *l = pins; l != NULL; l = l->next) {
        ChicagoPinEntry *p = (ChicagoPinEntry *)l->data;
        GtkWidget *qbtn = gtk_button_new();
        gtk_style_context_add_class(gtk_widget_get_style_context(qbtn), "quick-btn");
        gtk_widget_add_events(qbtn, GDK_BUTTON_PRESS_MASK);
        char tip[256];
        snprintf(tip, sizeof(tip), "%s (Right-click for GPU / Unpin / Properties)", p->name ? p->name : "App");
        gtk_widget_set_tooltip_text(qbtn, tip);

        GdkPixbuf *qpb = chicago_load_icon(p->icon, 16);
        gtk_container_add(GTK_CONTAINER(qbtn), gtk_image_new_from_pixbuf(qpb));
        if (qpb) g_object_unref(qpb);

        g_signal_connect_data(qbtn, "clicked", G_CALLBACK(on_launch_cmd_btn),
                              g_strdup(p->exec), free_closure_str, 0);

        AppRowCtx *rc = g_malloc0(sizeof(AppRowCtx));
        rc->name = g_strdup(p->name ? p->name : "App");
        rc->icon = g_strdup(p->icon ? p->icon : "application-x-executable");
        rc->exec = g_strdup(p->exec ? p->exec : "");
        rc->desktop_path = g_strdup(p->desktop_path ? p->desktop_path : "");
        g_signal_connect_data(qbtn, "button-press-event", G_CALLBACK(on_app_row_button_press),
                              rc, app_row_ctx_free, 0);

        gtk_box_pack_start(GTK_BOX(bi->pins_box), qbtn, FALSE, FALSE, 0);
    }
    g_list_free_full(pins, chicago_pin_entry_free);
    gtk_widget_show_all(bi->pins_box);
}

static void reload_all_taskbar_pins(void) {
    for (int i = 0; i < g_tb.bar_count; i++) {
        populate_pins_for_bar(&g_tb.bars[i]);
    }
}

static void on_pins_file_changed(GFileMonitor *mon, GFile *file, GFile *other, GFileMonitorEvent ev, gpointer data) {
    (void)mon; (void)file; (void)other; (void)data;
    if (ev == G_FILE_MONITOR_EVENT_CHANGED ||
        ev == G_FILE_MONITOR_EVENT_CREATED ||
        ev == G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT) {
        reload_all_taskbar_pins();
    }
}

static void create_taskbar_for_monitor(GdkMonitor *mon) {
    if (g_tb.bar_count >= MAX_BAR_MONITORS) return;
    BarInstance *bi = &g_tb.bars[g_tb.bar_count++];
    memset(bi, 0, sizeof(*bi));
    bi->monitor = mon;

    bi->bar_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(bi->bar_window), "Chicago95 Taskbar");
    gtk_style_context_add_class(gtk_widget_get_style_context(bi->bar_window), "chicago-taskbar");
    gtk_widget_add_events(bi->bar_window, GDK_BUTTON_PRESS_MASK);
    g_signal_connect(bi->bar_window, "button-press-event", G_CALLBACK(on_bar_window_button_press), bi);

    if (gtk_layer_is_supported()) {
        gtk_layer_init_for_window(GTK_WINDOW(bi->bar_window));
        gtk_layer_set_layer(GTK_WINDOW(bi->bar_window), GTK_LAYER_SHELL_LAYER_TOP);
        if (mon) {
            gtk_layer_set_monitor(GTK_WINDOW(bi->bar_window), mon);
            g_object_set_data(G_OBJECT(bi->bar_window), "chicago-monitor", mon);
        }
        gtk_layer_set_anchor(GTK_WINDOW(bi->bar_window), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
        gtk_layer_set_anchor(GTK_WINDOW(bi->bar_window), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
        gtk_layer_set_anchor(GTK_WINDOW(bi->bar_window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
        gtk_layer_set_exclusive_zone(GTK_WINDOW(bi->bar_window), 28);
        gtk_layer_set_namespace(GTK_WINDOW(bi->bar_window), "chicago-taskbar");
    }
    gtk_widget_set_size_request(bi->bar_window, -1, 28);

    GtkWidget *root_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_style_context_add_class(gtk_widget_get_style_context(root_hbox), "win95-panel-top-bevel");

    /* 1. Classic Windows 95 Start Button */
    bi->start_button = gtk_button_new();
    gtk_style_context_add_class(gtk_widget_get_style_context(bi->start_button), "start-btn");
    gtk_widget_add_events(bi->start_button, GDK_BUTTON_PRESS_MASK);
    GtkWidget *start_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GdkPixbuf *flag_pb = chicago_draw_win95_flag_pixbuf(16);
    GtkWidget *flag_img = gtk_image_new_from_pixbuf(flag_pb);
    if (flag_pb) g_object_unref(flag_pb);
    GtkWidget *start_lbl = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(start_lbl), "<b>Start</b>");
    gtk_box_pack_start(GTK_BOX(start_hbox), flag_img, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(start_hbox), start_lbl, FALSE, FALSE, 1);
    gtk_container_add(GTK_CONTAINER(bi->start_button), start_hbox);
    g_signal_connect(bi->start_button, "clicked", G_CALLBACK(on_start_btn_clicked), bi);
    g_signal_connect(bi->start_button, "button-press-event", G_CALLBACK(on_start_btn_button_press), bi);
    gtk_box_pack_start(GTK_BOX(root_hbox), bi->start_button, FALSE, FALSE, 2);

    /* Divider */
    gtk_box_pack_start(GTK_BOX(root_hbox), gtk_separator_new(GTK_ORIENTATION_VERTICAL), FALSE, FALSE, 1);

    /* 2. Dynamic Pinned Quick Launch Bar */
    bi->pins_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    populate_pins_for_bar(bi);
    gtk_box_pack_start(GTK_BOX(root_hbox), bi->pins_box, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(root_hbox), gtk_separator_new(GTK_ORIENTATION_VERTICAL), FALSE, FALSE, 2);

    /* 3. Running Application Window List */
    bi->task_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_box_pack_start(GTK_BOX(root_hbox), bi->task_box, TRUE, TRUE, 2);

    /* 4. Sunken 3D System Tray Well */
    GtkWidget *tray_well = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
    gtk_style_context_add_class(gtk_widget_get_style_context(tray_well), "win95-well");
    gtk_widget_set_margin_top(tray_well, 2);
    gtk_widget_set_margin_bottom(tray_well, 2);
    gtk_widget_set_margin_end(tray_well, 3);

    /* Speaker Icon */
    GtkWidget *vol_btn = gtk_button_new();
    gtk_style_context_add_class(gtk_widget_get_style_context(vol_btn), "tray-btn");
    gtk_widget_add_events(vol_btn, GDK_BUTTON_PRESS_MASK);
    GdkPixbuf *vpb = chicago_load_icon("audio-volume-high", 16);
    gtk_container_add(GTK_CONTAINER(vol_btn), gtk_image_new_from_pixbuf(vpb));
    if (vpb) g_object_unref(vpb);
    gtk_widget_set_tooltip_text(vol_btn, "Volume Control (Click to adjust)");
    g_signal_connect(vol_btn, "clicked", G_CALLBACK(on_tray_vol_clicked), bi);
    g_signal_connect(vol_btn, "button-press-event", G_CALLBACK(on_bar_window_button_press), bi);
    gtk_box_pack_start(GTK_BOX(tray_well), vol_btn, FALSE, FALSE, 0);

    /* Network Icon */
    GtkWidget *net_btn = gtk_button_new();
    gtk_style_context_add_class(gtk_widget_get_style_context(net_btn), "tray-btn");
    gtk_widget_add_events(net_btn, GDK_BUTTON_PRESS_MASK);
    GdkPixbuf *npb = chicago_load_icon("network-workgroup", 16);
    gtk_container_add(GTK_CONTAINER(net_btn), gtk_image_new_from_pixbuf(npb));
    if (npb) g_object_unref(npb);
    gtk_widget_set_tooltip_text(net_btn, "Network & Wi-Fi Connections");
    g_signal_connect(net_btn, "clicked", G_CALLBACK(on_tray_net_clicked), bi);
    g_signal_connect(net_btn, "button-press-event", G_CALLBACK(on_bar_window_button_press), bi);
    gtk_box_pack_start(GTK_BOX(tray_well), net_btn, FALSE, FALSE, 0);

    /* Battery / Power Icon */
    GtkWidget *pwr_btn = gtk_button_new();
    gtk_style_context_add_class(gtk_widget_get_style_context(pwr_btn), "tray-btn");
    gtk_widget_add_events(pwr_btn, GDK_BUTTON_PRESS_MASK);
    GdkPixbuf *ppb = chicago_load_icon("battery-good", 16);
    gtk_container_add(GTK_CONTAINER(pwr_btn), gtk_image_new_from_pixbuf(ppb));
    if (ppb) g_object_unref(ppb);
    gtk_widget_set_tooltip_text(pwr_btn, "Power Meter & Screen Brightness");
    g_signal_connect(pwr_btn, "clicked", G_CALLBACK(on_tray_pwr_clicked), bi);
    g_signal_connect(pwr_btn, "button-press-event", G_CALLBACK(on_bar_window_button_press), bi);
    gtk_box_pack_start(GTK_BOX(tray_well), pwr_btn, FALSE, FALSE, 0);

    /* Clock Button */
    GtkWidget *clk_btn = gtk_button_new();
    gtk_style_context_add_class(gtk_widget_get_style_context(clk_btn), "tray-btn");
    gtk_widget_add_events(clk_btn, GDK_BUTTON_PRESS_MASK);
    bi->clock_label = gtk_label_new("12:00 PM");
    gtk_container_add(GTK_CONTAINER(clk_btn), bi->clock_label);
    g_signal_connect(clk_btn, "clicked", G_CALLBACK(on_tray_clock_clicked), bi);
    g_signal_connect(clk_btn, "button-press-event", G_CALLBACK(on_bar_window_button_press), bi);
    gtk_box_pack_start(GTK_BOX(tray_well), clk_btn, FALSE, FALSE, 2);

    gtk_box_pack_end(GTK_BOX(root_hbox), tray_well, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(bi->bar_window), root_hbox);
    gtk_widget_show_all(bi->bar_window);
}

int main(int argc, char **argv) {
    gtk_init(&argc, &argv);
    chicago_apply_theme();

    memset(&g_tb, 0, sizeof(g_tb));
    scan_desktop_applications();
    build_start_menu_and_flyouts();

    /* Create a 28px Windows 95 Taskbar on EVERY connected monitor */
    GdkDisplay *gdk_dpy = gdk_display_get_default();
    int n_mons = gdk_dpy ? gdk_display_get_n_monitors(gdk_dpy) : 0;
    if (n_mons > 0) {
        for (int i = 0; i < n_mons && i < MAX_BAR_MONITORS; i++) {
            create_taskbar_for_monitor(gdk_display_get_monitor(gdk_dpy, i));
        }
    } else {
        create_taskbar_for_monitor(NULL);
    }

    update_clock_labels();
    schedule_next_minute_tick();

    /* Bind zwlr_foreign_toplevel_manager_v1 */
    if (gdk_dpy && GDK_IS_WAYLAND_DISPLAY(gdk_dpy)) {
        g_tb.wl_display = gdk_wayland_display_get_wl_display(gdk_dpy);
        if (g_tb.wl_display) {
            g_tb.wl_registry = wl_display_get_registry(g_tb.wl_display);
            wl_registry_add_listener(g_tb.wl_registry, &registry_listener, NULL);
            wl_display_roundtrip(g_tb.wl_display);
        }
    }

    char pins_path[512];
    snprintf(pins_path, sizeof(pins_path), "%s/.config/chicago-wayland/pins.conf", g_get_home_dir());
    GFile *pf = g_file_new_for_path(pins_path);
    g_tb.pins_monitor = g_file_monitor_file(pf, G_FILE_MONITOR_NONE, NULL, NULL);
    if (g_tb.pins_monitor) {
        g_signal_connect(g_tb.pins_monitor, "changed", G_CALLBACK(on_pins_file_changed), NULL);
    }
    g_object_unref(pf);

    g_unix_signal_add(SIGUSR1, on_sigusr1, NULL);
    g_unix_signal_add(SIGUSR2, on_sigusr2, NULL);

    gtk_main();
    return 0;
}
