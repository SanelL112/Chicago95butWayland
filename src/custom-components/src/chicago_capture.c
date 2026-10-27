/*
 * chicago_capture.c - Authentic Windows 95 / GNOME-Style Screen Capture Tool
 *
 * Provides:
 *  1. Floating Interactive Capture Pill Bar (--interactive / Print)
 *  2. Standalone Windows 95 Snipping Tool GUI (no args / Start Menu)
 *  3. Direct CLI flags (--snip, --full, --window, --edit, --clipboard-only)
 *
 * Zero-polling, instant start, native Wayland (grim + slurp + swappy + wl-copy).
 */

#define _GNU_SOURCE
#include <gtk/gtk.h>
#include <gtk-layer-shell/gtk-layer-shell.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "chicago_common.h"

typedef enum {
    CAPTURE_MODE_REGION = 0,
    CAPTURE_MODE_SCREEN,
    CAPTURE_MODE_WINDOW
} CaptureMode;

typedef struct {
    CaptureMode mode;
    gboolean include_cursor;
    gboolean copy_clipboard;
    gboolean save_disk;
    gboolean open_swappy;
    int delay_seconds;
    char last_captured_path[1024];

    /* Floating Pill Widgets */
    GtkWidget *pill_window;
    GtkWidget *pill_btn_region;
    GtkWidget *pill_btn_screen;
    GtkWidget *pill_btn_window;
    GtkWidget *pill_chk_cursor;
    GtkWidget *pill_chk_swappy;

    /* Standalone Window Widgets */
    GtkWidget *app_window;
    GtkWidget *app_preview_image;
    GtkWidget *app_status_label;
    GtkWidget *app_mode_combo;
    GtkWidget *app_delay_combo;
    GtkWidget *app_chk_cursor;
    GtkWidget *app_chk_clipboard;
    GtkWidget *app_chk_swappy;
    GdkPixbuf *current_pixbuf;
} CaptureApp;

static CaptureApp g_cap;

/* CSS specific to Capture Tool (floating pill & preview well) */
static const char *CAPTURE_CSS =
    "window.capture-pill {\n"
    "  background-color: #c0c0c0;\n"
    "  border-top: 2px solid #ffffff;\n"
    "  border-left: 2px solid #ffffff;\n"
    "  border-right: 2px solid #000000;\n"
    "  border-bottom: 2px solid #000000;\n"
    "  box-shadow: inset -1px -1px 0px #808080, inset 1px 1px 0px #dfdfdf;\n"
    "  padding: 4px 6px;\n"
    "}\n"
    ".capture-handle {\n"
    "  background-color: #000080;\n"
    "  color: #ffffff;\n"
    "  font-weight: bold;\n"
    "  padding: 2px 6px;\n"
    "  font-size: 8pt;\n"
    "}\n"
    ".pill-btn {\n"
    "  padding: 3px 8px;\n"
    "  min-height: 24px;\n"
    "}\n"
    ".pill-btn:checked, .pill-btn-active {\n"
    "  background-color: #dfdfdf;\n"
    "  border-top: 1px solid #000000;\n"
    "  border-left: 1px solid #000000;\n"
    "  border-right: 1px solid #ffffff;\n"
    "  border-bottom: 1px solid #ffffff;\n"
    "  box-shadow: inset 1px 1px 0px #808080;\n"
    "  font-weight: bold;\n"
    "}\n"
    "button.capture-shutter-btn {\n"
    "  background-color: #c0c0c0;\n"
    "  font-weight: bold;\n"
    "  border-top: 2px solid #ffffff;\n"
    "  border-left: 2px solid #ffffff;\n"
    "  border-right: 2px solid #000000;\n"
    "  border-bottom: 2px solid #000000;\n"
    "  padding: 3px 12px;\n"
    "  color: #000000;\n"
    "}\n"
    "button.capture-shutter-btn:active {\n"
    "  border-top: 2px solid #000000;\n"
    "  border-left: 2px solid #000000;\n"
    "  border-right: 2px solid #ffffff;\n"
    "  border-bottom: 2px solid #ffffff;\n"
    "  background-color: #dfdfdf;\n"
    "}\n"
    ".preview-viewport {\n"
    "  background-color: #808080;\n"
    "}\n";

static void get_default_screenshots_dir(char *buf, size_t len) {
    const char *home = g_get_home_dir();
    snprintf(buf, len, "%s/Pictures/Screenshots", home);
    g_mkdir_with_parents(buf, 0755);
}

static void generate_screenshot_filename(char *buf, size_t len) {
    char dir[512];
    get_default_screenshots_dir(dir, sizeof(dir));
    time_t now = time(NULL);
    struct tm tm_info;
    localtime_r(&now, &tm_info);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d_%H.%M.%S", &tm_info);
    snprintf(buf, len, "%s/Screenshot_%s.png", dir, time_str);
}

static void play_shutter_sound(void) {
    chicago_spawn_command("canberra-gtk-play -i camera-shutter 2>/dev/null || "
                          "pw-play /usr/share/sounds/freedesktop/stereo/camera-shutter.oga 2>/dev/null &");
}

static void send_capture_notification(const char *filepath) {
    char cmd[1536];
    const char *fn = g_path_get_basename(filepath);
    snprintf(cmd, sizeof(cmd),
             "notify-send -a \"Capture Tool\" -i \"%s\" "
             "\"Screenshot Captured\" \"Saved to %s\\nCopied to clipboard\"",
             filepath, fn);
    chicago_spawn_command(cmd);
}

/*
 * Core capture execution function
 * Returns 0 on success, -1 on cancel / error
 */
static int do_capture(CaptureMode mode, gboolean include_cursor, gboolean copy_clipboard,
                      gboolean save_disk, gboolean open_swappy, int delay_sec,
                      char *out_path_buf, size_t out_path_len) {
    if (delay_sec > 0) {
        sleep(delay_sec);
    }

    char target_file[1024];
    if (save_disk) {
        generate_screenshot_filename(target_file, sizeof(target_file));
    } else {
        snprintf(target_file, sizeof(target_file), "/tmp/chicago_capture_%d.png", (int)getpid());
    }

    char cursor_arg[16] = "";
    if (include_cursor) {
        snprintf(cursor_arg, sizeof(cursor_arg), "-c ");
    }

    if (mode == CAPTURE_MODE_REGION || mode == CAPTURE_MODE_WINDOW) {
        /* Run slurp with Windows 95 cyan/navy highlights */
        const char *slurp_cmd = "slurp -d -b \"#00008033\" -c \"#000080ff\" -s \"#ffffff22\"";
        FILE *sfp = popen(slurp_cmd, "r");
        if (!sfp) return -1;
        char geom[128] = "";
        if (fgets(geom, sizeof(geom), sfp)) {
            geom[strcspn(geom, "\r\n")] = '\0';
        }
        int s_status = pclose(sfp);
        if (WEXITSTATUS(s_status) != 0 || geom[0] == '\0') {
            /* User pressed Escape or cancelled */
            return -1;
        }

        char grim_cmd[1536];
        snprintf(grim_cmd, sizeof(grim_cmd), "grim %s-g \"%s\" \"%s\"", cursor_arg, geom, target_file);
        int g_ret = system(grim_cmd);
        if (g_ret != 0 || !g_file_test(target_file, G_FILE_TEST_EXISTS)) {
            return -1;
        }
    } else {
        /* Full Screen / Monitor: query active output or prompt if multiple */
        char grim_cmd[1536];
        snprintf(grim_cmd, sizeof(grim_cmd), "grim %s\"%s\"", cursor_arg, target_file);
        int g_ret = system(grim_cmd);
        if (g_ret != 0 || !g_file_test(target_file, G_FILE_TEST_EXISTS)) {
            return -1;
        }
    }

    /* Clipboard Copy */
    if (copy_clipboard) {
        char wl_cmd[1536];
        snprintf(wl_cmd, sizeof(wl_cmd), "wl-copy -t image/png < \"%s\"", target_file);
        chicago_spawn_command(wl_cmd);
    }

    /* Shutter sound & notification */
    play_shutter_sound();
    send_capture_notification(target_file);

    /* Open in Swappy if requested */
    if (open_swappy) {
        char swap_cmd[1536];
        snprintf(swap_cmd, sizeof(swap_cmd), "swappy -f \"%s\" &", target_file);
        chicago_spawn_command(swap_cmd);
    }

    if (out_path_buf && out_path_len > 0) {
        g_strlcpy(out_path_buf, target_file, out_path_len);
    }

    return 0;
}

/* =========================================================================
 * Interactive Floating Capture Pill (GNOME 42+ capture tool style)
 * ========================================================================= */

static void trigger_pill_capture_mode(CaptureMode mode) {
    g_cap.mode = mode;

    /* 1. Release keyboard grab before unmapping */
    if (g_cap.pill_window) {
        gtk_layer_set_keyboard_mode(GTK_WINDOW(g_cap.pill_window), GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);
        gtk_widget_hide(g_cap.pill_window);
    }

    /* 2. Flush GDK and Wayland display to guarantee compositor unmaps surface */
    GdkDisplay *disp = gdk_display_get_default();
    if (disp) {
        gdk_display_flush(disp);
        gdk_display_sync(disp);
    }
    while (gtk_events_pending()) gtk_main_iteration();
    if (disp) gdk_display_flush(disp);
    usleep(80000); /* 80ms buffer */

    gboolean cur = g_cap.pill_chk_cursor ? gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_cap.pill_chk_cursor)) : FALSE;
    gboolean swp = g_cap.pill_chk_swappy ? gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_cap.pill_chk_swappy)) : FALSE;

    int res = do_capture(g_cap.mode, cur, TRUE, TRUE, swp, 0,
                         g_cap.last_captured_path, sizeof(g_cap.last_captured_path));

    if (res != 0) {
        /* Cancelled by user (e.g. Esc in slurp): restore keyboard mode & re-show pill */
        if (g_cap.pill_window) {
            gtk_layer_set_keyboard_mode(GTK_WINDOW(g_cap.pill_window), GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);
            gtk_widget_show_all(g_cap.pill_window);
        }
    } else {
        /* Success: exit cleanly */
        gtk_main_quit();
    }
}

static void on_pill_action_clicked(GtkButton *btn, gpointer data) {
    (void)btn;
    CaptureMode cm = (CaptureMode)GPOINTER_TO_INT(data);
    trigger_pill_capture_mode(cm);
}

static gboolean on_pill_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data) {
    (void)widget; (void)data;
    if (event->keyval == GDK_KEY_Escape) {
        gtk_main_quit();
        return TRUE;
    }
    if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter || event->keyval == GDK_KEY_space) {
        trigger_pill_capture_mode(g_cap.mode);
        return TRUE;
    }
    if (event->keyval == GDK_KEY_s || event->keyval == GDK_KEY_S) {
        trigger_pill_capture_mode(CAPTURE_MODE_REGION);
        return TRUE;
    }
    if (event->keyval == GDK_KEY_f || event->keyval == GDK_KEY_F) {
        trigger_pill_capture_mode(CAPTURE_MODE_SCREEN);
        return TRUE;
    }
    if (event->keyval == GDK_KEY_w || event->keyval == GDK_KEY_W) {
        trigger_pill_capture_mode(CAPTURE_MODE_WINDOW);
        return TRUE;
    }
    return FALSE;
}

static void create_interactive_pill(void) {
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_cap.pill_window = win;
    gtk_widget_set_name(win, "capture-pill");

    gtk_layer_init_for_window(GTK_WINDOW(win));
    gtk_layer_set_layer(GTK_WINDOW(win), GTK_LAYER_SHELL_LAYER_OVERLAY);
    gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    gtk_layer_set_margin(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_BOTTOM, 42);
    gtk_layer_set_keyboard_mode(GTK_WINDOW(win), GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 4);
    gtk_container_add(GTK_CONTAINER(win), hbox);

    /* Title Grip */
    GtkWidget *title_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_style_context_add_class(gtk_widget_get_style_context(title_box), "capture-handle");
    GtkWidget *cam_icon = gtk_image_new_from_pixbuf(chicago_load_icon("gnome-screenshot", 16));
    GtkWidget *title_lbl = gtk_label_new("Capture Tool");
    gtk_box_pack_start(GTK_BOX(title_box), cam_icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(title_box), title_lbl, FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(hbox), title_box, FALSE, FALSE, 0);

    /* Direct Action Buttons: Clicking immediately initiates that capture mode! */
    g_cap.pill_btn_region = gtk_button_new_with_label("Selection (S)");
    gtk_style_context_add_class(gtk_widget_get_style_context(g_cap.pill_btn_region), "pill-btn");
    gtk_style_context_add_class(gtk_widget_get_style_context(g_cap.pill_btn_region), "capture-shutter-btn");
    gtk_widget_set_tooltip_text(g_cap.pill_btn_region, "Click to drag and select a rectangular area to capture");
    g_signal_connect(g_cap.pill_btn_region, "clicked", G_CALLBACK(on_pill_action_clicked), GINT_TO_POINTER(CAPTURE_MODE_REGION));
    gtk_box_pack_start(GTK_BOX(hbox), g_cap.pill_btn_region, FALSE, FALSE, 0);

    g_cap.pill_btn_screen = gtk_button_new_with_label("Screen (F)");
    gtk_style_context_add_class(gtk_widget_get_style_context(g_cap.pill_btn_screen), "pill-btn");
    gtk_widget_set_tooltip_text(g_cap.pill_btn_screen, "Click to capture full screen or click a monitor");
    g_signal_connect(g_cap.pill_btn_screen, "clicked", G_CALLBACK(on_pill_action_clicked), GINT_TO_POINTER(CAPTURE_MODE_SCREEN));
    gtk_box_pack_start(GTK_BOX(hbox), g_cap.pill_btn_screen, FALSE, FALSE, 0);

    g_cap.pill_btn_window = gtk_button_new_with_label("Window (W)");
    gtk_style_context_add_class(gtk_widget_get_style_context(g_cap.pill_btn_window), "pill-btn");
    gtk_widget_set_tooltip_text(g_cap.pill_btn_window, "Click to select and capture a specific window");
    g_signal_connect(g_cap.pill_btn_window, "clicked", G_CALLBACK(on_pill_action_clicked), GINT_TO_POINTER(CAPTURE_MODE_WINDOW));
    gtk_box_pack_start(GTK_BOX(hbox), g_cap.pill_btn_window, FALSE, FALSE, 0);

    /* Separator */
    GtkWidget *sep1 = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    gtk_box_pack_start(GTK_BOX(hbox), sep1, FALSE, FALSE, 2);

    /* Options: Cursor, Swappy */
    g_cap.pill_chk_cursor = gtk_check_button_new_with_label("Pointer");
    gtk_widget_set_tooltip_text(g_cap.pill_chk_cursor, "Include mouse cursor in screenshot");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_cap.pill_chk_cursor), FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), g_cap.pill_chk_cursor, FALSE, FALSE, 0);

    g_cap.pill_chk_swappy = gtk_check_button_new_with_label("Annotate");
    gtk_widget_set_tooltip_text(g_cap.pill_chk_swappy, "Open in Swappy image editor immediately after capture");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_cap.pill_chk_swappy), FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), g_cap.pill_chk_swappy, FALSE, FALSE, 0);

    /* Separator */
    GtkWidget *sep2 = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    gtk_box_pack_start(GTK_BOX(hbox), sep2, FALSE, FALSE, 2);

    /* Close Button */
    GtkWidget *close_btn = gtk_button_new_with_label("✕ (Esc)");
    gtk_widget_set_tooltip_text(close_btn, "Cancel and close capture bar");
    g_signal_connect(close_btn, "clicked", G_CALLBACK(gtk_main_quit), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), close_btn, FALSE, FALSE, 0);

    g_signal_connect(win, "key-press-event", G_CALLBACK(on_pill_key_press), NULL);
    gtk_widget_show_all(win);
}

/* =========================================================================
 * Standalone Windows 95 Snipping Tool GUI
 * ========================================================================= */

static void update_app_preview(const char *image_path) {
    if (!image_path || !g_file_test(image_path, G_FILE_TEST_EXISTS)) return;
    if (g_cap.current_pixbuf) {
        g_object_unref(g_cap.current_pixbuf);
        g_cap.current_pixbuf = NULL;
    }

    g_cap.current_pixbuf = gdk_pixbuf_new_from_file(image_path, NULL);
    if (!g_cap.current_pixbuf) return;

    int orig_w = gdk_pixbuf_get_width(g_cap.current_pixbuf);
    int orig_h = gdk_pixbuf_get_height(g_cap.current_pixbuf);

    /* Scale preview to fit comfortably in viewport */
    int max_w = 460, max_h = 240;
    double scale = 1.0;
    if (orig_w > max_w || orig_h > max_h) {
        double sw = (double)max_w / orig_w;
        double sh = (double)max_h / orig_h;
        scale = sw < sh ? sw : sh;
    }
    int disp_w = (int)(orig_w * scale);
    int disp_h = (int)(orig_h * scale);
    if (disp_w < 1) disp_w = 1;
    if (disp_h < 1) disp_h = 1;

    GdkPixbuf *scaled = gdk_pixbuf_scale_simple(g_cap.current_pixbuf, disp_w, disp_h, GDK_INTERP_BILINEAR);
    if (scaled) {
        gtk_image_set_from_pixbuf(GTK_IMAGE(g_cap.app_preview_image), scaled);
        g_object_unref(scaled);
    }

    struct stat st;
    off_t size_bytes = 0;
    if (stat(image_path, &st) == 0) size_bytes = st.st_size;

    char status[512];
    snprintf(status, sizeof(status), "%d × %d pixels • %.1f KB • %s",
             orig_w, orig_h, size_bytes / 1024.0, g_path_get_basename(image_path));
    gtk_label_set_text(GTK_LABEL(g_cap.app_status_label), status);
}

static void trigger_app_capture(void) {
    gtk_widget_hide(g_cap.app_window);
    while (gtk_events_pending()) gtk_main_iteration();
    usleep(50000);

    CaptureMode mode = (CaptureMode)gtk_combo_box_get_active(GTK_COMBO_BOX(g_cap.app_mode_combo));
    int delay_idx = gtk_combo_box_get_active(GTK_COMBO_BOX(g_cap.app_delay_combo));
    int delays[] = { 0, 1, 3, 5 };
    int delay_sec = (delay_idx >= 0 && delay_idx < 4) ? delays[delay_idx] : 0;

    gboolean cur = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_cap.app_chk_cursor));
    gboolean clip = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_cap.app_chk_clipboard));
    gboolean swp = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_cap.app_chk_swappy));

    int res = do_capture(mode, cur, clip, TRUE, swp, delay_sec,
                         g_cap.last_captured_path, sizeof(g_cap.last_captured_path));

    gtk_widget_show_all(g_cap.app_window);

    if (res == 0) {
        update_app_preview(g_cap.last_captured_path);
    }
}

static void on_app_new_snip_clicked(GtkButton *btn, gpointer data) {
    (void)btn; (void)data;
    trigger_app_capture();
}

static void on_app_copy_clicked(GtkButton *btn, gpointer data) {
    (void)btn; (void)data;
    if (g_cap.last_captured_path[0] && g_file_test(g_cap.last_captured_path, G_FILE_TEST_EXISTS)) {
        char cmd[1536];
        snprintf(cmd, sizeof(cmd), "wl-copy -t image/png < \"%s\"", g_cap.last_captured_path);
        chicago_spawn_command(cmd);
        gtk_label_set_text(GTK_LABEL(g_cap.app_status_label), "Copied to clipboard!");
    }
}

static void on_app_save_as_clicked(GtkButton *btn, gpointer data) {
    (void)btn; (void)data;
    if (!g_cap.last_captured_path[0] || !g_file_test(g_cap.last_captured_path, G_FILE_TEST_EXISTS)) return;

    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        "Save Screenshot As...",
        GTK_WINDOW(g_cap.app_window),
        GTK_FILE_CHOOSER_ACTION_SAVE,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Save", GTK_RESPONSE_ACCEPT,
        NULL);

    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), g_path_get_basename(g_cap.last_captured_path));
    char dir[512];
    get_default_screenshots_dir(dir, sizeof(dir));
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), dir);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if (filename) {
            GFile *src = g_file_new_for_path(g_cap.last_captured_path);
            GFile *dst = g_file_new_for_path(filename);
            g_file_copy(src, dst, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, NULL);
            g_object_unref(src);
            g_object_unref(dst);
            g_free(filename);
        }
    }
    gtk_widget_destroy(dialog);
}

static void on_app_annotate_clicked(GtkButton *btn, gpointer data) {
    (void)btn; (void)data;
    if (g_cap.last_captured_path[0] && g_file_test(g_cap.last_captured_path, G_FILE_TEST_EXISTS)) {
        char cmd[1536];
        snprintf(cmd, sizeof(cmd), "swappy -f \"%s\" &", g_cap.last_captured_path);
        chicago_spawn_command(cmd);
    }
}

static void on_app_open_folder_clicked(GtkButton *btn, gpointer data) {
    (void)btn; (void)data;
    char dir[512];
    get_default_screenshots_dir(dir, sizeof(dir));
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "thunar \"%s\" &", dir);
    chicago_spawn_command(cmd);
}

static void create_standalone_app_window(void) {
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_cap.app_window = win;
    gtk_window_set_title(GTK_WINDOW(win), "Capture Tool");
    gtk_window_set_default_size(GTK_WINDOW(win), 520, 380);
    gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_CENTER);

    GdkPixbuf *app_icon = chicago_load_icon("gnome-screenshot", 32);
    if (app_icon) {
        gtk_window_set_icon(GTK_WINDOW(win), app_icon);
        g_object_unref(app_icon);
    }

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(win), vbox);

    /* Menu Bar */
    GtkWidget *menubar = gtk_menu_bar_new();
    GtkWidget *file_item = gtk_menu_item_new_with_mnemonic("_File");
    GtkWidget *file_menu = gtk_menu_new();
    GtkWidget *item_new = gtk_menu_item_new_with_label("New Snip (Ctrl+N)");
    g_signal_connect_swapped(item_new, "activate", G_CALLBACK(trigger_app_capture), NULL);
    GtkWidget *item_save = gtk_menu_item_new_with_label("Save As... (Ctrl+S)");
    g_signal_connect_swapped(item_save, "activate", G_CALLBACK(on_app_save_as_clicked), NULL);
    GtkWidget *item_folder = gtk_menu_item_new_with_label("Open Screenshots Folder");
    g_signal_connect_swapped(item_folder, "activate", G_CALLBACK(on_app_open_folder_clicked), NULL);
    GtkWidget *item_exit = gtk_menu_item_new_with_label("Exit");
    g_signal_connect_swapped(item_exit, "activate", G_CALLBACK(gtk_main_quit), NULL);

    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), item_new);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), item_save);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), item_folder);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), gtk_separator_menu_item_new());
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), item_exit);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(file_item), file_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), file_item);

    GtkWidget *edit_item = gtk_menu_item_new_with_mnemonic("_Edit");
    GtkWidget *edit_menu = gtk_menu_new();
    GtkWidget *item_copy = gtk_menu_item_new_with_label("Copy to Clipboard (Ctrl+C)");
    g_signal_connect_swapped(item_copy, "activate", G_CALLBACK(on_app_copy_clicked), NULL);
    GtkWidget *item_annot = gtk_menu_item_new_with_label("Annotate with Swappy (Ctrl+E)");
    g_signal_connect_swapped(item_annot, "activate", G_CALLBACK(on_app_annotate_clicked), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), item_copy);
    gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), item_annot);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(edit_item), edit_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), edit_item);

    gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, FALSE, 0);

    /* Toolbar */
    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(toolbar), 4);

    GtkWidget *btn_new = gtk_button_new_with_label("New Snip");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_new), "capture-shutter-btn");
    g_signal_connect(btn_new, "clicked", G_CALLBACK(on_app_new_snip_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(toolbar), btn_new, FALSE, FALSE, 0);

    GtkWidget *mode_lbl = gtk_label_new("Mode:");
    gtk_box_pack_start(GTK_BOX(toolbar), mode_lbl, FALSE, FALSE, 2);

    g_cap.app_mode_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(g_cap.app_mode_combo), "Rectangular Region");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(g_cap.app_mode_combo), "Full Screen");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(g_cap.app_mode_combo), "Window");
    gtk_combo_box_set_active(GTK_COMBO_BOX(g_cap.app_mode_combo), 0);
    gtk_box_pack_start(GTK_BOX(toolbar), g_cap.app_mode_combo, FALSE, FALSE, 0);

    GtkWidget *delay_lbl = gtk_label_new("Delay:");
    gtk_box_pack_start(GTK_BOX(toolbar), delay_lbl, FALSE, FALSE, 2);

    g_cap.app_delay_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(g_cap.app_delay_combo), "No Delay");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(g_cap.app_delay_combo), "1 second");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(g_cap.app_delay_combo), "3 seconds");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(g_cap.app_delay_combo), "5 seconds");
    gtk_combo_box_set_active(GTK_COMBO_BOX(g_cap.app_delay_combo), 0);
    gtk_box_pack_start(GTK_BOX(toolbar), g_cap.app_delay_combo, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);

    /* Options Bar */
    GtkWidget *optbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_container_set_border_width(GTK_CONTAINER(optbox), 4);

    g_cap.app_chk_clipboard = gtk_check_button_new_with_label("Copy to clipboard");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_cap.app_chk_clipboard), TRUE);
    gtk_box_pack_start(GTK_BOX(optbox), g_cap.app_chk_clipboard, FALSE, FALSE, 0);

    g_cap.app_chk_cursor = gtk_check_button_new_with_label("Include cursor");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_cap.app_chk_cursor), FALSE);
    gtk_box_pack_start(GTK_BOX(optbox), g_cap.app_chk_cursor, FALSE, FALSE, 0);

    g_cap.app_chk_swappy = gtk_check_button_new_with_label("Edit in Swappy");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_cap.app_chk_swappy), FALSE);
    gtk_box_pack_start(GTK_BOX(optbox), g_cap.app_chk_swappy, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), optbox, FALSE, FALSE, 0);

    /* Central Preview Area */
    GtkWidget *preview_frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(preview_frame), GTK_SHADOW_IN);
    gtk_container_set_border_width(GTK_CONTAINER(preview_frame), 6);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    GtkWidget *center_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_valign(center_box, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(center_box, GTK_ALIGN_CENTER);

    g_cap.app_preview_image = gtk_image_new();
    GdkPixbuf *welcome_icon = chicago_load_icon("gnome-screenshot", 48);
    if (welcome_icon) {
        gtk_image_set_from_pixbuf(GTK_IMAGE(g_cap.app_preview_image), welcome_icon);
        g_object_unref(welcome_icon);
    }
    GtkWidget *hint_lbl = gtk_label_new("Click 'New Snip' or press Enter to capture your screen.");
    gtk_box_pack_start(GTK_BOX(center_box), g_cap.app_preview_image, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(center_box), hint_lbl, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(scroll), center_box);
    gtk_container_add(GTK_CONTAINER(preview_frame), scroll);
    gtk_box_pack_start(GTK_BOX(vbox), preview_frame, TRUE, TRUE, 0);

    /* Bottom Status & Action Bar */
    GtkWidget *bot_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(bot_bar), 6);

    g_cap.app_status_label = gtk_label_new("Ready");
    gtk_style_context_add_class(gtk_widget_get_style_context(g_cap.app_status_label), "win95-well");
    gtk_widget_set_hexpand(g_cap.app_status_label, TRUE);
    gtk_label_set_xalign(GTK_LABEL(g_cap.app_status_label), 0.0);
    gtk_box_pack_start(GTK_BOX(bot_bar), g_cap.app_status_label, TRUE, TRUE, 0);

    GtkWidget *btn_copy = gtk_button_new_with_label("Copy");
    g_signal_connect(btn_copy, "clicked", G_CALLBACK(on_app_copy_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(bot_bar), btn_copy, FALSE, FALSE, 0);

    GtkWidget *btn_save = gtk_button_new_with_label("Save As...");
    g_signal_connect(btn_save, "clicked", G_CALLBACK(on_app_save_as_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(bot_bar), btn_save, FALSE, FALSE, 0);

    GtkWidget *btn_annot = gtk_button_new_with_label("Annotate");
    g_signal_connect(btn_annot, "clicked", G_CALLBACK(on_app_annotate_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(bot_bar), btn_annot, FALSE, FALSE, 0);

    GtkWidget *btn_folder = gtk_button_new_with_label("Open Folder");
    g_signal_connect(btn_folder, "clicked", G_CALLBACK(on_app_open_folder_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(bot_bar), btn_folder, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), bot_bar, FALSE, FALSE, 0);

    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    gtk_widget_show_all(win);
}

/* =========================================================================
 * Main Entry & CLI Dispatcher
 * ========================================================================= */

static void print_usage(const char *prog) {
    printf("Chicago Capture Tool - Authentic Windows 95 / Wayland Screen Capture\n\n");
    printf("Usage: %s [OPTIONS]\n\n", prog);
    printf("Options:\n");
    printf("  -i, --interactive      Launch GNOME-style floating interactive capture pill bar\n");
    printf("  -s, --snip             Instant interactive rectangular region snip\n");
    printf("  -f, --full             Instant full-screen capture\n");
    printf("  -w, --window           Instant window snip\n");
    printf("  -e, --edit             Capture region and open directly in Swappy editor\n");
    printf("  -c, --clipboard-only   Copy capture to clipboard only (do not save to file)\n");
    printf("  -d, --delay <seconds>  Delay capture by N seconds (default: 0)\n");
    printf("  -h, --help             Show this help message\n\n");
    printf("Without options, launches the standalone Windows 95 Capture Tool GUI window.\n");
}

int main(int argc, char *argv[]) {
    gboolean interactive_mode = FALSE;
    gboolean direct_snip = FALSE;
    gboolean direct_full = FALSE;
    gboolean direct_window = FALSE;
    gboolean edit_swappy = FALSE;
    gboolean clipboard_only = FALSE;
    int delay_sec = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--interactive") == 0) {
            interactive_mode = TRUE;
        } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--snip") == 0) {
            direct_snip = TRUE;
        } else if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--full") == 0) {
            direct_full = TRUE;
        } else if (strcmp(argv[i], "-w") == 0 || strcmp(argv[i], "--window") == 0) {
            direct_window = TRUE;
        } else if (strcmp(argv[i], "-e") == 0 || strcmp(argv[i], "--edit") == 0) {
            edit_swappy = TRUE;
            direct_snip = TRUE;
        } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--clipboard-only") == 0) {
            clipboard_only = TRUE;
        } else if ((strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--delay") == 0) && i + 1 < argc) {
            delay_sec = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }

    /* Headless CLI fast-path: no GTK loop overhead */
    if (direct_snip || direct_full || direct_window) {
        CaptureMode cm = CAPTURE_MODE_REGION;
        if (direct_full) cm = CAPTURE_MODE_SCREEN;
        else if (direct_window) cm = CAPTURE_MODE_WINDOW;

        char out[1024] = "";
        int r = do_capture(cm, FALSE, TRUE, !clipboard_only, edit_swappy, delay_sec, out, sizeof(out));
        return r == 0 ? 0 : 1;
    }

    /* GUI Modes: Initialize GTK */
    gtk_init(&argc, &argv);
    chicago_apply_theme();

    /* Apply capture-specific CSS */
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css, CAPTURE_CSS, -1, NULL);
    GdkScreen *screen = gdk_screen_get_default();
    if (screen) {
        gtk_style_context_add_provider_for_screen(screen, GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_USER + 210);
    }
    g_object_unref(css);

    if (interactive_mode) {
        create_interactive_pill();
    } else {
        create_standalone_app_window();
    }

    gtk_main();
    return 0;
}
