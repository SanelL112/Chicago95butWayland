#ifndef CHICAGO_COMMON_H
#define CHICAGO_COMMON_H

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <cairo.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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

#endif /* CHICAGO_COMMON_H */
