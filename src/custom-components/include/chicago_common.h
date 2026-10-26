#ifndef CHICAGO_COMMON_H
#define CHICAGO_COMMON_H

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <cairo.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const char *CHICAGO95_CORE_CSS =
    "* {\n"
    "  border-radius: 0px;\n"
    "  box-shadow: none;\n"
    "  text-shadow: none;\n"
    "  -gtk-icon-shadow: none;\n"
    "  font-family: 'Microsoft Sans Serif', 'Liberation Sans', 'DejaVu Sans', Sans;\n"
    "  font-size: 9pt;\n"
    "}\n"
    "window.chicago-taskbar {\n"
    "  background-color: #c0c0c0;\n"
    "  color: #000000;\n"
    "  border-top: 1px solid #ffffff;\n"
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
    "  border-top: 2px solid #ffffff;\n"
    "  border-left: 2px solid #ffffff;\n"
    "  border-right: 2px solid #000000;\n"
    "  border-bottom: 2px solid #000000;\n"
    "}\n"
    ".win95-inset, entry, treeview, textview {\n"
    "  background-color: #ffffff;\n"
    "  color: #000000;\n"
    "  border-top: 2px solid #808080;\n"
    "  border-left: 2px solid #808080;\n"
    "  border-right: 2px solid #ffffff;\n"
    "  border-bottom: 2px solid #ffffff;\n"
    "}\n"
    ".win95-well {\n"
    "  background-color: #c0c0c0;\n"
    "  color: #000000;\n"
    "  border-top: 1px solid #808080;\n"
    "  border-left: 1px solid #808080;\n"
    "  border-right: 1px solid #ffffff;\n"
    "  border-bottom: 1px solid #ffffff;\n"
    "  padding: 1px 4px;\n"
    "}\n"
    "button, .win95-btn {\n"
    "  background-color: #c0c0c0;\n"
    "  background-image: none;\n"
    "  color: #000000;\n"
    "  border-top: 2px solid #ffffff;\n"
    "  border-left: 2px solid #ffffff;\n"
    "  border-right: 2px solid #000000;\n"
    "  border-bottom: 2px solid #000000;\n"
    "  padding: 2px 6px;\n"
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
    "  border-top: 2px solid #000000;\n"
    "  border-left: 2px solid #000000;\n"
    "  border-right: 2px solid #ffffff;\n"
    "  border-bottom: 2px solid #ffffff;\n"
    "}\n"
    "button.start-btn {\n"
    "  font-weight: bold;\n"
    "  padding: 1px 6px;\n"
    "  margin: 2px 2px 2px 2px;\n"
    "}\n"
    "button.task-btn {\n"
    "  padding: 1px 4px;\n"
    "  margin: 2px 1px;\n"
    "}\n"
    "button.task-btn.active-task {\n"
    "  font-weight: bold;\n"
    "  background-color: #dfdfdf;\n"
    "  border-top: 2px solid #000000;\n"
    "  border-left: 2px solid #000000;\n"
    "  border-right: 2px solid #ffffff;\n"
    "  border-bottom: 2px solid #ffffff;\n"
    "}\n"
    "button.tray-btn {\n"
    "  border: none;\n"
    "  background: transparent;\n"
    "  padding: 0px 2px;\n"
    "  min-height: 16px;\n"
    "  min-width: 16px;\n"
    "}\n"
    "menu, .win95-menu {\n"
    "  background-color: #c0c0c0;\n"
    "  color: #000000;\n"
    "  border-top: 2px solid #ffffff;\n"
    "  border-left: 2px solid #ffffff;\n"
    "  border-right: 2px solid #000000;\n"
    "  border-bottom: 2px solid #000000;\n"
    "  padding: 2px;\n"
    "}\n"
    "menuitem {\n"
    "  background-color: #c0c0c0;\n"
    "  color: #000000;\n"
    "  padding: 3px 8px;\n"
    "}\n"
    "menuitem:hover, menuitem:selected, .win95-menuitem:hover {\n"
    "  background-color: #000080;\n"
    "  color: #ffffff;\n"
    "}\n"
    "menuitem:hover label, menuitem:selected label, .win95-menuitem:hover label {\n"
    "  color: #ffffff;\n"
    "}\n"
    "button.win95-menuitem {\n"
    "  border: none;\n"
    "  background-color: #c0c0c0;\n"
    "  background-image: none;\n"
    "  color: #000000;\n"
    "  padding: 4px 8px;\n"
    "}\n"
    "button.win95-menuitem:hover, button.win95-menuitem:focus, button.win95-menuitem:active {\n"
    "  border: none;\n"
    "  background-color: #000080;\n"
    "  background-image: none;\n"
    "  color: #ffffff;\n"
    "}\n"
    "button.win95-menuitem:hover label, button.win95-menuitem:focus label, button.win95-menuitem:active label {\n"
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
    "  border-top: 2px solid #ffffff;\n"
    "  border-left: 2px solid #ffffff;\n"
    "  border-right: 2px solid #000000;\n"
    "  padding: 3px 8px;\n"
    "}\n"
    "notebook tab:checked {\n"
    "  background-color: #c0c0c0;\n"
    "  font-weight: bold;\n"
    "}\n"
    "notebook stack {\n"
    "  background-color: #c0c0c0;\n"
    "  border-top: 2px solid #ffffff;\n"
    "  border-left: 2px solid #ffffff;\n"
    "  border-right: 2px solid #000000;\n"
    "  border-bottom: 2px solid #000000;\n"
    "  padding: 8px;\n"
    "}\n"
    "frame > border {\n"
    "  border-top: 1px solid #808080;\n"
    "  border-left: 1px solid #808080;\n"
    "  border-right: 1px solid #ffffff;\n"
    "  border-bottom: 1px solid #ffffff;\n"
    "}\n"
    "scale trough {\n"
    "  background-color: #808080;\n"
    "  border-top: 1px solid #000000;\n"
    "  border-left: 1px solid #000000;\n"
    "  border-right: 1px solid #ffffff;\n"
    "  border-bottom: 1px solid #ffffff;\n"
    "  min-height: 6px;\n"
    "}\n"
    "scale slider {\n"
    "  background-color: #c0c0c0;\n"
    "  border-top: 2px solid #ffffff;\n"
    "  border-left: 2px solid #ffffff;\n"
    "  border-right: 2px solid #000000;\n"
    "  border-bottom: 2px solid #000000;\n"
    "  min-width: 11px;\n"
    "  min-height: 18px;\n"
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
                     "gtk-font-name", "Sans 9",
                     "gtk-enable-animations", FALSE,
                     NULL);
    }

    GtkIconTheme *icon_theme = gtk_icon_theme_get_default();
    if (icon_theme) {
        char user_icons[512];
        snprintf(user_icons, sizeof(user_icons), "%s/.local/share/icons", g_get_home_dir());
        gtk_icon_theme_prepend_search_path(icon_theme, user_icons);
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

static inline GdkPixbuf *chicago_draw_win95_flag_pixbuf(int size) {
    cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size, size);
    cairo_t *cr = cairo_create(surf);

    int half = size / 2 - 1;
    /* Red top-left */
    cairo_set_source_rgb(cr, 1.0, 0.15, 0.0);
    cairo_rectangle(cr, 1, 1, half - 1, half - 1);
    cairo_fill(cr);
    /* Green top-right */
    cairo_set_source_rgb(cr, 0.0, 0.65, 0.15);
    cairo_rectangle(cr, half + 1, 1, half - 1, half - 1);
    cairo_fill(cr);
    /* Blue bottom-left */
    cairo_set_source_rgb(cr, 0.0, 0.25, 0.85);
    cairo_rectangle(cr, 1, half + 1, half - 1, half - 1);
    cairo_fill(cr);
    /* Yellow bottom-right */
    cairo_set_source_rgb(cr, 1.0, 0.85, 0.0);
    cairo_rectangle(cr, half + 1, half + 1, half - 1, half - 1);
    cairo_fill(cr);

    /* Black pixel frame accent */
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
    GtkIconTheme *theme = gtk_icon_theme_get_default();
    GdkPixbuf *pb = NULL;

    if (icon_name && *icon_name) {
        if (g_path_is_absolute(icon_name) && g_file_test(icon_name, G_FILE_TEST_EXISTS)) {
            pb = gdk_pixbuf_new_from_file_at_scale(icon_name, size, size, TRUE, NULL);
            if (pb) return pb;
        }
        if (theme) {
            pb = gtk_icon_theme_load_icon(theme, icon_name, size, GTK_ICON_LOOKUP_FORCE_SIZE, NULL);
            if (pb) return pb;
        }
    }

    const char *fallbacks[] = {
        "application-x-executable",
        "utilities-terminal",
        "folder",
        "system-run",
        NULL
    };
    for (int i = 0; fallbacks[i] && theme; i++) {
        pb = gtk_icon_theme_load_icon(theme, fallbacks[i], size, GTK_ICON_LOOKUP_FORCE_SIZE, NULL);
        if (pb) return pb;
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

#endif /* CHICAGO_COMMON_H */
