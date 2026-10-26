/*
 * chicago-taskbar: Native Wayland Windows 95 Taskbar, Start Menu & System Tray
 * Uses GTK3 + gtk-layer-shell + wlr-foreign-toplevel-management-unstable-v1
 * Zero-polling event-driven architecture (minute-aligned clock, Wayland socket events)
 * License: GPL-3.0+
 */

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkwayland.h>
#include <gdk/gdkkeysyms.h>
#include <gtk-layer-shell/gtk-layer-shell.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include <glib-unix.h>
#include <wayland-client.h>
#include <time.h>
#include "chicago_common.h"
#include "wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"

#define TASKBAR_HEIGHT 28
#define MAX_TASK_BUTTON_WIDTH 160

typedef struct ToplevelEntry {
    struct zwlr_foreign_toplevel_handle_v1 *handle;
    char *title;
    char *app_id;
    gboolean activated;
    gboolean minimized;
    gboolean maximized;
    GtkWidget *button;
    GtkWidget *icon_img;
    GtkWidget *label;
    struct ToplevelEntry *next;
} ToplevelEntry;

typedef struct {
    GtkWidget *bar_window;
    GtkWidget *start_btn;
    GtkWidget *task_box;
    GtkWidget *clock_label;
    GtkWidget *vol_btn;
    GtkWidget *vol_img;
    GtkWidget *net_btn;
    GtkWidget *bat_btn;

    GtkWidget *start_popup;
    gboolean start_open;

    GtkWidget *vol_popup;
    gboolean vol_open;

    GtkWidget *cal_popup;
    gboolean cal_open;

    struct wl_display *wl_display;
    struct wl_registry *wl_registry;
    struct wl_seat *wl_seat;
    struct zwlr_foreign_toplevel_manager_v1 *toplevel_mgr;
    ToplevelEntry *toplevels;

    GFileMonitor *app_dir_monitor_sys;
    GFileMonitor *app_dir_monitor_user;
} TaskbarState;

static TaskbarState g_tb = {0};

static void close_all_popups(void) {
    if (g_tb.start_open && g_tb.start_popup) {
        gtk_widget_hide(g_tb.start_popup);
        g_tb.start_open = FALSE;
        GtkStyleContext *ctx = gtk_widget_get_style_context(g_tb.start_btn);
        gtk_style_context_remove_class(ctx, "win95-btn-active");
    }
    if (g_tb.vol_open && g_tb.vol_popup) {
        gtk_widget_hide(g_tb.vol_popup);
        g_tb.vol_open = FALSE;
    }
    if (g_tb.cal_open && g_tb.cal_popup) {
        gtk_widget_hide(g_tb.cal_popup);
        g_tb.cal_open = FALSE;
    }
}

/* --- Helper: Resolve app_id to icon name --- */
static GdkPixbuf *resolve_app_icon(const char *app_id, int size) {
    if (!app_id || !*app_id) {
        return chicago_load_icon("application-x-executable", size);
    }

    char desktop_id[256];
    snprintf(desktop_id, sizeof(desktop_id), "%s.desktop", app_id);
    GDesktopAppInfo *info = g_desktop_app_info_new(desktop_id);
    if (!info) {
        char *lower = g_ascii_strdown(app_id, -1);
        snprintf(desktop_id, sizeof(desktop_id), "%s.desktop", lower);
        info = g_desktop_app_info_new(desktop_id);
        g_free(lower);
    }
    if (info) {
        GIcon *gicon = g_app_info_get_icon(G_APP_INFO(info));
        if (gicon) {
            char *icon_name = g_icon_to_string(gicon);
            if (icon_name) {
                GdkPixbuf *pb = chicago_load_icon(icon_name, size);
                g_free(icon_name);
                g_object_unref(info);
                if (pb) return pb;
            }
        }
        g_object_unref(info);
    }

    char *lower_id = g_ascii_strdown(app_id, -1);
    GdkPixbuf *pb = chicago_load_icon(lower_id, size);
    g_free(lower_id);
    return pb;
}

/* --- Toplevel Taskbar Button Management --- */
static void update_toplevel_button(ToplevelEntry *entry) {
    if (!entry || !entry->button) return;

    const char *display_title = (entry->title && *entry->title)
        ? entry->title
        : ((entry->app_id && *entry->app_id) ? entry->app_id : "Window");

    gtk_label_set_text(GTK_LABEL(entry->label), display_title);
    gtk_widget_set_tooltip_text(entry->button, display_title);

    GdkPixbuf *pb = resolve_app_icon(entry->app_id, 16);
    if (pb) {
        gtk_image_set_from_pixbuf(GTK_IMAGE(entry->icon_img), pb);
        g_object_unref(pb);
    }

    GtkStyleContext *ctx = gtk_widget_get_style_context(entry->button);
    if (entry->activated && !entry->minimized) {
        gtk_style_context_add_class(ctx, "active-task");
        gtk_style_context_add_class(ctx, "win95-btn-active");
    } else {
        gtk_style_context_remove_class(ctx, "active-task");
        gtk_style_context_remove_class(ctx, "win95-btn-active");
    }
}

static void on_task_ctx_restore(GtkMenuItem *item, gpointer user_data) {
    (void)item;
    ToplevelEntry *entry = (ToplevelEntry *)user_data;
    if (!entry || !entry->handle) return;
    zwlr_foreign_toplevel_handle_v1_unset_minimized(entry->handle);
    zwlr_foreign_toplevel_handle_v1_unset_maximized(entry->handle);
    if (g_tb.wl_seat) {
        zwlr_foreign_toplevel_handle_v1_activate(entry->handle, g_tb.wl_seat);
    }
    wl_display_flush(g_tb.wl_display);
}

static void on_task_ctx_minimize(GtkMenuItem *item, gpointer user_data) {
    (void)item;
    ToplevelEntry *entry = (ToplevelEntry *)user_data;
    if (!entry || !entry->handle) return;
    zwlr_foreign_toplevel_handle_v1_set_minimized(entry->handle);
    wl_display_flush(g_tb.wl_display);
}

static void on_task_ctx_maximize(GtkMenuItem *item, gpointer user_data) {
    (void)item;
    ToplevelEntry *entry = (ToplevelEntry *)user_data;
    if (!entry || !entry->handle) return;
    zwlr_foreign_toplevel_handle_v1_unset_minimized(entry->handle);
    zwlr_foreign_toplevel_handle_v1_set_maximized(entry->handle);
    if (g_tb.wl_seat) {
        zwlr_foreign_toplevel_handle_v1_activate(entry->handle, g_tb.wl_seat);
    }
    wl_display_flush(g_tb.wl_display);
}

static void on_task_ctx_close(GtkMenuItem *item, gpointer user_data) {
    (void)item;
    ToplevelEntry *entry = (ToplevelEntry *)user_data;
    if (!entry || !entry->handle) return;
    zwlr_foreign_toplevel_handle_v1_close(entry->handle);
    wl_display_flush(g_tb.wl_display);
}

static gboolean on_task_button_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    (void)widget;
    ToplevelEntry *entry = (ToplevelEntry *)user_data;
    if (!entry || !entry->handle) return FALSE;

    close_all_popups();

    if (event->button == 1) {
        if (entry->activated && !entry->minimized) {
            zwlr_foreign_toplevel_handle_v1_set_minimized(entry->handle);
        } else {
            zwlr_foreign_toplevel_handle_v1_unset_minimized(entry->handle);
            if (g_tb.wl_seat) {
                zwlr_foreign_toplevel_handle_v1_activate(entry->handle, g_tb.wl_seat);
            }
        }
        wl_display_flush(g_tb.wl_display);
        return TRUE;
    } else if (event->button == 3) {
        GtkWidget *menu = gtk_menu_new();

        GtkWidget *m_restore = gtk_menu_item_new_with_label("Restore");
        g_signal_connect(m_restore, "activate", G_CALLBACK(on_task_ctx_restore), entry);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), m_restore);

        GtkWidget *m_min = gtk_menu_item_new_with_label("Minimize");
        g_signal_connect(m_min, "activate", G_CALLBACK(on_task_ctx_minimize), entry);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), m_min);

        GtkWidget *m_max = gtk_menu_item_new_with_label("Maximize");
        g_signal_connect(m_max, "activate", G_CALLBACK(on_task_ctx_maximize), entry);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), m_max);

        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

        GtkWidget *m_close = gtk_menu_item_new_with_label("Close");
        g_signal_connect(m_close, "activate", G_CALLBACK(on_task_ctx_close), entry);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), m_close);

        gtk_widget_show_all(menu);
        gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)event);
        return TRUE;
    }
    return FALSE;
}

/* --- Wayland zwlr_foreign_toplevel_handle_v1 Callbacks --- */
static void toplevel_handle_title(void *data,
                                  struct zwlr_foreign_toplevel_handle_v1 *handle,
                                  const char *title) {
    (void)handle;
    ToplevelEntry *entry = (ToplevelEntry *)data;
    g_free(entry->title);
    entry->title = g_strdup(title ? title : "");
}

static void toplevel_handle_app_id(void *data,
                                   struct zwlr_foreign_toplevel_handle_v1 *handle,
                                   const char *app_id) {
    (void)handle;
    ToplevelEntry *entry = (ToplevelEntry *)data;
    g_free(entry->app_id);
    entry->app_id = g_strdup(app_id ? app_id : "");
}

static void toplevel_handle_output_enter(void *data,
                                         struct zwlr_foreign_toplevel_handle_v1 *handle,
                                         struct wl_output *output) {
    (void)data;
    (void)handle;
    (void)output;
}

static void toplevel_handle_output_leave(void *data,
                                         struct zwlr_foreign_toplevel_handle_v1 *handle,
                                         struct wl_output *output) {
    (void)data;
    (void)handle;
    (void)output;
}

static void toplevel_handle_state(void *data,
                                  struct zwlr_foreign_toplevel_handle_v1 *handle,
                                  struct wl_array *state) {
    (void)handle;
    ToplevelEntry *entry = (ToplevelEntry *)data;
    entry->activated = FALSE;
    entry->minimized = FALSE;
    entry->maximized = FALSE;

    uint32_t *val;
    wl_array_for_each(val, state) {
        if (*val == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED) {
            entry->activated = TRUE;
        } else if (*val == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MINIMIZED) {
            entry->minimized = TRUE;
        } else if (*val == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MAXIMIZED) {
            entry->maximized = TRUE;
        }
    }
}

static void toplevel_handle_done(void *data,
                                 struct zwlr_foreign_toplevel_handle_v1 *handle) {
    (void)handle;
    ToplevelEntry *entry = (ToplevelEntry *)data;
    update_toplevel_button(entry);
}

static void toplevel_handle_closed(void *data,
                                   struct zwlr_foreign_toplevel_handle_v1 *handle) {
    ToplevelEntry *entry = (ToplevelEntry *)data;

    ToplevelEntry **pp = &g_tb.toplevels;
    while (*pp) {
        if (*pp == entry) {
            *pp = entry->next;
            break;
        }
        pp = &((*pp)->next);
    }

    if (entry->button) {
        gtk_widget_destroy(entry->button);
    }
    zwlr_foreign_toplevel_handle_v1_destroy(handle);
    g_free(entry->title);
    g_free(entry->app_id);
    g_free(entry);
}

static void toplevel_handle_parent(void *data,
                                   struct zwlr_foreign_toplevel_handle_v1 *handle,
                                   struct zwlr_foreign_toplevel_handle_v1 *parent) {
    (void)data;
    (void)handle;
    (void)parent;
}

static const struct zwlr_foreign_toplevel_handle_v1_listener toplevel_handle_listener = {
    .title = toplevel_handle_title,
    .app_id = toplevel_handle_app_id,
    .output_enter = toplevel_handle_output_enter,
    .output_leave = toplevel_handle_output_leave,
    .state = toplevel_handle_state,
    .done = toplevel_handle_done,
    .closed = toplevel_handle_closed,
    .parent = toplevel_handle_parent,
};

static void toplevel_mgr_toplevel(void *data,
                                  struct zwlr_foreign_toplevel_manager_v1 *mgr,
                                  struct zwlr_foreign_toplevel_handle_v1 *handle) {
    (void)data;
    (void)mgr;
    ToplevelEntry *entry = g_malloc0(sizeof(ToplevelEntry));
    entry->handle = handle;
    entry->next = g_tb.toplevels;
    g_tb.toplevels = entry;

    GtkWidget *btn = gtk_button_new();
    gtk_widget_set_size_request(btn, MAX_TASK_BUTTON_WIDTH, 22);
    GtkStyleContext *ctx = gtk_widget_get_style_context(btn);
    gtk_style_context_add_class(ctx, "task-btn");

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget *img = gtk_image_new();
    GtkWidget *lbl = gtk_label_new("Window");
    gtk_label_set_ellipsize(GTK_LABEL(lbl), PANGO_ELLIPSIZE_END);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0);
    gtk_label_set_max_width_chars(GTK_LABEL(lbl), 18);

    gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), lbl, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(btn), hbox);

    entry->button = btn;
    entry->icon_img = img;
    entry->label = lbl;

    g_signal_connect(btn, "button-press-event", G_CALLBACK(on_task_button_press), entry);

    gtk_box_pack_start(GTK_BOX(g_tb.task_box), btn, FALSE, TRUE, 1);
    gtk_widget_show_all(btn);

    zwlr_foreign_toplevel_handle_v1_add_listener(handle, &toplevel_handle_listener, entry);
}

static void toplevel_mgr_finished(void *data,
                                  struct zwlr_foreign_toplevel_manager_v1 *mgr) {
    (void)data;
    zwlr_foreign_toplevel_manager_v1_destroy(mgr);
    g_tb.toplevel_mgr = NULL;
}

static const struct zwlr_foreign_toplevel_manager_v1_listener toplevel_mgr_listener = {
    .toplevel = toplevel_mgr_toplevel,
    .finished = toplevel_mgr_finished,
};

static void registry_global(void *data, struct wl_registry *registry,
                            uint32_t name, const char *interface, uint32_t version) {
    (void)data;
    if (strcmp(interface, zwlr_foreign_toplevel_manager_v1_interface.name) == 0) {
        uint32_t bind_ver = version < 3 ? version : 3;
        g_tb.toplevel_mgr = wl_registry_bind(
            registry, name, &zwlr_foreign_toplevel_manager_v1_interface, bind_ver);
        zwlr_foreign_toplevel_manager_v1_add_listener(
            g_tb.toplevel_mgr, &toplevel_mgr_listener, NULL);
    } else if (strcmp(interface, wl_seat_interface.name) == 0 && !g_tb.wl_seat) {
        g_tb.wl_seat = wl_registry_bind(registry, name, &wl_seat_interface, 1);
    }
}

static void registry_global_remove(void *data, struct wl_registry *registry, uint32_t name) {
    (void)data;
    (void)registry;
    (void)name;
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

/* --- Windows 95 Vertical Banner Drawing for Start Menu --- */
static gboolean on_draw_start_banner(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    (void)user_data;
    int w = gtk_widget_get_allocated_width(widget);
    int h = gtk_widget_get_allocated_height(widget);

    /* Classic Windows 95 #808080 dark gray sidebar with #000080 bottom accent */
    cairo_set_source_rgb(cr, 0.502, 0.502, 0.502);
    cairo_rectangle(cr, 0, 0, w, h);
    cairo_fill(cr);

    cairo_save(cr);
    cairo_translate(cr, 4, h - 10);
    cairo_rotate(cr, -G_PI / 2.0);

    PangoLayout *layout = gtk_widget_create_pango_layout(widget, "");
    pango_layout_set_markup(
        layout,
        "<span font_desc='Sans Bold 13' foreground='#ffffff'>Windows</span>"
        "<span font_desc='Sans 13' foreground='#c0c0c0'> 95</span>",
        -1);
    pango_cairo_show_layout(cr, layout);
    g_object_unref(layout);
    cairo_restore(cr);

    return FALSE;
}

/* --- Start Menu Item Factory --- */
static void closure_free_str(gpointer data, GClosure *closure) {
    (void)closure;
    g_free(data);
}

static void closure_unref_obj(gpointer data, GClosure *closure) {
    (void)closure;
    if (data) g_object_unref(data);
}

static void on_start_cmd_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    const char *cmd = (const char *)user_data;
    close_all_popups();
    chicago_spawn_command(cmd);
}

static void on_launch_app_info(GtkMenuItem *item, gpointer user_data) {
    (void)item;
    GAppInfo *app = G_APP_INFO(user_data);
    close_all_popups();
    if (app) {
        g_app_info_launch(app, NULL, NULL, NULL);
    }
}

static GtkWidget *create_start_row_button(const char *label_text, const char *icon_name,
                                          int icon_sz, const char *cmd_str, gboolean has_arrow) {
    GtkWidget *btn = gtk_button_new();
    gtk_button_set_relief(GTK_BUTTON(btn), GTK_RELIEF_NONE);
    GtkStyleContext *ctx = gtk_widget_get_style_context(btn);
    gtk_style_context_add_class(ctx, "win95-menuitem");

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GdkPixbuf *pb = chicago_load_icon(icon_name, icon_sz);
    GtkWidget *img = gtk_image_new_from_pixbuf(pb);
    if (pb) g_object_unref(pb);

    GtkWidget *lbl = gtk_label_new_with_mnemonic(label_text);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0);

    gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(hbox), lbl, TRUE, TRUE, 2);

    if (has_arrow) {
        GtkWidget *arr = gtk_label_new("▸");
        gtk_box_pack_end(GTK_BOX(hbox), arr, FALSE, FALSE, 2);
    }

    gtk_container_add(GTK_CONTAINER(btn), hbox);
    if (cmd_str) {
        g_signal_connect_data(btn, "clicked", G_CALLBACK(on_start_cmd_clicked),
                              g_strdup(cmd_str), closure_free_str, 0);
    }
    return btn;
}

static int compare_app_info(gconstpointer a, gconstpointer b) {
    GAppInfo *app_a = G_APP_INFO((gpointer)a);
    GAppInfo *app_b = G_APP_INFO((gpointer)b);
    const char *na = g_app_info_get_display_name(app_a);
    const char *nb = g_app_info_get_display_name(app_b);
    return g_ascii_strcasecmp(na ? na : "", nb ? nb : "");
}

static const char *classify_desktop_category(const char *cats) {
    if (!cats) return "Accessories";
    if (strstr(cats, "System") || strstr(cats, "Monitor")) return "System Tools";
    if (strstr(cats, "Development") || strstr(cats, "IDE")) return "Development";
    if (strstr(cats, "Network") || strstr(cats, "WebBrowser")) return "Internet";
    if (strstr(cats, "AudioVideo") || strstr(cats, "Audio") || strstr(cats, "Video")) return "Multimedia";
    if (strstr(cats, "Graphics")) return "Graphics";
    if (strstr(cats, "Office")) return "Office";
    if (strstr(cats, "Game")) return "Games";
    if (strstr(cats, "Settings")) return "Settings";
    return "Accessories";
}

static GtkWidget *build_programs_menu(void) {
    GtkWidget *menu = gtk_menu_new();
    const char *cat_names[] = {
        "Accessories",
        "Development",
        "Games",
        "Graphics",
        "Internet",
        "Multimedia",
        "Office",
        "System Tools",
        NULL
    };
    const char *cat_icons[] = {
        "applications-utilities",
        "applications-development",
        "applications-games",
        "applications-graphics",
        "applications-internet",
        "applications-multimedia",
        "applications-office",
        "applications-system",
        NULL
    };

    GHashTable *submenus = g_hash_table_new(g_str_hash, g_str_equal);
    for (int i = 0; cat_names[i]; i++) {
        GtkWidget *cat_item = gtk_menu_item_new();
        GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        GdkPixbuf *pb = chicago_load_icon(cat_icons[i], 16);
        GtkWidget *img = gtk_image_new_from_pixbuf(pb);
        if (pb) g_object_unref(pb);
        GtkWidget *lbl = gtk_label_new(cat_names[i]);
        gtk_label_set_xalign(GTK_LABEL(lbl), 0.0);
        gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), lbl, TRUE, TRUE, 0);
        gtk_container_add(GTK_CONTAINER(cat_item), hbox);

        GtkWidget *sub = gtk_menu_new();
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(cat_item), sub);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), cat_item);
        g_hash_table_insert(submenus, (gpointer)cat_names[i], sub);
    }

    GList *apps = g_app_info_get_all();
    apps = g_list_sort(apps, compare_app_info);

    for (GList *l = apps; l != NULL; l = l->next) {
        GAppInfo *app = G_APP_INFO(l->data);
        if (!g_app_info_should_show(app)) continue;
        const char *name = g_app_info_get_display_name(app);
        if (!name || !*name) continue;

        const char *cats = NULL;
        if (G_IS_DESKTOP_APP_INFO(app)) {
            cats = g_desktop_app_info_get_categories(G_DESKTOP_APP_INFO(app));
        }
        const char *bucket = classify_desktop_category(cats);
        GtkWidget *target_sub = g_hash_table_lookup(submenus, bucket);
        if (!target_sub) target_sub = g_hash_table_lookup(submenus, "Accessories");

        GtkWidget *item = gtk_menu_item_new();
        GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        GIcon *gicon = g_app_info_get_icon(app);
        char *icon_str = gicon ? g_icon_to_string(gicon) : NULL;
        GdkPixbuf *pb = chicago_load_icon(icon_str ? icon_str : "application-x-executable", 16);
        g_free(icon_str);
        GtkWidget *img = gtk_image_new_from_pixbuf(pb);
        if (pb) g_object_unref(pb);

        GtkWidget *lbl = gtk_label_new(name);
        gtk_label_set_xalign(GTK_LABEL(lbl), 0.0);
        gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), lbl, TRUE, TRUE, 0);
        gtk_container_add(GTK_CONTAINER(item), hbox);

        g_signal_connect_data(item, "activate", G_CALLBACK(on_launch_app_info),
                              g_object_ref(app), closure_unref_obj, 0);
        gtk_menu_shell_append(GTK_MENU_SHELL(target_sub), item);
    }

    g_list_free_full(apps, g_object_unref);
    g_hash_table_destroy(submenus);

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
    GtkWidget *dos_item = gtk_menu_item_new_with_label("MS-DOS Prompt");
    g_signal_connect_data(dos_item, "activate", G_CALLBACK(on_start_cmd_clicked),
                          g_strdup("kitty --title \"MS-DOS Prompt\" -o background=#000000 -o foreground=#c0c0c0"),
                          closure_free_str, 0);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), dos_item);

    gtk_widget_show_all(menu);
    return menu;
}

static void on_programs_btn_clicked(GtkButton *btn, gpointer user_data) {
    (void)user_data;
    GtkWidget *menu = build_programs_menu();
    gtk_menu_popup_at_widget(GTK_MENU(menu), GTK_WIDGET(btn),
                             GDK_GRAVITY_NORTH_EAST, GDK_GRAVITY_NORTH_WEST, NULL);
}

static void on_documents_btn_clicked(GtkButton *btn, gpointer user_data) {
    (void)user_data;
    GtkWidget *menu = gtk_menu_new();
    const char *home = g_get_home_dir();
    struct {
        const char *label;
        const char *subpath;
    } folders[] = {
        { "My Documents", "Documents" },
        { "Downloads", "Downloads" },
        { "Pictures", "Pictures" },
        { "Desktop", "Desktop" },
        { "Home Folder", "" },
        { NULL, NULL }
    };

    for (int i = 0; folders[i].label; i++) {
        char cmd[512];
        if (*folders[i].subpath)
            snprintf(cmd, sizeof(cmd), "nautilus \"%s/%s\" 2>/dev/null || xdg-open \"%s/%s\"",
                     home, folders[i].subpath, home, folders[i].subpath);
        else
            snprintf(cmd, sizeof(cmd), "nautilus \"%s\" 2>/dev/null || xdg-open \"%s\"", home, home);

        GtkWidget *it = gtk_menu_item_new_with_label(folders[i].label);
        g_signal_connect_data(it, "activate", G_CALLBACK(on_start_cmd_clicked),
                              g_strdup(cmd), closure_free_str, 0);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), it);
    }

    gtk_widget_show_all(menu);
    gtk_menu_popup_at_widget(GTK_MENU(menu), GTK_WIDGET(btn),
                             GDK_GRAVITY_NORTH_EAST, GDK_GRAVITY_NORTH_WEST, NULL);
}

static void on_settings_btn_clicked(GtkButton *btn, gpointer user_data) {
    (void)user_data;
    GtkWidget *menu = gtk_menu_new();
    struct {
        const char *label;
        const char *cmd;
    } items[] = {
        { "Control Panel", "chicago-control-panel" },
        { "Display Properties", "chicago-control-panel --tab display" },
        { "Sound & Audio Devices", "chicago-control-panel --tab audio" },
        { "Network Connections", "chicago-control-panel --tab network" },
        { "Power Management", "chicago-control-panel --tab power" },
        { "Mouse & Keyboard", "chicago-control-panel --tab input" },
        { "System Properties", "chicago-control-panel --tab system" },
        { "Reload Window Manager (labwc -r)", "labwc -r" },
        { NULL, NULL }
    };

    for (int i = 0; items[i].label; i++) {
        GtkWidget *it = gtk_menu_item_new_with_label(items[i].label);
        g_signal_connect_data(it, "activate", G_CALLBACK(on_start_cmd_clicked),
                              g_strdup(items[i].cmd), closure_free_str, 0);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), it);
    }

    gtk_widget_show_all(menu);
    gtk_menu_popup_at_widget(GTK_MENU(menu), GTK_WIDGET(btn),
                             GDK_GRAVITY_NORTH_EAST, GDK_GRAVITY_NORTH_WEST, NULL);
}

static gboolean on_popup_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    (void)widget;
    (void)user_data;
    if (event->keyval == GDK_KEY_Escape) {
        close_all_popups();
        return TRUE;
    }
    return FALSE;
}

static void build_start_menu_popup(void) {
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_decorated(GTK_WINDOW(win), FALSE);
    gtk_layer_init_for_window(GTK_WINDOW(win));
    gtk_layer_set_layer(GTK_WINDOW(win), GTK_LAYER_SHELL_LAYER_OVERLAY);
    gtk_layer_set_namespace(GTK_WINDOW(win), "chicago-startmenu");
    gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_margin(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_BOTTOM, TASKBAR_HEIGHT);
    gtk_layer_set_margin(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_LEFT, 2);
    gtk_layer_set_keyboard_mode(GTK_WINDOW(win), GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);

    g_signal_connect(win, "key-press-event", G_CALLBACK(on_popup_key_press), NULL);

    GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkStyleContext *octx = gtk_widget_get_style_context(outer);
    gtk_style_context_add_class(octx, "win95-outset");

    /* Left vertical banner ("Windows 95") */
    GtkWidget *banner = gtk_drawing_area_new();
    gtk_widget_set_size_request(banner, 26, 310);
    g_signal_connect(banner, "draw", G_CALLBACK(on_draw_start_banner), NULL);
    gtk_box_pack_start(GTK_BOX(outer), banner, FALSE, TRUE, 0);

    /* Right menu column */
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
    gtk_widget_set_size_request(vbox, 200, -1);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 2);

    GtkWidget *btn_prog = create_start_row_button("_Programs", "applications-other", 24, NULL, TRUE);
    g_signal_connect(btn_prog, "clicked", G_CALLBACK(on_programs_btn_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), btn_prog, FALSE, FALSE, 0);

    GtkWidget *btn_docs = create_start_row_button("_Documents", "folder-documents", 24, NULL, TRUE);
    g_signal_connect(btn_docs, "clicked", G_CALLBACK(on_documents_btn_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), btn_docs, FALSE, FALSE, 0);

    GtkWidget *btn_set = create_start_row_button("_Settings", "preferences-system", 24, NULL, TRUE);
    g_signal_connect(btn_set, "clicked", G_CALLBACK(on_settings_btn_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), btn_set, FALSE, FALSE, 0);

    GtkWidget *btn_find = create_start_row_button("_Find...", "edit-find", 24, "chicago-control-panel --find", FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), btn_find, FALSE, FALSE, 0);

    GtkWidget *btn_help = create_start_row_button("_Help", "help-browser", 24, "chicago-control-panel --help-viewer", FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), btn_help, FALSE, FALSE, 0);

    GtkWidget *btn_run = create_start_row_button("_Run...", "system-run", 24, "chicago-control-panel --run", FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), btn_run, FALSE, FALSE, 0);

    GtkWidget *btn_dos = create_start_row_button("MS-_DOS Prompt", "utilities-terminal", 24,
                                                 "kitty --title \"MS-DOS Prompt\" -o background=#000000 -o foreground=#c0c0c0",
                                                 FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), btn_dos, FALSE, FALSE, 0);

    GtkWidget *btn_files = create_start_row_button("Windows _Explorer", "system-file-manager", 24,
                                                   "nautilus 2>/dev/null || xdg-open ~", FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), btn_files, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 3);

    GtkWidget *btn_lock = create_start_row_button("_Lock Workstation", "system-lock-screen", 24,
                                                  "swaylock -f -c 008080", FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), btn_lock, FALSE, FALSE, 0);

    GtkWidget *btn_shut = create_start_row_button("Sh_ut Down...", "system-shutdown", 24,
                                                  "chicago-control-panel --shutdown", FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), btn_shut, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(outer), vbox, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(win), outer);

    g_tb.start_popup = win;
}

static void toggle_start_menu(void) {
    if (!g_tb.start_popup) {
        build_start_menu_popup();
    }
    if (g_tb.start_open) {
        close_all_popups();
    } else {
        close_all_popups();
        gtk_widget_show_all(g_tb.start_popup);
        g_tb.start_open = TRUE;
        GtkStyleContext *ctx = gtk_widget_get_style_context(g_tb.start_btn);
        gtk_style_context_add_class(ctx, "win95-btn-active");
    }
}

static void on_start_btn_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    (void)user_data;
    toggle_start_menu();
}

static gboolean on_sigusr1(gpointer user_data) {
    (void)user_data;
    toggle_start_menu();
    return G_SOURCE_CONTINUE;
}

static gboolean on_sigusr2(gpointer user_data) {
    (void)user_data;
    close_all_popups();
    gboolean any_unminimized = FALSE;
    for (ToplevelEntry *e = g_tb.toplevels; e != NULL; e = e->next) {
        if (!e->minimized) {
            any_unminimized = TRUE;
            break;
        }
    }
    for (ToplevelEntry *e = g_tb.toplevels; e != NULL; e = e->next) {
        if (!e->handle) continue;
        if (any_unminimized) {
            zwlr_foreign_toplevel_handle_v1_set_minimized(e->handle);
        } else {
            zwlr_foreign_toplevel_handle_v1_unset_minimized(e->handle);
        }
    }
    if (g_tb.wl_display) {
        wl_display_flush(g_tb.wl_display);
    }
    return G_SOURCE_CONTINUE;
}

/* --- Volume Control Popup (On-Demand, Zero Idle Polling) --- */
static int get_current_volume_percent(gboolean *muted_out) {
    if (muted_out) *muted_out = FALSE;
    char *stdout_str = NULL;
    char *argv[] = { "wpctl", "get-volume", "@DEFAULT_AUDIO_SINK@", NULL };
    if (g_spawn_sync(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, &stdout_str, NULL, NULL, NULL) && stdout_str) {
        if (muted_out && strstr(stdout_str, "[MUTED]")) {
            *muted_out = TRUE;
        }
        const char *colon = strchr(stdout_str, ':');
        int pct = 75;
        if (colon) {
            double v = g_ascii_strtod(colon + 1, NULL);
            pct = (int)(v * 100.0 + 0.5);
        }
        g_free(stdout_str);
        return CLAMP(pct, 0, 150);
    }
    g_free(stdout_str);
    return 75;
}

static void on_vol_scale_changed(GtkRange *range, gpointer user_data) {
    (void)user_data;
    int val = (int)gtk_range_get_value(range);
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "wpctl set-volume @DEFAULT_AUDIO_SINK@ %d%%", CLAMP(val, 0, 150));
    chicago_spawn_command(cmd);
}

static void on_vol_mute_toggled(GtkToggleButton *toggle, gpointer user_data) {
    (void)user_data;
    gboolean muted = gtk_toggle_button_get_active(toggle);
    chicago_spawn_command(muted ? "wpctl set-mute @DEFAULT_AUDIO_SINK@ 1"
                                : "wpctl set-mute @DEFAULT_AUDIO_SINK@ 0");
}

static void toggle_volume_popup(void) {
    if (g_tb.vol_open) {
        close_all_popups();
        return;
    }
    close_all_popups();

    if (g_tb.vol_popup) {
        gtk_widget_destroy(g_tb.vol_popup);
        g_tb.vol_popup = NULL;
    }

    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_decorated(GTK_WINDOW(win), FALSE);
    gtk_layer_init_for_window(GTK_WINDOW(win));
    gtk_layer_set_layer(GTK_WINDOW(win), GTK_LAYER_SHELL_LAYER_OVERLAY);
    gtk_layer_set_namespace(GTK_WINDOW(win), "chicago-volume");
    gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
    gtk_layer_set_margin(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_BOTTOM, TASKBAR_HEIGHT);
    gtk_layer_set_margin(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_RIGHT, 40);
    gtk_layer_set_keyboard_mode(GTK_WINDOW(win), GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);
    g_signal_connect(win, "key-press-event", G_CALLBACK(on_popup_key_press), NULL);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(box), 8);
    gtk_style_context_add_class(gtk_widget_get_style_context(box), "win95-outset");

    GtkWidget *title = gtk_label_new("Volume");
    gtk_box_pack_start(GTK_BOX(box), title, FALSE, FALSE, 0);

    gboolean is_muted = FALSE;
    int cur_vol = get_current_volume_percent(&is_muted);

    GtkWidget *scale = gtk_scale_new_with_range(GTK_ORIENTATION_VERTICAL, 0, 100, 5);
    gtk_range_set_inverted(GTK_RANGE(scale), TRUE);
    gtk_scale_set_draw_value(GTK_SCALE(scale), TRUE);
    gtk_widget_set_size_request(scale, 44, 120);
    gtk_range_set_value(GTK_RANGE(scale), cur_vol);
    g_signal_connect(scale, "value-changed", G_CALLBACK(on_vol_scale_changed), NULL);
    gtk_box_pack_start(GTK_BOX(box), scale, TRUE, TRUE, 0);

    GtkWidget *mute_chk = gtk_check_button_new_with_label("Mute");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mute_chk), is_muted);
    g_signal_connect(mute_chk, "toggled", G_CALLBACK(on_vol_mute_toggled), NULL);
    gtk_box_pack_start(GTK_BOX(box), mute_chk, FALSE, FALSE, 0);

    GtkWidget *mixer_btn = gtk_button_new_with_label("Mixer...");
    g_signal_connect_data(mixer_btn, "clicked", G_CALLBACK(on_start_cmd_clicked),
                          g_strdup("chicago-control-panel --tab audio"), closure_free_str, 0);
    gtk_box_pack_start(GTK_BOX(box), mixer_btn, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(win), box);
    gtk_widget_show_all(win);
    g_tb.vol_popup = win;
    g_tb.vol_open = TRUE;
}

static gboolean on_vol_btn_event(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
    (void)widget;
    (void)user_data;
    if (event->type == GDK_BUTTON_PRESS) {
        GdkEventButton *bev = (GdkEventButton *)event;
        if (bev->button == 1) {
            toggle_volume_popup();
            return TRUE;
        } else if (bev->button == 2) {
            chicago_spawn_command("wpctl set-mute @DEFAULT_AUDIO_SINK@ toggle");
            return TRUE;
        } else if (bev->button == 3) {
            close_all_popups();
            chicago_spawn_command("chicago-control-panel --tab audio");
            return TRUE;
        }
    } else if (event->type == GDK_SCROLL) {
        GdkEventScroll *sev = (GdkEventScroll *)event;
        if (sev->direction == GDK_SCROLL_UP) {
            chicago_spawn_command("wpctl set-volume @DEFAULT_AUDIO_SINK@ 5%+");
            return TRUE;
        } else if (sev->direction == GDK_SCROLL_DOWN) {
            chicago_spawn_command("wpctl set-volume @DEFAULT_AUDIO_SINK@ 5%-");
            return TRUE;
        }
    }
    return FALSE;
}

/* --- Calendar / Date-Time Popup --- */
static void toggle_calendar_popup(void) {
    if (g_tb.cal_open) {
        close_all_popups();
        return;
    }
    close_all_popups();

    if (!g_tb.cal_popup) {
        GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_decorated(GTK_WINDOW(win), FALSE);
        gtk_layer_init_for_window(GTK_WINDOW(win));
        gtk_layer_set_layer(GTK_WINDOW(win), GTK_LAYER_SHELL_LAYER_OVERLAY);
        gtk_layer_set_namespace(GTK_WINDOW(win), "chicago-calendar");
        gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
        gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
        gtk_layer_set_margin(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_BOTTOM, TASKBAR_HEIGHT);
        gtk_layer_set_margin(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_RIGHT, 2);
        gtk_layer_set_keyboard_mode(GTK_WINDOW(win), GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);
        g_signal_connect(win, "key-press-event", G_CALLBACK(on_popup_key_press), NULL);

        GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
        gtk_container_set_border_width(GTK_CONTAINER(box), 8);
        gtk_style_context_add_class(gtk_widget_get_style_context(box), "win95-outset");

        GtkWidget *hdr = gtk_label_new("Date/Time Properties");
        gtk_box_pack_start(GTK_BOX(box), hdr, FALSE, FALSE, 0);

        GtkWidget *cal = gtk_calendar_new();
        gtk_box_pack_start(GTK_BOX(box), cal, TRUE, TRUE, 0);

        gtk_container_add(GTK_CONTAINER(win), box);
        g_tb.cal_popup = win;
    }

    gtk_widget_show_all(g_tb.cal_popup);
    g_tb.cal_open = TRUE;
}

static gboolean on_clock_btn_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    (void)widget;
    (void)user_data;
    if (event->button == 1) {
        toggle_calendar_popup();
        return TRUE;
    }
    return FALSE;
}

/* --- Minute-Boundary Aligned Clock (1 wakeup per 60 seconds!) --- */
static gboolean on_clock_minute_tick(gpointer user_data);

static void update_clock_display(void) {
    time_t now = time(NULL);
    struct tm *lt = localtime(&now);
    if (!lt) return;

    char time_str[64];
    strftime(time_str, sizeof(time_str), "%I:%M %p", lt);
    /* Trim leading zero in hour for authentic Win95 look (e.g. "6:20 PM") */
    const char *disp = (time_str[0] == '0') ? (time_str + 1) : time_str;
    gtk_label_set_text(GTK_LABEL(g_tb.clock_label), disp);

    char date_str[128];
    strftime(date_str, sizeof(date_str), "%A, %B %d, %Y", lt);
    gtk_widget_set_tooltip_text(g_tb.clock_label, date_str);

    /* Schedule next wakeup strictly on the next minute boundary */
    int secs_remaining = 60 - lt->tm_sec;
    if (secs_remaining < 1 || secs_remaining > 60) secs_remaining = 60;
    g_timeout_add_seconds(secs_remaining, on_clock_minute_tick, NULL);
}

static gboolean on_clock_minute_tick(gpointer user_data) {
    (void)user_data;
    update_clock_display();
    return G_SOURCE_REMOVE; /* Re-scheduled with exact boundary alignment */
}

static void on_net_btn_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    (void)user_data;
    close_all_popups();
    chicago_spawn_command("chicago-control-panel --tab network");
}

static void on_bat_btn_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    (void)user_data;
    close_all_popups();
    chicago_spawn_command("chicago-control-panel --tab power");
}

static void create_taskbar_window(void) {
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Chicago95 Taskbar");
    gtk_window_set_decorated(GTK_WINDOW(win), FALSE);
    gtk_style_context_add_class(gtk_widget_get_style_context(win), "chicago-taskbar");

    gtk_layer_init_for_window(GTK_WINDOW(win));
    gtk_layer_set_layer(GTK_WINDOW(win), GTK_LAYER_SHELL_LAYER_TOP);
    gtk_layer_set_namespace(GTK_WINDOW(win), "chicago-taskbar");
    gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
    gtk_layer_set_exclusive_zone(GTK_WINDOW(win), TASKBAR_HEIGHT);
    gtk_widget_set_size_request(win, -1, TASKBAR_HEIGHT);

    GtkWidget *root_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_container_set_border_width(GTK_CONTAINER(root_hbox), 1);

    /* 1. Start Button */
    GtkWidget *start_btn = gtk_button_new();
    gtk_style_context_add_class(gtk_widget_get_style_context(start_btn), "start-btn");
    GtkWidget *start_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GdkPixbuf *flag_pb = chicago_draw_win95_flag_pixbuf(16);
    GtkWidget *flag_img = gtk_image_new_from_pixbuf(flag_pb);
    g_object_unref(flag_pb);
    GtkWidget *start_lbl = gtk_label_new("Start");
    gtk_box_pack_start(GTK_BOX(start_hbox), flag_img, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(start_hbox), start_lbl, FALSE, FALSE, 1);
    gtk_container_add(GTK_CONTAINER(start_btn), start_hbox);
    g_signal_connect(start_btn, "clicked", G_CALLBACK(on_start_btn_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(root_hbox), start_btn, FALSE, FALSE, 1);
    g_tb.start_btn = start_btn;

    /* Vertical 3D groove separator */
    gtk_box_pack_start(GTK_BOX(root_hbox), gtk_separator_new(GTK_ORIENTATION_VERTICAL), FALSE, FALSE, 2);

    /* 2. Running Application Taskbar Box */
    GtkWidget *task_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_box_set_homogeneous(GTK_BOX(task_box), FALSE);
    gtk_box_pack_start(GTK_BOX(root_hbox), task_box, TRUE, TRUE, 0);
    g_tb.task_box = task_box;

    /* 3. System Tray & Clock Sunken Well */
    GtkWidget *tray_well = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_style_context_add_class(gtk_widget_get_style_context(tray_well), "win95-well");

    /* Audio icon */
    GtkWidget *vol_btn = gtk_button_new();
    gtk_style_context_add_class(gtk_widget_get_style_context(vol_btn), "tray-btn");
    gtk_widget_add_events(vol_btn, GDK_SCROLL_MASK);
    GdkPixbuf *vol_pb = chicago_load_icon("audio-volume-high", 16);
    GtkWidget *vol_img = gtk_image_new_from_pixbuf(vol_pb);
    if (vol_pb) g_object_unref(vol_pb);
    gtk_container_add(GTK_CONTAINER(vol_btn), vol_img);
    gtk_widget_set_tooltip_text(vol_btn, "Volume Control (Click for slider, Scroll to adjust)");
    g_signal_connect(vol_btn, "event", G_CALLBACK(on_vol_btn_event), NULL);
    gtk_box_pack_start(GTK_BOX(tray_well), vol_btn, FALSE, FALSE, 0);
    g_tb.vol_btn = vol_btn;
    g_tb.vol_img = vol_img;

    /* Network icon */
    GtkWidget *net_btn = gtk_button_new();
    gtk_style_context_add_class(gtk_widget_get_style_context(net_btn), "tray-btn");
    GdkPixbuf *net_pb = chicago_load_icon("network-transmit-receive", 16);
    GtkWidget *net_img = gtk_image_new_from_pixbuf(net_pb);
    if (net_pb) g_object_unref(net_pb);
    gtk_container_add(GTK_CONTAINER(net_btn), net_img);
    gtk_widget_set_tooltip_text(net_btn, "Network Connections");
    g_signal_connect(net_btn, "clicked", G_CALLBACK(on_net_btn_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(tray_well), net_btn, FALSE, FALSE, 0);
    g_tb.net_btn = net_btn;

    /* Battery / Power icon */
    GtkWidget *bat_btn = gtk_button_new();
    gtk_style_context_add_class(gtk_widget_get_style_context(bat_btn), "tray-btn");
    GdkPixbuf *bat_pb = chicago_load_icon("battery-full-charged", 16);
    GtkWidget *bat_img = gtk_image_new_from_pixbuf(bat_pb);
    if (bat_pb) g_object_unref(bat_pb);
    gtk_container_add(GTK_CONTAINER(bat_btn), bat_img);
    gtk_widget_set_tooltip_text(bat_btn, "Power Meter (93% - AC Power)");
    g_signal_connect(bat_btn, "clicked", G_CALLBACK(on_bat_btn_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(tray_well), bat_btn, FALSE, FALSE, 0);
    g_tb.bat_btn = bat_btn;

    /* Clock */
    GtkWidget *clock_eb = gtk_event_box_new();
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(clock_eb), FALSE);
    GtkWidget *clock_lbl = gtk_label_new("12:00 PM");
    gtk_container_add(GTK_CONTAINER(clock_eb), clock_lbl);
    g_signal_connect(clock_eb, "button-press-event", G_CALLBACK(on_clock_btn_press), NULL);
    gtk_box_pack_start(GTK_BOX(tray_well), clock_eb, FALSE, FALSE, 4);
    g_tb.clock_label = clock_lbl;

    gtk_box_pack_end(GTK_BOX(root_hbox), tray_well, FALSE, FALSE, 2);

    gtk_container_add(GTK_CONTAINER(win), root_hbox);
    gtk_widget_show_all(win);
    g_tb.bar_window = win;

    update_clock_display();
}

int main(int argc, char **argv) {
    gtk_init(&argc, &argv);
    chicago_apply_theme();

    create_taskbar_window();

    /* Connect to Wayland registry for zwlr_foreign_toplevel_manager_v1 */
    GdkDisplay *gdk_disp = gdk_display_get_default();
    if (GDK_IS_WAYLAND_DISPLAY(gdk_disp)) {
        g_tb.wl_display = gdk_wayland_display_get_wl_display(gdk_disp);
        if (g_tb.wl_display) {
            g_tb.wl_registry = wl_display_get_registry(g_tb.wl_display);
            wl_registry_add_listener(g_tb.wl_registry, &registry_listener, NULL);
            wl_display_roundtrip(g_tb.wl_display);
        }
    }

    /* Register SIGUSR1 (Start menu toggle) and SIGUSR2 (Show Desktop toggle) */
    g_unix_signal_add(SIGUSR1, on_sigusr1, NULL);
    g_unix_signal_add(SIGUSR2, on_sigusr2, NULL);

    gtk_main();
    return 0;
}
