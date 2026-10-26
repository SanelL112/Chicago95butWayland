/*
 * chicago-control-panel: Native Windows 95 Control Panel, Run, Find, Help & Shut Down Dialogs
 * Uses GTK3 + GIO + POSIX / sysfs integration
 * License: GPL-3.0+
 */

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include "chicago_common.h"

static char *read_cmd_output(const char *cmd) {
    char *stdout_str = NULL;
    char *argv[] = { "/bin/sh", "-c", (char *)cmd, NULL };
    if (g_spawn_sync(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, &stdout_str, NULL, NULL, NULL)) {
        return stdout_str;
    }
    return g_strdup("");
}

static char *read_sysfs_str(const char *path) {
    char *content = NULL;
    if (g_file_get_contents(path, &content, NULL, NULL) && content) {
        g_strstrip(content);
        return content;
    }
    return g_strdup("N/A");
}

/* =========================================================================
 * 1. Classic Windows 95 "Run" Dialog (--run)
 * ========================================================================= */
static void on_run_browse_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    GtkEntry *entry = GTK_ENTRY(user_data);
    GtkWidget *fc = gtk_file_chooser_dialog_new(
        "Browse",
        NULL,
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Open", GTK_RESPONSE_ACCEPT,
        NULL);
    if (gtk_dialog_run(GTK_DIALOG(fc)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(fc));
        if (filename) {
            gtk_entry_set_text(entry, filename);
            g_free(filename);
        }
    }
    gtk_widget_destroy(fc);
}

static void on_run_ok(GtkWidget *w, gpointer user_data) {
    (void)w;
    GtkEntry *entry = GTK_ENTRY(user_data);
    const char *txt = gtk_entry_get_text(entry);
    if (txt && *txt) {
        chicago_spawn_command(txt);
    }
    gtk_main_quit();
}

static void show_run_dialog(void) {
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Run");
    gtk_window_set_default_size(GTK_WINDOW(win), 360, 160);
    gtk_window_set_resizable(GTK_WINDOW(win), FALSE);
    gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_CENTER);
    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 12);

    GtkWidget *top_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GdkPixbuf *pb = chicago_load_icon("system-run", 32);
    GtkWidget *img = gtk_image_new_from_pixbuf(pb);
    if (pb) g_object_unref(pb);
    GtkWidget *desc = gtk_label_new("Type the name of a program, folder, or document, and\nWindows will open it for you.");
    gtk_label_set_xalign(GTK_LABEL(desc), 0.0);
    gtk_box_pack_start(GTK_BOX(top_hbox), img, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(top_hbox), desc, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), top_hbox, FALSE, FALSE, 0);

    GtkWidget *entry_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *open_lbl = gtk_label_new_with_mnemonic("_Open:");
    GtkWidget *entry = gtk_entry_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(open_lbl), entry);
    g_signal_connect(entry, "activate", G_CALLBACK(on_run_ok), entry);
    gtk_box_pack_start(GTK_BOX(entry_hbox), open_lbl, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(entry_hbox), entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), entry_hbox, FALSE, FALSE, 0);

    GtkWidget *btn_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *btn_browse = gtk_button_new_with_mnemonic("_Browse...");
    GtkWidget *btn_cancel = gtk_button_new_with_mnemonic("Cancel");
    GtkWidget *btn_ok = gtk_button_new_with_mnemonic("OK");
    gtk_widget_set_size_request(btn_ok, 75, 24);
    gtk_widget_set_size_request(btn_cancel, 75, 24);
    gtk_widget_set_size_request(btn_browse, 75, 24);

    g_signal_connect(btn_ok, "clicked", G_CALLBACK(on_run_ok), entry);
    g_signal_connect(btn_cancel, "clicked", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(btn_browse, "clicked", G_CALLBACK(on_run_browse_clicked), entry);

    gtk_box_pack_end(GTK_BOX(btn_hbox), btn_browse, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(btn_hbox), btn_cancel, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(btn_hbox), btn_ok, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(vbox), btn_hbox, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(win), vbox);
    gtk_widget_show_all(win);
    gtk_widget_grab_focus(entry);
}

/* =========================================================================
 * 2. Classic Windows 95 "Shut Down Windows" Dialog (--shutdown)
 * ========================================================================= */
typedef struct {
    GtkWidget *rb_logout;
    GtkWidget *rb_suspend;
    GtkWidget *rb_lock;
    GtkWidget *rb_poweroff;
    GtkWidget *rb_reboot;
} ShutdownDialogWidgets;

static void on_shutdown_yes(GtkButton *btn, gpointer user_data) {
    (void)btn;
    ShutdownDialogWidgets *w = (ShutdownDialogWidgets *)user_data;
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->rb_logout))) {
        chicago_spawn_command("labwc -e");
    } else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->rb_suspend))) {
        chicago_spawn_command("systemctl suspend");
    } else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->rb_lock))) {
        chicago_spawn_command("swaylock -f -c 008080");
    } else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->rb_poweroff))) {
        chicago_spawn_command("systemctl poweroff");
    } else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->rb_reboot))) {
        chicago_spawn_command("systemctl reboot");
    }
    gtk_main_quit();
}

static void show_shutdown_dialog(void) {
    ShutdownDialogWidgets *w = g_malloc0(sizeof(ShutdownDialogWidgets));

    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Shut Down Windows");
    gtk_window_set_default_size(GTK_WINDOW(win), 380, 230);
    gtk_window_set_resizable(GTK_WINDOW(win), FALSE);
    gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_CENTER);
    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 14);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
    GdkPixbuf *pb = chicago_load_icon("system-shutdown", 32);
    GtkWidget *img = gtk_image_new_from_pixbuf(pb);
    if (pb) g_object_unref(pb);
    gtk_widget_set_valign(img, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);

    GtkWidget *rvbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *prompt = gtk_label_new("Are you sure you want to:");
    gtk_label_set_xalign(GTK_LABEL(prompt), 0.0);
    gtk_box_pack_start(GTK_BOX(rvbox), prompt, FALSE, FALSE, 2);

    w->rb_logout = gtk_radio_button_new_with_mnemonic(
        NULL, "_Close all programs and log out of Chicago-Wayland?");
    w->rb_suspend = gtk_radio_button_new_with_mnemonic_from_widget(
        GTK_RADIO_BUTTON(w->rb_logout), "Stand _by (Suspend system to RAM)?");
    w->rb_lock = gtk_radio_button_new_with_mnemonic_from_widget(
        GTK_RADIO_BUTTON(w->rb_logout), "_Lock workstation?");
    w->rb_poweroff = gtk_radio_button_new_with_mnemonic_from_widget(
        GTK_RADIO_BUTTON(w->rb_logout), "_Shut down the computer?");
    w->rb_reboot = gtk_radio_button_new_with_mnemonic_from_widget(
        GTK_RADIO_BUTTON(w->rb_logout), "_Restart the computer?");

    gtk_box_pack_start(GTK_BOX(rvbox), w->rb_logout, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(rvbox), w->rb_suspend, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(rvbox), w->rb_lock, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(rvbox), w->rb_poweroff, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(rvbox), w->rb_reboot, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(hbox), rvbox, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

    GtkWidget *btn_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *btn_yes = gtk_button_new_with_mnemonic("_Yes");
    GtkWidget *btn_no = gtk_button_new_with_mnemonic("_No");
    gtk_widget_set_size_request(btn_yes, 80, 24);
    gtk_widget_set_size_request(btn_no, 80, 24);

    g_signal_connect(btn_yes, "clicked", G_CALLBACK(on_shutdown_yes), w);
    g_signal_connect(btn_no, "clicked", G_CALLBACK(gtk_main_quit), NULL);

    gtk_box_pack_end(GTK_BOX(btn_hbox), btn_no, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(btn_hbox), btn_yes, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(vbox), btn_hbox, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(win), vbox);
    gtk_widget_show_all(win);
}

/* =========================================================================
 * 3. Classic Windows 95 "Find: All Applications & Files" (--find)
 * ========================================================================= */
typedef struct {
    GtkEntry *entry;
    GtkListStore *store;
    GtkTreeView *tree;
} FindDialogState;

static void populate_find_results(FindDialogState *st, const char *query) {
    gtk_list_store_clear(st->store);
    char *q_lower = g_ascii_strdown(query ? query : "", -1);

    GList *apps = g_app_info_get_all();
    for (GList *l = apps; l != NULL; l = l->next) {
        GAppInfo *app = G_APP_INFO(l->data);
        if (!g_app_info_should_show(app)) continue;
        const char *name = g_app_info_get_display_name(app);
        const char *desc = g_app_info_get_description(app);
        const char *exec = g_app_info_get_commandline(app);
        if (!name || !exec) continue;

        char *n_low = g_ascii_strdown(name, -1);
        char *d_low = g_ascii_strdown(desc ? desc : "", -1);
        gboolean match = (*q_lower == '\0') || strstr(n_low, q_lower) || strstr(d_low, q_lower);
        g_free(n_low);
        g_free(d_low);

        if (match) {
            GtkTreeIter iter;
            gtk_list_store_append(st->store, &iter);
            gtk_list_store_set(st->store, &iter,
                               0, name,
                               1, desc ? desc : "Application",
                               2, exec,
                               -1);
        }
    }
    g_list_free_full(apps, g_object_unref);
    g_free(q_lower);
}

static void on_find_query_changed(GtkEditable *editable, gpointer user_data) {
    FindDialogState *st = (FindDialogState *)user_data;
    populate_find_results(st, gtk_entry_get_text(GTK_ENTRY(editable)));
}

static void on_find_row_activated(GtkTreeView *tree_view, GtkTreePath *path,
                                  GtkTreeViewColumn *column, gpointer user_data) {
    (void)column;
    (void)user_data;
    GtkTreeModel *model = gtk_tree_view_get_model(tree_view);
    GtkTreeIter iter;
    if (gtk_tree_model_get_iter(model, &iter, path)) {
        char *cmd = NULL;
        gtk_tree_model_get(model, &iter, 2, &cmd, -1);
        if (cmd) {
            /* Strip %u / %U / %f / %F field codes from Exec line */
            char *pct = strchr(cmd, '%');
            if (pct) *pct = '\0';
            chicago_spawn_command(cmd);
            g_free(cmd);
            gtk_main_quit();
        }
    }
}

static void show_find_dialog(void) {
    FindDialogState *st = g_malloc0(sizeof(FindDialogState));

    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Find: All Programs & Files");
    gtk_window_set_default_size(GTK_WINDOW(win), 520, 380);
    gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_CENTER);
    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *lbl = gtk_label_new_with_mnemonic("_Named:");
    GtkWidget *entry = gtk_entry_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(lbl), entry);
    st->entry = GTK_ENTRY(entry);
    gtk_box_pack_start(GTK_BOX(hbox), lbl, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    st->store = gtk_list_store_new(3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    GtkWidget *tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(st->store));
    st->tree = GTK_TREE_VIEW(tree);

    GtkCellRenderer *rend = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree), -1, "Name", rend, "text", 0, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(tree), -1, "Description", rend, "text", 1, NULL);

    g_signal_connect(entry, "changed", G_CALLBACK(on_find_query_changed), st);
    g_signal_connect(tree, "row-activated", G_CALLBACK(on_find_row_activated), st);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scroll), tree);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);

    GtkWidget *hint = gtk_label_new("Double-click or press Enter on any item to launch it.");
    gtk_label_set_xalign(GTK_LABEL(hint), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox), hint, FALSE, FALSE, 0);

    populate_find_results(st, "");

    gtk_container_add(GTK_CONTAINER(win), vbox);
    gtk_widget_show_all(win);
    gtk_widget_grab_focus(entry);
}

/* =========================================================================
 * 4. Classic Windows 95 Help Viewer (--help-viewer)
 * ========================================================================= */
static void show_help_viewer(void) {
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Windows Help: Chicago-Wayland Desktop");
    gtk_window_set_default_size(GTK_WINDOW(win), 680, 480);
    gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_CENTER);
    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *nb = gtk_notebook_new();
    gtk_container_set_border_width(GTK_CONTAINER(nb), 8);

    struct {
        const char *tab_title;
        const char *rel_path;
    } docs[] = {
        { "Overview", "README.md" },
        { "Architecture", "docs/architecture.md" },
        { "Configuration", "docs/configuration.md" },
        { "Compatibility", "docs/compatibility.md" },
        { "Benchmarks", "docs/benchmarks.md" },
        { "Troubleshooting", "docs/troubleshooting.md" },
        { NULL, NULL }
    };

    for (int i = 0; docs[i].tab_title; i++) {
        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/chicago-wayland/%s", g_get_home_dir(), docs[i].rel_path);
        char *text = NULL;
        if (!g_file_get_contents(full_path, &text, NULL, NULL)) {
            text = g_strdup_printf("Documentation file: %s", full_path);
        }

        GtkWidget *tv = gtk_text_view_new();
        gtk_text_view_set_editable(GTK_TEXT_VIEW(tv), FALSE);
        gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(tv), GTK_WRAP_WORD_CHAR);
        gtk_text_view_set_left_margin(GTK_TEXT_VIEW(tv), 8);
        gtk_text_view_set_right_margin(GTK_TEXT_VIEW(tv), 8);
        GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv));
        gtk_text_buffer_set_text(buf, text, -1);
        g_free(text);

        GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);
        gtk_container_add(GTK_CONTAINER(sw), tv);
        gtk_notebook_append_page(GTK_NOTEBOOK(nb), sw, gtk_label_new(docs[i].tab_title));
    }

    gtk_container_add(GTK_CONTAINER(win), nb);
    gtk_widget_show_all(win);
}

/* =========================================================================
 * 5. Windows 95 Control Panel & System Properties (default / --tab ...)
 * ========================================================================= */
static void on_bg_color_apply(GtkButton *btn, gpointer user_data) {
    (void)btn;
    const char *hex = (const char *)user_data;
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "pkill -x chicago-desktop; CHICAGO_BG_COLOR=\"%s\" chicago-desktop &",
             hex);
    chicago_spawn_command(cmd);
}

static void on_brightness_changed(GtkRange *range, gpointer user_data) {
    (void)user_data;
    int pct = (int)gtk_range_get_value(range);
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "brightnessctl set %d%% >/dev/null 2>&1", CLAMP(pct, 5, 100));
    chicago_spawn_command(cmd);
}

static void on_cp_vol_changed(GtkRange *range, gpointer user_data) {
    (void)user_data;
    int pct = (int)gtk_range_get_value(range);
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "wpctl set-volume @DEFAULT_AUDIO_SINK@ %d%%", CLAMP(pct, 0, 150));
    chicago_spawn_command(cmd);
}

static void on_play_startup_sound(GtkButton *btn, gpointer user_data) {
    (void)btn;
    (void)user_data;
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "pw-play \"%s/chicago-wayland/themes/sounds/startup.ogg\" 2>/dev/null || true",
             g_get_home_dir());
    chicago_spawn_command(cmd);
}

static void on_reconfigure_labwc(GtkButton *btn, gpointer user_data) {
    (void)btn;
    (void)user_data;
    chicago_spawn_command("labwc -r");
}

static void show_control_panel(const char *initial_tab) {
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Control Panel - System Properties");
    gtk_window_set_default_size(GTK_WINDOW(win), 540, 420);
    gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_CENTER);
    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *root_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(root_vbox), 10);

    GtkWidget *nb = gtk_notebook_new();

    /* --- Tab 0: System Properties --- */
    GtkWidget *sys_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_container_set_border_width(GTK_CONTAINER(sys_hbox), 12);

    GdkPixbuf *comp_pb = chicago_load_icon("computer", 48);
    GtkWidget *comp_img = gtk_image_new_from_pixbuf(comp_pb);
    if (comp_pb) g_object_unref(comp_pb);
    gtk_widget_set_valign(comp_img, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(sys_hbox), comp_img, FALSE, FALSE, 4);

    char *uname_out = read_cmd_output("uname -sr");
    g_strstrip(uname_out);
    char *cpu_out = read_cmd_output("lscpu | awk -F: '/Model name/ {gsub(/^[ \\t]+/, \"\", $2); print $2; exit}'");
    g_strstrip(cpu_out);
    char *mem_out = read_cmd_output("free -h | awk '/^Mem:/ {print $2 \" Total (\" $3 \" Used)\"}'");
    g_strstrip(mem_out);

    char sys_markup[1024];
    snprintf(sys_markup, sizeof(sys_markup),
             "<b>System:</b>\n"
             "    Microsoft Windows 95 (Chicago-Wayland Native Edition)\n"
             "    4.00.950 B (%s / Labwc 0.9.6 wlroots 0.19)\n\n"
             "<b>Registered to:</b>\n"
             "    %s\n"
             "    Fedora Workstation 44 Wayland Session\n\n"
             "<b>Computer:</b>\n"
             "    %s\n"
             "    Memory: %s\n"
             "    Display Protocol: Native Wayland (wlr-layer-shell + xdg-shell)",
             uname_out, g_get_user_name(), cpu_out, mem_out);
    g_free(uname_out);
    g_free(cpu_out);
    g_free(mem_out);

    GtkWidget *sys_lbl = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(sys_lbl), sys_markup);
    gtk_label_set_xalign(GTK_LABEL(sys_lbl), 0.0);
    gtk_label_set_yalign(GTK_LABEL(sys_lbl), 0.0);
    gtk_box_pack_start(GTK_BOX(sys_hbox), sys_lbl, TRUE, TRUE, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(nb), sys_hbox, gtk_label_new("General"));

    /* --- Tab 1: Display Properties --- */
    GtkWidget *disp_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(disp_vbox), 8);

    GtkWidget *color_frame = gtk_frame_new("Desktop Color Palette");
    GtkWidget *color_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(color_hbox), 8);

    struct {
        const char *name;
        const char *hex;
    } palettes[] = {
        { "Win95 Teal (#008080)", "#008080" },
        { "Classic Navy (#000080)", "#000080" },
        { "Win2000 Blue (#3A6EA5)", "#3a6ea5" },
        { "Forest Green (#004040)", "#004040" },
        { NULL, NULL }
    };
    for (int i = 0; palettes[i].name; i++) {
        GtkWidget *cbtn = gtk_button_new_with_label(palettes[i].name);
        g_signal_connect(cbtn, "clicked", G_CALLBACK(on_bg_color_apply), (gpointer)palettes[i].hex);
        gtk_box_pack_start(GTK_BOX(color_hbox), cbtn, TRUE, TRUE, 0);
    }
    gtk_container_add(GTK_CONTAINER(color_frame), color_hbox);
    gtk_box_pack_start(GTK_BOX(disp_vbox), color_frame, FALSE, FALSE, 0);

    GtkWidget *bright_frame = gtk_frame_new("Internal Panel Brightness (eDP-1)");
    GtkWidget *bright_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 5, 100, 5);
    gtk_range_set_value(GTK_RANGE(bright_scale), 20);
    g_signal_connect(bright_scale, "value-changed", G_CALLBACK(on_brightness_changed), NULL);
    gtk_container_add(GTK_CONTAINER(bright_frame), bright_scale);
    gtk_box_pack_start(GTK_BOX(disp_vbox), bright_frame, FALSE, FALSE, 0);

    GtkWidget *mon_frame = gtk_frame_new("Connected Wayland Outputs (wlr-randr)");
    char *randr_out = read_cmd_output("wlr-randr 2>&1 || echo 'wlr-randr output unavailable'");
    GtkWidget *mon_tv = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(mon_tv), FALSE);
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(mon_tv)), randr_out, -1);
    g_free(randr_out);
    GtkWidget *mon_sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(mon_sw), mon_tv);
    gtk_container_add(GTK_CONTAINER(mon_frame), mon_sw);
    gtk_box_pack_start(GTK_BOX(disp_vbox), mon_frame, TRUE, TRUE, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(nb), disp_vbox, gtk_label_new("Display"));

    /* --- Tab 2: Audio & Multimedia --- */
    GtkWidget *aud_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(aud_vbox), 10);

    GtkWidget *vol_frame = gtk_frame_new("Master Output Volume (PipeWire / WirePlumber)");
    GtkWidget *vol_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 5);
    gtk_range_set_value(GTK_RANGE(vol_scale), 75);
    g_signal_connect(vol_scale, "value-changed", G_CALLBACK(on_cp_vol_changed), NULL);
    gtk_container_add(GTK_CONTAINER(vol_frame), vol_scale);
    gtk_box_pack_start(GTK_BOX(aud_vbox), vol_frame, FALSE, FALSE, 0);

    GtkWidget *snd_btn = gtk_button_new_with_label("Test Windows 95 Startup Sound (The Microsoft Sound)");
    g_signal_connect(snd_btn, "clicked", G_CALLBACK(on_play_startup_sound), NULL);
    gtk_box_pack_start(GTK_BOX(aud_vbox), snd_btn, FALSE, FALSE, 0);

    char *wp_status = read_cmd_output("wpctl status 2>&1 | head -n 35");
    GtkWidget *aud_tv = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(aud_tv), FALSE);
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(aud_tv)), wp_status, -1);
    g_free(wp_status);
    GtkWidget *aud_sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(aud_sw), aud_tv);
    gtk_box_pack_start(GTK_BOX(aud_vbox), aud_sw, TRUE, TRUE, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(nb), aud_vbox, gtk_label_new("Audio"));

    /* --- Tab 3: Network Neighborhood --- */
    GtkWidget *net_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(net_vbox), 10);
    char *nm_out = read_cmd_output("nmcli device status 2>&1; echo ''; ip -br addr 2>&1");
    GtkWidget *net_tv = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(net_tv), FALSE);
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(net_tv)), nm_out, -1);
    g_free(nm_out);
    GtkWidget *net_sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(net_sw), net_tv);
    gtk_box_pack_start(GTK_BOX(net_vbox), net_sw, TRUE, TRUE, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(nb), net_vbox, gtk_label_new("Network"));

    /* --- Tab 4: Power Management --- */
    GtkWidget *pwr_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(pwr_vbox), 10);

    char *bat_stat = read_sysfs_str("/sys/class/power_supply/BAT0/status");
    char *bat_cap = read_sysfs_str("/sys/class/power_supply/BAT0/capacity");
    char *cpu_gov = read_sysfs_str("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor");
    char *cpu_epp = read_sysfs_str("/sys/devices/system/cpu/cpu0/cpufreq/energy_performance_preference");
    char *nv_pm = read_sysfs_str("/sys/bus/pci/devices/0000:01:00.0/power/runtime_status");

    char pwr_info[1024];
    snprintf(pwr_info, sizeof(pwr_info),
             "<b>Power Source &amp; Battery (BAT0):</b>\n"
             "    Status: %s (%s%% capacity)\n"
             "    Charge Thresholds: 75%% start / 80%% end\n\n"
             "<b>Processor &amp; GPU Power State:</b>\n"
             "    CPU Scaling Governor: %s (EPP: %s)\n"
             "    NVIDIA Quadro T1000 Runtime PM: %s\n\n"
             "<b>Chicago-Wayland Power Architecture:</b>\n"
             "    • Zero desktop polling loops (event-driven Wayland sockets &amp; inotify)\n"
             "    • Clock aligned to 60s minute boundaries (0.017 wakeups/sec)\n"
             "    • Window animations, blur, and drop-shadows disabled\n"
             "    • On-demand XWayland (0 MB RAM when no X11 clients are open)",
             bat_stat, bat_cap, cpu_gov, cpu_epp, nv_pm);
    g_free(bat_stat);
    g_free(bat_cap);
    g_free(cpu_gov);
    g_free(cpu_epp);
    g_free(nv_pm);

    GtkWidget *pwr_lbl = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(pwr_lbl), pwr_info);
    gtk_label_set_xalign(GTK_LABEL(pwr_lbl), 0.0);
    gtk_label_set_yalign(GTK_LABEL(pwr_lbl), 0.0);
    gtk_box_pack_start(GTK_BOX(pwr_vbox), pwr_lbl, TRUE, TRUE, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(nb), pwr_vbox, gtk_label_new("Power"));

    /* --- Tab 5: Mouse, Keyboard & Compositor --- */
    GtkWidget *inp_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(inp_vbox), 10);
    GtkWidget *inp_lbl = gtk_label_new(
        "Keyboard Shortcuts (Windows 95 + Wayland Standard):\n\n"
        "  • Super (Windows Key): Open / Close Start Menu\n"
        "  • Alt + F4: Close Active Window\n"
        "  • Alt + Tab: Classic Windows 95 Window Switcher\n"
        "  • Alt + Space: Open Window Titlebar Menu (Restore/Move/Size/Minimize/Maximize/Close)\n"
        "  • Super + R: Open Run Dialog\n"
        "  • Super + F: Open Find Dialog\n"
        "  • Super + E: Open File Manager\n"
        "  • Super + Return: Open MS-DOS Prompt (Terminal)\n"
        "  • Super + Left / Right / Up / Down: Snap / Maximize / Restore Window\n"
        "  • Super + L: Lock Workstation");
    gtk_label_set_xalign(GTK_LABEL(inp_lbl), 0.0);
    gtk_box_pack_start(GTK_BOX(inp_vbox), inp_lbl, FALSE, FALSE, 0);

    GtkWidget *reconf_btn = gtk_button_new_with_label("Reload Labwc Compositor Configuration (labwc -r)");
    g_signal_connect(reconf_btn, "clicked", G_CALLBACK(on_reconfigure_labwc), NULL);
    gtk_box_pack_start(GTK_BOX(inp_vbox), reconf_btn, FALSE, FALSE, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(nb), inp_vbox, gtk_label_new("Input & Keys"));

    gtk_box_pack_start(GTK_BOX(root_vbox), nb, TRUE, TRUE, 0);

    GtkWidget *bottom_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *close_btn = gtk_button_new_with_label("OK");
    gtk_widget_set_size_request(close_btn, 80, 24);
    g_signal_connect(close_btn, "clicked", G_CALLBACK(gtk_main_quit), NULL);
    gtk_box_pack_end(GTK_BOX(bottom_hbox), close_btn, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(root_vbox), bottom_hbox, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(win), root_vbox);
    gtk_widget_show_all(win);

    if (initial_tab) {
        if (strcmp(initial_tab, "display") == 0) gtk_notebook_set_current_page(GTK_NOTEBOOK(nb), 1);
        else if (strcmp(initial_tab, "audio") == 0) gtk_notebook_set_current_page(GTK_NOTEBOOK(nb), 2);
        else if (strcmp(initial_tab, "network") == 0) gtk_notebook_set_current_page(GTK_NOTEBOOK(nb), 3);
        else if (strcmp(initial_tab, "power") == 0) gtk_notebook_set_current_page(GTK_NOTEBOOK(nb), 4);
        else if (strcmp(initial_tab, "input") == 0) gtk_notebook_set_current_page(GTK_NOTEBOOK(nb), 5);
    }
}

int main(int argc, char **argv) {
    gtk_init(&argc, &argv);
    chicago_apply_theme();

    const char *mode = "control-panel";
    const char *tab = "system";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--run") == 0) {
            mode = "run";
        } else if (strcmp(argv[i], "--shutdown") == 0) {
            mode = "shutdown";
        } else if (strcmp(argv[i], "--find") == 0) {
            mode = "find";
        } else if (strcmp(argv[i], "--help-viewer") == 0) {
            mode = "help";
        } else if (strcmp(argv[i], "--tab") == 0 && i + 1 < argc) {
            tab = argv[++i];
        }
    }

    if (strcmp(mode, "run") == 0) {
        show_run_dialog();
    } else if (strcmp(mode, "shutdown") == 0) {
        show_shutdown_dialog();
    } else if (strcmp(mode, "find") == 0) {
        show_find_dialog();
    } else if (strcmp(mode, "help") == 0) {
        show_help_viewer();
    } else {
        show_control_panel(tab);
    }

    gtk_main();
    return 0;
}
