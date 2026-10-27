#ifndef CHICAGO_COMMON_H
#define CHICAGO_COMMON_H

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gtk-layer-shell/gtk-layer-shell.h>
#include <cairo.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

/*
 * Authentic Windows 95 4-Color 3D Bevel & Pixel-Crisp Helvetica 8pt CSS
 * Preserves inset box-shadow bevels (#ffffff, #dfdfdf, #808080, #000000).
 */
static const char *CHICAGO95_CORE_CSS =
    "* {\n"
    "  border-radius: 0px;\n"
    "  text-shadow: none;\n"
    "  -gtk-icon-shadow: none;\n"
    "  font-family: 'Helvetica', 'Microsoft Sans Serif', 'Liberation Sans', Sans;\n"
    "  font-size: 8pt;\n"
    "}\n"
    "window.chicago-taskbar {\n"
    "  background-color: #c0c0c0;\n"
    "  color: #000000;\n"
    "  border-top: 1px solid #ffffff;\n"
    "  box-shadow: inset 0px 1px 0px #dfdfdf;\n"
    "}\n"
    ".win95-panel-top-bevel {\n"
    "  background-color: #c0c0c0;\n"
    "  border-top: 1px solid #dfdfdf;\n"
    "}\n"
    ".win95-window, window.win95-dialog, dialog {\n"
    "  background-color: #c0c0c0;\n"
    "  color: #000000;\n"
    "}\n"
    ".win95-outset {\n"
    "  background-color: #c0c0c0;\n"
    "  border-top: 1px solid #ffffff;\n"
    "  border-left: 1px solid #ffffff;\n"
    "  border-right: 1px solid #000000;\n"
    "  border-bottom: 1px solid #000000;\n"
    "  box-shadow: inset -1px -1px 0px #808080, inset 1px 1px 0px #dfdfdf;\n"
    "}\n"
    ".win95-inset, entry, scrolledwindow {\n"
    "  background-color: #ffffff;\n"
    "  color: #000000;\n"
    "  border-top: 1px solid #808080;\n"
    "  border-left: 1px solid #808080;\n"
    "  border-right: 1px solid #ffffff;\n"
    "  border-bottom: 1px solid #ffffff;\n"
    "  box-shadow: inset 1px 1px 0px #000000, inset -1px -1px 0px #dfdfdf;\n"
    "}\n"
    "treeview, textview, iconview, flowbox, viewport, flowboxchild {\n"
    "  background-color: #ffffff;\n"
    "  color: #000000;\n"
    "  border: none;\n"
    "  box-shadow: none;\n"
    "  outline: none;\n"
    "}\n"
    "flowbox {\n"
    "  padding: 10px;\n"
    "}\n"
    "treeview row:selected, iconview:selected {\n"
    "  background-color: #000080;\n"
    "  color: #ffffff;\n"
    "}\n"
    "button.cp-icon-btn {\n"
    "  background-color: #ffffff;\n"
    "  background-image: none;\n"
    "  border: 1px solid transparent;\n"
    "  box-shadow: none;\n"
    "  padding: 6px;\n"
    "}\n"
    "button.cp-icon-btn:hover, button.cp-icon-btn:focus {\n"
    "  background-color: #ffffff;\n"
    "  border: 1px dotted #000080;\n"
    "  box-shadow: none;\n"
    "}\n"
    "button.cp-icon-btn:hover label, button.cp-icon-btn:focus label {\n"
    "  background-color: #000080;\n"
    "  color: #ffffff;\n"
    "}\n"
    ".win95-well {\n"
    "  background-color: #c0c0c0;\n"
    "  color: #000000;\n"
    "  border-top: 1px solid #808080;\n"
    "  border-left: 1px solid #808080;\n"
    "  border-right: 1px solid #ffffff;\n"
    "  border-bottom: 1px solid #ffffff;\n"
    "  box-shadow: none;\n"
    "  padding: 1px 4px;\n"
    "}\n"
    "button, .win95-btn {\n"
    "  background-color: #c0c0c0;\n"
    "  background-image: none;\n"
    "  color: #000000;\n"
    "  border-top: 1px solid #ffffff;\n"
    "  border-left: 1px solid #ffffff;\n"
    "  border-right: 1px solid #000000;\n"
    "  border-bottom: 1px solid #000000;\n"
    "  box-shadow: inset -1px -1px 0px #808080, inset 1px 1px 0px #dfdfdf;\n"
    "  padding: 2px 8px;\n"
    "  min-height: 20px;\n"
    "  min-width: 16px;\n"
    "}\n"
    "button:hover, .win95-btn:hover {\n"
    "  background-color: #c0c0c0;\n"
    "  background-image: none;\n"
    "  color: #000000;\n"
    "}\n"
    "button:active, button:checked, .win95-btn-active {\n"
    "  background-color: #dfdfdf;\n"
    "  background-image: linear-gradient(45deg, #c0c0c0 25%, transparent 25%, transparent 75%, #c0c0c0 75%, #c0c0c0), linear-gradient(45deg, #c0c0c0 25%, #ffffff 25%, #ffffff 75%, #c0c0c0 75%, #c0c0c0);\n"
    "  background-size: 2px 2px;\n"
    "  background-position: 0 0, 1px 1px;\n"
    "  color: #000000;\n"
    "  border-top: 1px solid #000000;\n"
    "  border-left: 1px solid #000000;\n"
    "  border-right: 1px solid #ffffff;\n"
    "  border-bottom: 1px solid #ffffff;\n"
    "  box-shadow: inset 1px 1px 0px #808080, inset -1px -1px 0px #dfdfdf;\n"
    "}\n"
    "button.start-btn {\n"
    "  font-weight: bold;\n"
    "  padding: 1px 5px;\n"
    "  margin: 2px 2px 2px 2px;\n"
    "}\n"
    "button.quick-btn {\n"
    "  border: 1px solid transparent;\n"
    "  box-shadow: none;\n"
    "  padding: 1px 2px;\n"
    "  margin: 2px 0px;\n"
    "  min-width: 20px;\n"
    "  min-height: 20px;\n"
    "}\n"
    "button.quick-btn:hover {\n"
    "  border-top: 1px solid #ffffff;\n"
    "  border-left: 1px solid #ffffff;\n"
    "  border-right: 1px solid #808080;\n"
    "  border-bottom: 1px solid #808080;\n"
    "}\n"
    "button.quick-btn:active {\n"
    "  border-top: 1px solid #808080;\n"
    "  border-left: 1px solid #808080;\n"
    "  border-right: 1px solid #ffffff;\n"
    "  border-bottom: 1px solid #ffffff;\n"
    "}\n"
    "button.task-btn {\n"
    "  padding: 1px 4px;\n"
    "  margin: 2px 1px;\n"
    "}\n"
    "button.task-btn.active-task {\n"
    "  font-weight: bold;\n"
    "  background-color: #dfdfdf;\n"
    "  background-image: linear-gradient(45deg, #c0c0c0 25%, transparent 25%, transparent 75%, #c0c0c0 75%, #c0c0c0), linear-gradient(45deg, #c0c0c0 25%, #ffffff 25%, #ffffff 75%, #c0c0c0 75%, #c0c0c0);\n"
    "  background-size: 2px 2px;\n"
    "  background-position: 0 0, 1px 1px;\n"
    "  border-top: 1px solid #000000;\n"
    "  border-left: 1px solid #000000;\n"
    "  border-right: 1px solid #ffffff;\n"
    "  border-bottom: 1px solid #ffffff;\n"
    "  box-shadow: inset 1px 1px 0px #808080, inset -1px -1px 0px #dfdfdf;\n"
    "}\n"
    "button.tray-btn {\n"
    "  border: none;\n"
    "  box-shadow: none;\n"
    "  background: transparent;\n"
    "  padding: 0px 2px;\n"
    "  min-height: 16px;\n"
    "  min-width: 16px;\n"
    "}\n"
    "menu, .win95-menu {\n"
    "  background-color: #c0c0c0;\n"
    "  color: #000000;\n"
    "  border-top: 1px solid #ffffff;\n"
    "  border-left: 1px solid #ffffff;\n"
    "  border-right: 1px solid #000000;\n"
    "  border-bottom: 1px solid #000000;\n"
    "  box-shadow: inset -1px -1px 0px #808080, inset 1px 1px 0px #dfdfdf;\n"
    "  padding: 2px;\n"
    "}\n"
    "menubar {\n"
    "  background-color: #c0c0c0;\n"
    "  color: #000000;\n"
    "  border-bottom: 1px solid #808080;\n"
    "  padding: 1px 2px;\n"
    "}\n"
    "menuitem {\n"
    "  background-color: #c0c0c0;\n"
    "  color: #000000;\n"
    "  padding: 3px 8px;\n"
    "}\n"
    "menuitem:hover, menuitem:selected, .win95-menuitem:hover, .win95-menuitem.active-row {\n"
    "  background-color: #000080;\n"
    "  color: #ffffff;\n"
    "}\n"
    "menuitem:hover label, menuitem:selected label, .win95-menuitem:hover label, .win95-menuitem.active-row label {\n"
    "  color: #ffffff;\n"
    "}\n"
    "button.win95-menuitem {\n"
    "  border: none;\n"
    "  box-shadow: none;\n"
    "  background-color: #c0c0c0;\n"
    "  background-image: none;\n"
    "  color: #000000;\n"
    "  padding: 4px 8px;\n"
    "}\n"
    "button.win95-menuitem:hover, button.win95-menuitem:focus, button.win95-menuitem:active, button.win95-menuitem.active-row {\n"
    "  border: none;\n"
    "  box-shadow: none;\n"
    "  background-color: #000080;\n"
    "  background-image: none;\n"
    "  color: #ffffff;\n"
    "}\n"
    "button.win95-menuitem:hover label, button.win95-menuitem:focus label, button.win95-menuitem:active label, button.win95-menuitem.active-row label {\n"
    "  color: #ffffff;\n"
    "}\n"
    "separator {\n"
    "  background-color: #808080;\n"
    "  border-bottom: 1px solid #ffffff;\n"
    "  min-height: 1px;\n"
    "  margin: 2px 1px;\n"
    "}\n"
    "notebook header {\n"
    "  background-color: #c0c0c0;\n"
    "}\n"
    "notebook tab {\n"
    "  background-color: #c0c0c0;\n"
    "  color: #000000;\n"
    "  border-top: 1px solid #ffffff;\n"
    "  border-left: 1px solid #ffffff;\n"
    "  border-right: 1px solid #000000;\n"
    "  box-shadow: inset -1px 0px 0px #808080, inset 1px 1px 0px #dfdfdf;\n"
    "  padding: 3px 8px;\n"
    "}\n"
    "notebook tab:checked {\n"
    "  background-color: #c0c0c0;\n"
    "  font-weight: bold;\n"
    "}\n"
    "notebook stack {\n"
    "  background-color: #c0c0c0;\n"
    "  border-top: 1px solid #ffffff;\n"
    "  border-left: 1px solid #ffffff;\n"
    "  border-right: 1px solid #000000;\n"
    "  border-bottom: 1px solid #000000;\n"
    "  box-shadow: inset -1px -1px 0px #808080, inset 1px 1px 0px #dfdfdf;\n"
    "  padding: 8px;\n"
    "}\n"
    "frame > border {\n"
    "  border-top: 1px solid #808080;\n"
    "  border-left: 1px solid #808080;\n"
    "  border-right: 1px solid #ffffff;\n"
    "  border-bottom: 1px solid #ffffff;\n"
    "  box-shadow: inset 1px 1px 0px #ffffff, inset -1px -1px 0px #808080;\n"
    "}\n"
    "scale trough {\n"
    "  background-color: #808080;\n"
    "  border-top: 1px solid #000000;\n"
    "  border-left: 1px solid #000000;\n"
    "  border-right: 1px solid #ffffff;\n"
    "  border-bottom: 1px solid #ffffff;\n"
    "  min-height: 4px;\n"
    "  min-width: 4px;\n"
    "}\n"
    "scale slider {\n"
    "  background-color: #c0c0c0;\n"
    "  border-top: 1px solid #ffffff;\n"
    "  border-left: 1px solid #ffffff;\n"
    "  border-right: 1px solid #000000;\n"
    "  border-bottom: 1px solid #000000;\n"
    "  box-shadow: inset -1px -1px 0px #808080, inset 1px 1px 0px #dfdfdf;\n"
    "  min-width: 11px;\n"
    "  min-height: 18px;\n"
    "}\n"
    "progressbar trough {\n"
    "  background-color: #c0c0c0;\n"
    "  border-top: 1px solid #808080;\n"
    "  border-left: 1px solid #808080;\n"
    "  border-right: 1px solid #ffffff;\n"
    "  border-bottom: 1px solid #ffffff;\n"
    "}\n"
    "progressbar progress {\n"
    "  background-color: #000080;\n"
    "}\n"
    "label {\n"
    "  color: #000000;\n"
    "}\n";

static inline void chicago_apply_theme(void) {
    GtkSettings *settings = gtk_settings_get_default();
    if (settings) {
        g_object_set(settings,
                     "gtk-theme-name", "Chicago95",
                     "gtk-icon-theme-name", "Chicago95",
                     "gtk-cursor-theme-name", "Chicago95_Cursor_Black",
                     "gtk-application-prefer-dark-theme", FALSE,
                     "gtk-font-name", "Helvetica 8",
                     "gtk-enable-animations", FALSE,
                     NULL);
    }

    GtkIconTheme *icon_theme = gtk_icon_theme_get_default();
    if (icon_theme) {
        char user_icons[512], fp_user_icons[512];
        snprintf(user_icons, sizeof(user_icons), "%s/.local/share/icons", g_get_home_dir());
        snprintf(fp_user_icons, sizeof(fp_user_icons), "%s/.local/share/flatpak/exports/share/icons", g_get_home_dir());
        gtk_icon_theme_prepend_search_path(icon_theme, user_icons);
        gtk_icon_theme_append_search_path(icon_theme, fp_user_icons);
        gtk_icon_theme_append_search_path(icon_theme, "/var/lib/flatpak/exports/share/icons");
    }

    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, CHICAGO95_CORE_CSS, -1, NULL);
    GdkScreen *screen = gdk_screen_get_default();
    if (screen) {
        gtk_style_context_add_provider_for_screen(
            screen,
            GTK_STYLE_PROVIDER(provider),
            GTK_STYLE_PROVIDER_PRIORITY_USER + 200);
    }
    g_object_unref(provider);
}

static inline void chicago_draw_bevel(cairo_t *cr, double x, double y, double w, double h, gboolean raised) {
    /* Outer top/left */
    if (raised) cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    else        cairo_set_source_rgb(cr, 0.502, 0.502, 0.502);
    cairo_rectangle(cr, x, y, w - 1, 1);
    cairo_rectangle(cr, x, y, 1, h - 1);
    cairo_fill(cr);

    /* Outer bottom/right */
    if (raised) cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    else        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_rectangle(cr, x, y + h - 1, w, 1);
    cairo_rectangle(cr, x + w - 1, y, 1, h);
    cairo_fill(cr);

    /* Inner top/left */
    if (raised) cairo_set_source_rgb(cr, 0.875, 0.875, 0.875);
    else        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_rectangle(cr, x + 1, y + 1, w - 3, 1);
    cairo_rectangle(cr, x + 1, y + 1, 1, h - 3);
    cairo_fill(cr);

    /* Inner bottom/right */
    if (raised) cairo_set_source_rgb(cr, 0.502, 0.502, 0.502);
    else        cairo_set_source_rgb(cr, 0.875, 0.875, 0.875);
    cairo_rectangle(cr, x + 1, y + h - 2, w - 2, 1);
    cairo_rectangle(cr, x + w - 2, y + 1, 1, h - 2);
    cairo_fill(cr);
}

static inline GdkPixbuf *chicago_draw_win95_flag_pixbuf(int size) {
    char path[512];
    const char *home = g_get_home_dir();
    if (size <= 18) {
        snprintf(path, sizeof(path), "%s/.local/share/themes/Chicago95/misc/windows_16x16px.png", home);
        if (g_file_test(path, G_FILE_TEST_EXISTS)) {
            GdkPixbuf *pb = gdk_pixbuf_new_from_file(path, NULL);
            if (pb) return pb;
        }
    } else {
        snprintf(path, sizeof(path), "%s/.local/share/themes/Chicago95/misc/windows_32x32px.png", home);
        if (g_file_test(path, G_FILE_TEST_EXISTS)) {
            GdkPixbuf *pb = gdk_pixbuf_new_from_file_at_scale(path, size, size, TRUE, NULL);
            if (pb) return pb;
        }
    }

    cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size, size);
    cairo_t *cr = cairo_create(surf);

    int half = size / 2 - 1;
    cairo_set_source_rgb(cr, 1.0, 0.15, 0.0);
    cairo_rectangle(cr, 1, 1, half - 1, half - 1);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, 0.0, 0.65, 0.15);
    cairo_rectangle(cr, half + 1, 1, half - 1, half - 1);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, 0.0, 0.25, 0.85);
    cairo_rectangle(cr, 1, half + 1, half - 1, half - 1);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, 1.0, 0.85, 0.0);
    cairo_rectangle(cr, half + 1, half + 1, half - 1, half - 1);
    cairo_fill(cr);

    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_set_line_width(cr, 1.0);
    cairo_rectangle(cr, 0.5, 0.5, size - 2, size - 2);
    cairo_stroke(cr);

    cairo_destroy(cr);
    GdkPixbuf *pb = gdk_pixbuf_get_from_surface(surf, 0, 0, size, size);
    cairo_surface_destroy(surf);
    return pb;
}

static inline GdkPixbuf *chicago_load_icon(const char *icon_name, int size) {
    GdkPixbuf *pb = NULL;
    const char *home = g_get_home_dir();

    if (icon_name && *icon_name) {
        if (g_path_is_absolute(icon_name) && g_file_test(icon_name, G_FILE_TEST_EXISTS)) {
            pb = gdk_pixbuf_new_from_file_at_scale(icon_name, size, size, TRUE, NULL);
            if (pb) return pb;
        }

        /* Deterministic direct lookup in ~/.local/share/icons/Chicago95 first */
        const char *cats[] = { "apps", "places", "devices", "actions", "status", "categories", "mimetypes", NULL };
        int try_sizes[] = { size, 32, 24, 22, 16, 48, 0 };
        for (int s = 0; try_sizes[s] != 0; s++) {
            for (int c = 0; cats[c] != NULL; c++) {
                char direct_path[512];
                snprintf(direct_path, sizeof(direct_path),
                         "%s/.local/share/icons/Chicago95/%s/%d/%s.png",
                         home, cats[c], try_sizes[s], icon_name);
                if (g_file_test(direct_path, G_FILE_TEST_EXISTS)) {
                    if (try_sizes[s] == size) {
                        pb = gdk_pixbuf_new_from_file(direct_path, NULL);
                    } else {
                        pb = gdk_pixbuf_new_from_file_at_scale(direct_path, size, size, TRUE, NULL);
                    }
                    if (pb) return pb;
                }
            }
        }

        /* Flatpak export icon directories (system & user) */
        const char *fp_roots[2];
        char fp_user_root[512];
        snprintf(fp_user_root, sizeof(fp_user_root), "%s/.local/share/flatpak/exports/share/icons/hicolor", home);
        fp_roots[0] = "/var/lib/flatpak/exports/share/icons/hicolor";
        fp_roots[1] = fp_user_root;
        const char *fp_sizes[] = { "32x32", "48x48", "64x64", "128x128", "256x256", "512x512", "scalable", NULL };
        const char *fp_exts[] = { "png", "svg", NULL };
        for (int r = 0; r < 2; r++) {
            for (int fs = 0; fp_sizes[fs]; fs++) {
                for (int fe = 0; fp_exts[fe]; fe++) {
                    char fp_path[768];
                    snprintf(fp_path, sizeof(fp_path), "%s/%s/apps/%s.%s",
                             fp_roots[r], fp_sizes[fs], icon_name, fp_exts[fe]);
                    if (g_file_test(fp_path, G_FILE_TEST_EXISTS)) {
                        pb = gdk_pixbuf_new_from_file_at_scale(fp_path, size, size, TRUE, NULL);
                        if (pb) return pb;
                    }
                }
            }
        }

        GtkIconTheme *theme = gtk_icon_theme_get_default();
        if (theme) {
            pb = gtk_icon_theme_load_icon(theme, icon_name, size, GTK_ICON_LOOKUP_FORCE_SIZE, NULL);
            if (pb) return pb;
        }
    }

    /* Fallbacks from Chicago95 */
    const char *fallbacks[] = {
        "application-x-executable",
        "utilities-terminal",
        "folder",
        "system-run",
        NULL
    };
    for (int i = 0; fallbacks[i]; i++) {
        char direct_path[512];
        snprintf(direct_path, sizeof(direct_path),
                 "%s/.local/share/icons/Chicago95/apps/%d/%s.png",
                 home, (size <= 16 ? 16 : 32), fallbacks[i]);
        if (g_file_test(direct_path, G_FILE_TEST_EXISTS)) {
            pb = gdk_pixbuf_new_from_file_at_scale(direct_path, size, size, TRUE, NULL);
            if (pb) return pb;
        }
    }

    return chicago_draw_win95_flag_pixbuf(size);
}

static inline void chicago_spawn_command(const char *cmd) {
    if (!cmd || !*cmd) return;
    GError *err = NULL;
    char *argv[] = { "/bin/sh", "-c", (char *)cmd, NULL };
    if (!g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &err)) {
        if (err) {
            g_warning("Failed to spawn '%s': %s", cmd, err->message);
            g_error_free(err);
        }
    }
}

/* Launch command on the Dedicated NVIDIA GPU (PRIME Render Offload) */
static inline void chicago_launch_with_dgpu(const char *cmd) {
    if (!cmd || !*cmd) return;
    char wrapped[2048];
    snprintf(wrapped, sizeof(wrapped),
             "env __NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia "
             "__VK_LAYER_NV_optimus=NVIDIA_only VK_LOADER_DRIVERS_SELECT=*nvidia* "
             "DRI_PRIME=1 %s",
             cmd);
    chicago_spawn_command(wrapped);
}

/* Launch command as Administrator (pkexec or terminal sudo) */
static inline void chicago_launch_as_admin(const char *cmd) {
    if (!cmd || !*cmd) return;
    char wrapped[2048];
    snprintf(wrapped, sizeof(wrapped),
             "if command -v pkexec >/dev/null 2>&1; then "
             "pkexec env DISPLAY=\"$DISPLAY\" WAYLAND_DISPLAY=\"$WAYLAND_DISPLAY\" XDG_RUNTIME_DIR=\"$XDG_RUNTIME_DIR\" %s; "
             "else kitty -c \"%s/.config/chicago-wayland/kitty/kitty.conf\" --title \"Administrator: %s\" -e sudo %s; fi",
             cmd, g_get_home_dir(), cmd, cmd);
    chicago_spawn_command(wrapped);
}

/* Cleanly strip XDG %U/%F and Flatpak @@u / @@ tokens without truncating arguments */
static inline void chicago_clean_exec_cmd(const char *raw, char *out, size_t out_len) {
    if (!raw || out_len == 0) {
        if (out_len > 0) out[0] = '\0';
        return;
    }
    char **tokens = g_strsplit(raw, " ", -1);
    GString *str = g_string_new("");
    for (int i = 0; tokens && tokens[i]; i++) {
        const char *t = tokens[i];
        if (!*t) continue;
        if (t[0] == '%' && t[1] != '\0') continue;
        if (g_strcmp0(t, "@@u") == 0 || g_strcmp0(t, "@@f") == 0 || g_strcmp0(t, "@@") == 0) continue;
        if (g_strcmp0(t, "--file-forwarding") == 0) continue;
        if (str->len > 0) g_string_append_c(str, ' ');
        g_string_append(str, t);
    }
    g_strfreev(tokens);
    g_strlcpy(out, str->str, out_len);
    g_string_free(str, TRUE);
}

static inline void chicago_config_get(const char *key, char *out_val, size_t out_len, const char *default_val) {
    g_strlcpy(out_val, default_val ? default_val : "", out_len);
    char path[512];
    snprintf(path, sizeof(path), "%s/.config/chicago-wayland/desktop.conf", g_get_home_dir());
    FILE *fp = fopen(path, "r");
    if (!fp) return;
    char line[512];
    size_t klen = strlen(key);
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, key, klen) == 0 && line[klen] == '=') {
            char *val = line + klen + 1;
            val[strcspn(val, "\r\n")] = '\0';
            g_strlcpy(out_val, val, out_len);
            break;
        }
    }
    fclose(fp);
}

static inline void chicago_config_set(const char *key, const char *val) {
    char dir[256], path[512];
    snprintf(dir, sizeof(dir), "%s/.config/chicago-wayland", g_get_home_dir());
    g_mkdir_with_parents(dir, 0755);
    snprintf(path, sizeof(path), "%s/desktop.conf", dir);

    GHashTable *kv = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    FILE *fp = fopen(path, "r");
    if (fp) {
        char line[512];
        while (fgets(line, sizeof(line), fp)) {
            line[strcspn(line, "\r\n")] = '\0';
            char *eq = strchr(line, '=');
            if (eq) {
                *eq = '\0';
                g_hash_table_insert(kv, g_strdup(line), g_strdup(eq + 1));
            }
        }
        fclose(fp);
    }
    g_hash_table_insert(kv, g_strdup(key), g_strdup(val ? val : ""));

    fp = fopen(path, "w");
    if (fp) {
        GHashTableIter iter;
        gpointer k, v;
        g_hash_table_iter_init(&iter, kv);
        while (g_hash_table_iter_next(&iter, &k, &v)) {
            fprintf(fp, "%s=%s\n", (const char *)k, (const char *)v);
        }
        fclose(fp);
    }
    g_hash_table_destroy(kv);
}

/* =========================================================================
 * Unified Application Discovery (XDG + System/User Flatpak + Snap)
 * ========================================================================= */

typedef struct {
    char *name;
    char *exec;
    char *icon;
    char *category;
    char *desktop_path;
    gboolean prefers_dgpu;
} ChicagoAppEntry;

static inline void chicago_app_entry_free(gpointer data) {
    ChicagoAppEntry *e = (ChicagoAppEntry *)data;
    if (!e) return;
    g_free(e->name);
    g_free(e->exec);
    g_free(e->icon);
    g_free(e->category);
    g_free(e->desktop_path);
    g_free(e);
}

static inline gint chicago_app_entry_compare(gconstpointer a, gconstpointer b) {
    const ChicagoAppEntry *ea = (const ChicagoAppEntry *)a;
    const ChicagoAppEntry *eb = (const ChicagoAppEntry *)b;
    return g_ascii_strcasecmp(ea->name ? ea->name : "", eb->name ? eb->name : "");
}

static inline GList *chicago_scan_all_desktop_apps(void) {
    const char *home = g_get_home_dir();
    char user_apps[512], user_fp_apps[512];
    snprintf(user_apps, sizeof(user_apps), "%s/.local/share/applications", home);
    snprintf(user_fp_apps, sizeof(user_fp_apps), "%s/.local/share/flatpak/exports/share/applications", home);

    const char *all_dirs[] = {
        user_apps,
        user_fp_apps,
        "/var/lib/flatpak/exports/share/applications",
        "/usr/local/share/applications",
        "/usr/share/applications",
        "/var/lib/snapd/desktop/applications",
        NULL
    };

    GHashTable *seen = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    GList *list = NULL;

    for (int d = 0; all_dirs[d] != NULL; d++) {
        GDir *dir = g_dir_open(all_dirs[d], 0, NULL);
        if (!dir) continue;
        const char *fname;
        while ((fname = g_dir_read_name(dir)) != NULL) {
            if (!g_str_has_suffix(fname, ".desktop")) continue;
            char full[768];
            snprintf(full, sizeof(full), "%s/%s", all_dirs[d], fname);
            GKeyFile *kf = g_key_file_new();
            if (g_key_file_load_from_file(kf, full, G_KEY_FILE_NONE, NULL)) {
                gboolean no_display = g_key_file_get_boolean(kf, "Desktop Entry", "NoDisplay", NULL);
                gboolean hidden = g_key_file_get_boolean(kf, "Desktop Entry", "Hidden", NULL);
                char *name = g_key_file_get_locale_string(kf, "Desktop Entry", "Name", NULL, NULL);
                char *raw_exec = g_key_file_get_string(kf, "Desktop Entry", "Exec", NULL);
                char *icon = g_key_file_get_string(kf, "Desktop Entry", "Icon", NULL);
                char *cats = g_key_file_get_string(kf, "Desktop Entry", "Categories", NULL);
                gboolean dgpu = g_key_file_get_boolean(kf, "Desktop Entry", "PrefersNonDefaultGPU", NULL);

                if (!no_display && !hidden && name && raw_exec && !g_hash_table_contains(seen, name)) {
                    char clean_exec[1024];
                    chicago_clean_exec_cmd(raw_exec, clean_exec, sizeof(clean_exec));
                    if (clean_exec[0] != '\0') {
                        g_hash_table_add(seen, g_strdup(name));
                        ChicagoAppEntry *entry = g_malloc0(sizeof(ChicagoAppEntry));
                        entry->name = g_strdup(name);
                        entry->exec = g_strdup(clean_exec);
                        entry->icon = g_strdup(icon ? icon : "application-x-executable");
                        entry->desktop_path = g_strdup(full);
                        entry->prefers_dgpu = dgpu;

                        if (cats) {
                            if (strstr(cats, "Audio") || strstr(cats, "Video") || strstr(cats, "Music") || strstr(cats, "Player") || strstr(cats, "Multimedia"))
                                entry->category = g_strdup("Multimedia");
                            else if (strstr(cats, "Network") || strstr(cats, "WebBrowser") || strstr(cats, "InstantMessaging") || strstr(cats, "Chat"))
                                entry->category = g_strdup("Internet");
                            else if (strstr(cats, "Game"))
                                entry->category = g_strdup("Games");
                            else if (strstr(cats, "Development") || strstr(cats, "IDE"))
                                entry->category = g_strdup("Development");
                            else if (strstr(cats, "Graphics"))
                                entry->category = g_strdup("Graphics");
                            else if (strstr(cats, "Office"))
                                entry->category = g_strdup("Office");
                            else if (strstr(cats, "Settings") || strstr(cats, "DesktopSettings"))
                                entry->category = g_strdup("Settings");
                            else if (strstr(cats, "System") || strstr(cats, "Monitor"))
                                entry->category = g_strdup("System Tools");
                            else
                                entry->category = g_strdup("Accessories");
                        } else {
                            entry->category = g_strdup("Accessories");
                        }
                        list = g_list_prepend(list, entry);
                    }
                }
                g_free(name);
                g_free(raw_exec);
                g_free(icon);
                g_free(cats);
            }
            g_key_file_free(kf);
        }
        g_dir_close(dir);
    }
    g_hash_table_destroy(seen);
    return g_list_sort(list, chicago_app_entry_compare);
}

/* =========================================================================
 * Taskbar Pin Management (~/.config/chicago-wayland/pins.conf)
 * ========================================================================= */

typedef struct {
    char *name;
    char *icon;
    char *exec;
    char *desktop_path;
} ChicagoPinEntry;

static inline void chicago_pin_entry_free(gpointer data) {
    ChicagoPinEntry *p = (ChicagoPinEntry *)data;
    if (!p) return;
    g_free(p->name);
    g_free(p->icon);
    g_free(p->exec);
    g_free(p->desktop_path);
    g_free(p);
}

static inline void chicago_pins_ensure_default(void) {
    char dir[256], path[512];
    const char *home = g_get_home_dir();
    snprintf(dir, sizeof(dir), "%s/.config/chicago-wayland", home);
    g_mkdir_with_parents(dir, 0755);
    snprintf(path, sizeof(path), "%s/pins.conf", dir);
    if (g_file_test(path, G_FILE_TEST_EXISTS)) return;

    FILE *fp = fopen(path, "w");
    if (!fp) return;
    fprintf(fp, "Windows Explorer|system-file-manager|thunar|/usr/share/applications/thunar.desktop\n");
    fprintf(fp, "Find Programs|system-search|chicago-control-panel --find|\n");
    fprintf(fp, "MS-DOS Prompt|utilities-terminal|kitty -c \"%s/.config/chicago-wayland/kitty/kitty.conf\" --title \"MS-DOS Prompt\"|/usr/share/applications/kitty.desktop\n", home);
    fprintf(fp, "Control Panel|preferences-system|chicago-control-panel|\n");
    if (g_file_test("/var/lib/flatpak/exports/share/applications/com.spotify.Client.desktop", G_FILE_TEST_EXISTS)) {
        fprintf(fp, "Spotify|com.spotify.Client|/usr/bin/flatpak run --branch=stable --arch=x86_64 --command=spotify com.spotify.Client|/var/lib/flatpak/exports/share/applications/com.spotify.Client.desktop\n");
    }
    fclose(fp);
}

static inline GList *chicago_pins_load(void) {
    chicago_pins_ensure_default();
    char path[512];
    snprintf(path, sizeof(path), "%s/.config/chicago-wayland/pins.conf", g_get_home_dir());
    FILE *fp = fopen(path, "r");
    if (!fp) return NULL;

    GList *list = NULL;
    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (!line[0] || line[0] == '#') continue;
        char **parts = g_strsplit(line, "|", 4);
        if (parts && parts[0] && parts[1] && parts[2]) {
            ChicagoPinEntry *p = g_malloc0(sizeof(ChicagoPinEntry));
            p->name = g_strdup(parts[0]);
            p->icon = g_strdup(parts[1]);
            p->exec = g_strdup(parts[2]);
            p->desktop_path = g_strdup(parts[3] ? parts[3] : "");
            list = g_list_append(list, p);
        }
        g_strfreev(parts);
    }
    fclose(fp);
    return list;
}

static inline void chicago_pins_save(GList *pins) {
    char dir[256], path[512];
    snprintf(dir, sizeof(dir), "%s/.config/chicago-wayland", g_get_home_dir());
    g_mkdir_with_parents(dir, 0755);
    snprintf(path, sizeof(path), "%s/pins.conf", dir);
    FILE *fp = fopen(path, "w");
    if (!fp) return;
    for (GList *l = pins; l != NULL; l = l->next) {
        ChicagoPinEntry *p = (ChicagoPinEntry *)l->data;
        fprintf(fp, "%s|%s|%s|%s\n",
                p->name ? p->name : "App",
                p->icon ? p->icon : "application-x-executable",
                p->exec ? p->exec : "",
                p->desktop_path ? p->desktop_path : "");
    }
    fclose(fp);
}

static inline gboolean chicago_pins_is_pinned(const char *name, const char *exec) {
    GList *pins = chicago_pins_load();
    gboolean found = FALSE;
    for (GList *l = pins; l != NULL; l = l->next) {
        ChicagoPinEntry *p = (ChicagoPinEntry *)l->data;
        if ((name && p->name && g_ascii_strcasecmp(p->name, name) == 0) ||
            (exec && p->exec && strcmp(p->exec, exec) == 0)) {
            found = TRUE;
            break;
        }
    }
    g_list_free_full(pins, chicago_pin_entry_free);
    return found;
}

static inline void chicago_pins_add(const char *name, const char *icon, const char *exec, const char *desktop_path) {
    if (!name || !exec) return;
    if (chicago_pins_is_pinned(name, exec)) return;
    GList *pins = chicago_pins_load();
    ChicagoPinEntry *p = g_malloc0(sizeof(ChicagoPinEntry));
    p->name = g_strdup(name);
    p->icon = g_strdup(icon && *icon ? icon : "application-x-executable");
    p->exec = g_strdup(exec);
    p->desktop_path = g_strdup(desktop_path ? desktop_path : "");
    pins = g_list_append(pins, p);
    chicago_pins_save(pins);
    g_list_free_full(pins, chicago_pin_entry_free);
}

static inline void chicago_pins_remove(const char *name, const char *exec) {
    GList *pins = chicago_pins_load();
    GList *keep = NULL;
    for (GList *l = pins; l != NULL; l = l->next) {
        ChicagoPinEntry *p = (ChicagoPinEntry *)l->data;
        gboolean match = ((name && p->name && g_ascii_strcasecmp(p->name, name) == 0) ||
                          (exec && p->exec && strcmp(p->exec, exec) == 0));
        if (!match) {
            ChicagoPinEntry *copy = g_malloc0(sizeof(ChicagoPinEntry));
            copy->name = g_strdup(p->name);
            copy->icon = g_strdup(p->icon);
            copy->exec = g_strdup(p->exec);
            copy->desktop_path = g_strdup(p->desktop_path);
            keep = g_list_append(keep, copy);
        }
    }
    chicago_pins_save(keep);
    g_list_free_full(pins, chicago_pin_entry_free);
    g_list_free_full(keep, chicago_pin_entry_free);
}

static inline void chicago_create_desktop_shortcut(const char *name, const char *icon, const char *exec, gboolean use_dgpu) {
    if (!name || !exec) return;
    char desktop_dir[512], safe_name[128], path[768];
    snprintf(desktop_dir, sizeof(desktop_dir), "%s/Desktop", g_get_home_dir());
    g_mkdir_with_parents(desktop_dir, 0755);

    g_strlcpy(safe_name, name, sizeof(safe_name));
    for (char *p = safe_name; *p; p++) {
        if (*p == '/' || *p == '\\' || *p == ':') *p = '_';
    }
    snprintf(path, sizeof(path), "%s/%s.desktop", desktop_dir, safe_name);

    FILE *fp = fopen(path, "w");
    if (!fp) return;
    fprintf(fp, "[Desktop Entry]\nType=Application\nName=%s\nIcon=%s\n",
            name, icon && *icon ? icon : "application-x-executable");
    if (use_dgpu) {
        fprintf(fp, "Exec=env __NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia __VK_LAYER_NV_optimus=NVIDIA_only VK_LOADER_DRIVERS_SELECT=*nvidia* DRI_PRIME=1 %s\n", exec);
        fprintf(fp, "PrefersNonDefaultGPU=true\n");
    } else {
        fprintf(fp, "Exec=%s\n", exec);
    }
    fprintf(fp, "Terminal=false\n");
    fclose(fp);
    chmod(path, 0755);
}

/* =========================================================================
 * Windows 95 Application / Shortcut Context Menu & Properties Dialog
 * ========================================================================= */

typedef struct {
    char *name;
    char *icon;
    char *exec;
    char *desktop_path;
    GtkWidget *target_entry;
    GtkWidget *dgpu_chk;
    GtkWidget *win;
} ChicagoAppMenuCtx;

static inline void chicago_app_menu_ctx_free(ChicagoAppMenuCtx *ctx) {
    if (!ctx) return;
    g_free(ctx->name);
    g_free(ctx->icon);
    g_free(ctx->exec);
    g_free(ctx->desktop_path);
    g_free(ctx);
}

static inline void on_app_prop_launch_dgpu_clicked(GtkButton *btn, gpointer data) {
    (void)btn;
    ChicagoAppMenuCtx *ctx = (ChicagoAppMenuCtx *)data;
    const char *cmd = gtk_entry_get_text(GTK_ENTRY(ctx->target_entry));
    chicago_launch_with_dgpu(cmd);
}

static inline void on_app_prop_ok_clicked(GtkButton *btn, gpointer data) {
    (void)btn;
    ChicagoAppMenuCtx *ctx = (ChicagoAppMenuCtx *)data;
    const char *cmd = gtk_entry_get_text(GTK_ENTRY(ctx->target_entry));
    gboolean use_dgpu = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ctx->dgpu_chk));
    if (chicago_pins_is_pinned(ctx->name, ctx->exec)) {
        chicago_pins_remove(ctx->name, ctx->exec);
        if (use_dgpu && !strstr(cmd, "__NV_PRIME_RENDER_OFFLOAD")) {
            char dcmd[1536];
            snprintf(dcmd, sizeof(dcmd),
                     "env __NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia "
                     "__VK_LAYER_NV_optimus=NVIDIA_only VK_LOADER_DRIVERS_SELECT=*nvidia* DRI_PRIME=1 %s",
                     cmd);
            chicago_pins_add(ctx->name, ctx->icon, dcmd, ctx->desktop_path);
        } else {
            chicago_pins_add(ctx->name, ctx->icon, cmd, ctx->desktop_path);
        }
    }
    if (ctx->win) gtk_widget_destroy(ctx->win);
}

static inline void chicago_show_app_properties_dialog(const char *name, const char *icon, const char *exec, const char *desktop_path) {
    ChicagoAppMenuCtx *ctx = g_malloc0(sizeof(ChicagoAppMenuCtx));
    ctx->name = g_strdup(name ? name : "Application");
    ctx->icon = g_strdup(icon ? icon : "application-x-executable");
    ctx->exec = g_strdup(exec ? exec : "");
    ctx->desktop_path = g_strdup(desktop_path ? desktop_path : "");

    char title[128];
    snprintf(title, sizeof(title), "%s Properties", ctx->name);

    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    ctx->win = win;
    gtk_window_set_title(GTK_WINDOW(win), title);
    gtk_window_set_default_size(GTK_WINDOW(win), 380, 320);
    gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_CENTER);
    gtk_style_context_add_class(gtk_widget_get_style_context(win), "win95-window");
    g_signal_connect_swapped(win, "destroy", G_CALLBACK(chicago_app_menu_ctx_free), ctx);

    GtkWidget *root_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(root_vbox), 8);

    GtkWidget *nb = gtk_notebook_new();
    GtkWidget *page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(page), 10);

    GtkWidget *top_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GdkPixbuf *pb = chicago_load_icon(ctx->icon, 32);
    GtkWidget *img = gtk_image_new_from_pixbuf(pb);
    if (pb) g_object_unref(pb);
    GtkWidget *name_lbl = gtk_label_new(NULL);
    char markup[256];
    snprintf(markup, sizeof(markup), "<b>%s</b>", ctx->name);
    gtk_label_set_markup(GTK_LABEL(name_lbl), markup);
    gtk_box_pack_start(GTK_BOX(top_hbox), img, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(top_hbox), name_lbl, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(page), top_hbox, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(page), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 2);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);

    GtkWidget *t_lbl = gtk_label_new("Target:");
    gtk_label_set_xalign(GTK_LABEL(t_lbl), 0.0f);
    ctx->target_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(ctx->target_entry), ctx->exec);
    gtk_widget_set_hexpand(ctx->target_entry, TRUE);
    gtk_grid_attach(GTK_GRID(grid), t_lbl, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), ctx->target_entry, 1, 0, 1, 1);

    GtkWidget *p_lbl = gtk_label_new("Location:");
    gtk_label_set_xalign(GTK_LABEL(p_lbl), 0.0f);
    GtkWidget *p_val = gtk_label_new(ctx->desktop_path && *ctx->desktop_path ? ctx->desktop_path : "(System Command)");
    gtk_label_set_xalign(GTK_LABEL(p_val), 0.0f);
    gtk_label_set_ellipsize(GTK_LABEL(p_val), PANGO_ELLIPSIZE_MIDDLE);
    gtk_grid_attach(GTK_GRID(grid), p_lbl, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), p_val, 1, 1, 1, 1);

    gtk_box_pack_start(GTK_BOX(page), grid, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(page), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 2);

    ctx->dgpu_chk = gtk_check_button_new_with_label("Run on Dedicated GPU (NVIDIA Quadro T1000 Mobile)");
    if (strstr(ctx->exec, "__NV_PRIME_RENDER_OFFLOAD") != NULL) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ctx->dgpu_chk), TRUE);
    }
    gtk_box_pack_start(GTK_BOX(page), ctx->dgpu_chk, FALSE, FALSE, 0);

    GtkWidget *act_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *dgpu_btn = gtk_button_new_with_label("Launch Now with NVIDIA GPU");
    g_signal_connect(dgpu_btn, "clicked", G_CALLBACK(on_app_prop_launch_dgpu_clicked), ctx);
    gtk_box_pack_start(GTK_BOX(act_hbox), dgpu_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(page), act_hbox, FALSE, FALSE, 2);

    gtk_notebook_append_page(GTK_NOTEBOOK(nb), page, gtk_label_new("Shortcut"));
    gtk_box_pack_start(GTK_BOX(root_vbox), nb, TRUE, TRUE, 0);

    GtkWidget *btn_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *ok_btn = gtk_button_new_with_label("OK");
    GtkWidget *cancel_btn = gtk_button_new_with_label("Cancel");
    gtk_widget_set_size_request(ok_btn, 75, 23);
    gtk_widget_set_size_request(cancel_btn, 75, 23);
    g_signal_connect(ok_btn, "clicked", G_CALLBACK(on_app_prop_ok_clicked), ctx);
    g_signal_connect_swapped(cancel_btn, "clicked", G_CALLBACK(gtk_widget_destroy), win);
    gtk_box_pack_end(GTK_BOX(btn_hbox), cancel_btn, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(btn_hbox), ok_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(root_vbox), btn_hbox, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(win), root_vbox);
    gtk_widget_show_all(win);
}

/* =========================================================================
 * Native Wayland gtk-layer-shell OVERLAY Windows 95 Context Menu System
 * ========================================================================= */

static GtkWidget *g_chicago_ctx_win = NULL;

static inline void chicago_close_context_menu(void) {
    if (g_chicago_ctx_win) {
        GtkWidget *w = g_chicago_ctx_win;
        g_chicago_ctx_win = NULL;
        gtk_widget_destroy(w);
    }
}

static inline gboolean chicago_is_context_menu_visible(void) {
    return (g_chicago_ctx_win != NULL && gtk_widget_get_visible(g_chicago_ctx_win));
}

static inline void on_chicago_ctx_win_destroyed(GtkWidget *win, gpointer data) {
    (void)data;
    if (g_chicago_ctx_win == win) {
        g_chicago_ctx_win = NULL;
    }
}

static inline gboolean on_chicago_ctx_win_key_press(GtkWidget *win, GdkEventKey *ev, gpointer data) {
    (void)win; (void)data;
    if (ev->keyval == GDK_KEY_Escape) {
        chicago_close_context_menu();
        return TRUE;
    }
    return FALSE;
}

static inline void on_chicago_ctx_item_close_after(GtkWidget *btn, gpointer menu_win) {
    (void)btn;
    if (g_chicago_ctx_win && g_chicago_ctx_win == (GtkWidget *)menu_win) {
        chicago_close_context_menu();
    }
}

static inline GtkWidget *chicago_ctx_menu_begin(GdkMonitor *mon, int left_margin, int edge_margin, gboolean anchor_bottom, GtkWidget **out_vbox) {
    chicago_close_context_menu();

    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_chicago_ctx_win = win;
    gtk_window_set_title(GTK_WINDOW(win), "Context Menu");
    gtk_window_set_decorated(GTK_WINDOW(win), FALSE);
    gtk_window_set_resizable(GTK_WINDOW(win), FALSE);
    gtk_style_context_add_class(gtk_widget_get_style_context(win), "win95-outset");
    gtk_style_context_add_class(gtk_widget_get_style_context(win), "win95-menu");

    if (gtk_layer_is_supported()) {
        gtk_layer_init_for_window(GTK_WINDOW(win));
        gtk_layer_set_layer(GTK_WINDOW(win), GTK_LAYER_SHELL_LAYER_OVERLAY);
        if (mon) {
            gtk_layer_set_monitor(GTK_WINDOW(win), mon);
            g_object_set_data(G_OBJECT(win), "chicago-monitor", mon);
        }
        gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
        if (anchor_bottom) {
            gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
            gtk_layer_set_margin(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_BOTTOM, MAX(2, edge_margin));
        } else {
            gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
            gtk_layer_set_margin(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_TOP, MAX(2, edge_margin));
        }
        gtk_layer_set_margin(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_LEFT, MAX(2, left_margin));
        gtk_layer_set_keyboard_mode(GTK_WINDOW(win), GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);
        gtk_layer_set_namespace(GTK_WINDOW(win), "chicago-context-menu");
    }

    gtk_widget_add_events(win, GDK_KEY_PRESS_MASK);
    g_signal_connect(win, "destroy", G_CALLBACK(on_chicago_ctx_win_destroyed), NULL);
    g_signal_connect(win, "key-press-event", G_CALLBACK(on_chicago_ctx_win_key_press), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 3);
    gtk_widget_set_size_request(vbox, 215, -1);
    gtk_container_add(GTK_CONTAINER(win), vbox);

    if (out_vbox) *out_vbox = vbox;
    return win;
}

static inline GtkWidget *chicago_ctx_menu_add_item(GtkWidget *vbox, const char *label_text, const char *icon_name,
                                                   gboolean bold, GCallback cb, gpointer user_data) {
    GtkWidget *btn = gtk_button_new();
    gtk_style_context_add_class(gtk_widget_get_style_context(btn), "win95-menuitem");

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    if (icon_name && *icon_name) {
        GdkPixbuf *pb = chicago_load_icon(icon_name, 16);
        GtkWidget *img = gtk_image_new_from_pixbuf(pb);
        if (pb) g_object_unref(pb);
        gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 2);
    }

    GtkWidget *lbl = gtk_label_new(NULL);
    if (bold) {
        char *mk = g_markup_printf_escaped("<b>%s</b>", label_text ? label_text : "");
        gtk_label_set_markup(GTK_LABEL(lbl), mk);
        g_free(mk);
    } else {
        gtk_label_set_text(GTK_LABEL(lbl), label_text ? label_text : "");
    }
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
    gtk_box_pack_start(GTK_BOX(hbox), lbl, TRUE, TRUE, 2);

    gtk_container_add(GTK_CONTAINER(btn), hbox);
    if (cb) {
        g_signal_connect(btn, "clicked", cb, user_data);
    }
    g_signal_connect_after(btn, "clicked", G_CALLBACK(on_chicago_ctx_item_close_after), g_chicago_ctx_win);
    gtk_box_pack_start(GTK_BOX(vbox), btn, FALSE, FALSE, 0);
    return btn;
}

static inline void chicago_ctx_menu_add_separator(GtkWidget *vbox) {
    gtk_box_pack_start(GTK_BOX(vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 2);
}

static inline void chicago_ctx_menu_show(GtkWidget *win, GdkMonitor *mon, int req_left, int req_edge, gboolean anchor_bottom) {
    if (!win) return;
    gtk_widget_show_all(win);

    if (gtk_layer_is_supported() && gtk_layer_is_layer_window(GTK_WINDOW(win))) {
        GtkRequisition min_req = {0, 0}, nat_req = {220, 200};
        gtk_widget_get_preferred_size(win, &min_req, &nat_req);
        int menu_w = MAX(nat_req.width, 215);
        int menu_h = MAX(nat_req.height, 120);

        GdkRectangle geom = {0, 0, 1920, 1080};
        if (mon) {
            gdk_monitor_get_geometry(mon, &geom);
        }
        int clamped_left = CLAMP(req_left, 4, MAX(4, geom.width - menu_w - 6));
        int clamped_edge = CLAMP(req_edge, 4, MAX(4, geom.height - menu_h - 32));

        gtk_layer_set_margin(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_LEFT, clamped_left);
        if (anchor_bottom) {
            gtk_layer_set_margin(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_BOTTOM, clamped_edge);
        } else {
            gtk_layer_set_margin(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_TOP, clamped_edge);
        }
    }
}

static inline void chicago_resolve_event_layer_coords(GtkWidget *widget, GdkEventButton *event,
                                                      GdkMonitor **out_mon, int *out_left, int *out_edge, gboolean *out_anchor_bottom) {
    GtkWidget *toplevel = widget ? gtk_widget_get_toplevel(widget) : NULL;
    GdkMonitor *mon = NULL;
    if (toplevel) {
        mon = (GdkMonitor *)g_object_get_data(G_OBJECT(toplevel), "chicago-monitor");
    }
    GdkDisplay *dpy = gdk_display_get_default();
    if (!mon && dpy && event && event->window) {
        mon = gdk_display_get_monitor_at_window(dpy, event->window);
    }
    if (!mon && dpy && gdk_display_get_n_monitors(dpy) > 0) {
        mon = gdk_display_get_monitor(dpy, 0);
    }

    int wx = event ? (int)event->x : 100;
    int wy = event ? (int)event->y : 100;
    if (widget && toplevel && widget != toplevel && gtk_widget_is_toplevel(toplevel)) {
        gtk_widget_translate_coordinates(widget, toplevel, wx, wy, &wx, &wy);
    }

    gboolean anchor_bottom = FALSE;
    int left = wx;
    int edge = wy;

    if (toplevel && gtk_layer_is_supported() && gtk_layer_is_layer_window(GTK_WINDOW(toplevel))) {
        gboolean is_bottom = gtk_layer_get_anchor(GTK_WINDOW(toplevel), GTK_LAYER_SHELL_EDGE_BOTTOM);
        gboolean is_top = gtk_layer_get_anchor(GTK_WINDOW(toplevel), GTK_LAYER_SHELL_EDGE_TOP);
        if (is_bottom && !is_top) {
            anchor_bottom = TRUE;
            int m_left = gtk_layer_get_margin(GTK_WINDOW(toplevel), GTK_LAYER_SHELL_EDGE_LEFT);
            int m_bot = gtk_layer_get_margin(GTK_WINDOW(toplevel), GTK_LAYER_SHELL_EDGE_BOTTOM);
            int top_h = gtk_widget_get_allocated_height(toplevel);
            left = m_left + wx;
            if (m_bot == 0) {
                /* 28px bottom taskbar */
                edge = 28;
            } else {
                /* Start menu or flyout surface */
                edge = MAX(28, m_bot + MAX(0, top_h - wy - 160));
            }
        }
    }

    if (out_mon) *out_mon = mon;
    if (out_left) *out_left = left;
    if (out_edge) *out_edge = edge;
    if (out_anchor_bottom) *out_anchor_bottom = anchor_bottom;
}

static inline void on_ctx_app_open(GtkWidget *btn, gpointer data) {
    (void)btn;
    ChicagoAppMenuCtx *ctx = (ChicagoAppMenuCtx *)data;
    chicago_spawn_command(ctx->exec);
}

static inline void on_ctx_app_dgpu(GtkWidget *btn, gpointer data) {
    (void)btn;
    ChicagoAppMenuCtx *ctx = (ChicagoAppMenuCtx *)data;
    chicago_launch_with_dgpu(ctx->exec);
}

static inline void on_ctx_app_admin(GtkWidget *btn, gpointer data) {
    (void)btn;
    ChicagoAppMenuCtx *ctx = (ChicagoAppMenuCtx *)data;
    chicago_launch_as_admin(ctx->exec);
}

static inline void on_ctx_app_toggle_pin(GtkWidget *btn, gpointer data) {
    (void)btn;
    ChicagoAppMenuCtx *ctx = (ChicagoAppMenuCtx *)data;
    if (chicago_pins_is_pinned(ctx->name, ctx->exec)) {
        chicago_pins_remove(ctx->name, ctx->exec);
    } else {
        chicago_pins_add(ctx->name, ctx->icon, ctx->exec, ctx->desktop_path);
    }
}

static inline void on_ctx_app_desktop_shortcut(GtkWidget *btn, gpointer data) {
    (void)btn;
    ChicagoAppMenuCtx *ctx = (ChicagoAppMenuCtx *)data;
    chicago_create_desktop_shortcut(ctx->name, ctx->icon, ctx->exec, FALSE);
}

static inline void on_ctx_app_desktop_shortcut_dgpu(GtkWidget *btn, gpointer data) {
    (void)btn;
    ChicagoAppMenuCtx *ctx = (ChicagoAppMenuCtx *)data;
    chicago_create_desktop_shortcut(ctx->name, ctx->icon, ctx->exec, TRUE);
}

static inline void on_ctx_app_open_location(GtkWidget *btn, gpointer data) {
    (void)btn;
    ChicagoAppMenuCtx *ctx = (ChicagoAppMenuCtx *)data;
    if (ctx->desktop_path && *ctx->desktop_path) {
        char *dir = g_path_get_dirname(ctx->desktop_path);
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "thunar \"%s\"", dir);
        chicago_spawn_command(cmd);
        g_free(dir);
    } else {
        chicago_spawn_command("thunar /usr/share/applications");
    }
}

static inline void on_ctx_app_properties(GtkWidget *btn, gpointer data) {
    (void)btn;
    ChicagoAppMenuCtx *ctx = (ChicagoAppMenuCtx *)data;
    chicago_show_app_properties_dialog(ctx->name, ctx->icon, ctx->exec, ctx->desktop_path);
}

static inline void chicago_popup_app_context_menu_for_widget(GtkWidget *widget, const char *name, const char *icon, const char *exec, const char *desktop_path, GdkEventButton *event) {
    ChicagoAppMenuCtx *ctx = g_malloc0(sizeof(ChicagoAppMenuCtx));
    ctx->name = g_strdup(name ? name : "Application");
    ctx->icon = g_strdup(icon ? icon : "application-x-executable");
    ctx->exec = g_strdup(exec ? exec : "");
    ctx->desktop_path = g_strdup(desktop_path ? desktop_path : "");

    GdkMonitor *mon = NULL;
    int left = 100, edge = 100;
    gboolean anchor_bottom = FALSE;
    chicago_resolve_event_layer_coords(widget, event, &mon, &left, &edge, &anchor_bottom);

    GtkWidget *vbox = NULL;
    GtkWidget *win = chicago_ctx_menu_begin(mon, left, edge, anchor_bottom, &vbox);
    g_object_set_data_full(G_OBJECT(win), "app-ctx", ctx, (GDestroyNotify)chicago_app_menu_ctx_free);

    chicago_ctx_menu_add_item(vbox, "Open", ctx->icon, TRUE, G_CALLBACK(on_ctx_app_open), ctx);
    chicago_ctx_menu_add_item(vbox, "Run with Dedicated GPU (NVIDIA Quadro T1000)", "preferences-desktop-display", FALSE, G_CALLBACK(on_ctx_app_dgpu), ctx);
    chicago_ctx_menu_add_item(vbox, "Run as Administrator...", "dialog-password", FALSE, G_CALLBACK(on_ctx_app_admin), ctx);

    chicago_ctx_menu_add_separator(vbox);

    gboolean is_pinned = chicago_pins_is_pinned(ctx->name, ctx->exec);
    chicago_ctx_menu_add_item(vbox, is_pinned ? "Unpin from Taskbar" : "Pin to Taskbar",
                              "preferences-desktop", FALSE, G_CALLBACK(on_ctx_app_toggle_pin), ctx);
    chicago_ctx_menu_add_item(vbox, "Create Shortcut on Desktop", "user-desktop", FALSE, G_CALLBACK(on_ctx_app_desktop_shortcut), ctx);
    chicago_ctx_menu_add_item(vbox, "Create Desktop Shortcut (Dedicated GPU)", "preferences-desktop-display", FALSE, G_CALLBACK(on_ctx_app_desktop_shortcut_dgpu), ctx);
    chicago_ctx_menu_add_item(vbox, "Open File Location", "folder", FALSE, G_CALLBACK(on_ctx_app_open_location), ctx);

    chicago_ctx_menu_add_separator(vbox);

    chicago_ctx_menu_add_item(vbox, "Properties", "document-properties", FALSE, G_CALLBACK(on_ctx_app_properties), ctx);

    chicago_ctx_menu_show(win, mon, left, edge, anchor_bottom);
}

static inline void chicago_popup_app_context_menu(const char *name, const char *icon, const char *exec, const char *desktop_path, GdkEventButton *event) {
    chicago_popup_app_context_menu_for_widget(NULL, name, icon, exec, desktop_path, event);
}

#endif /* CHICAGO_COMMON_H */
