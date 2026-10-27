/*
 * chicago_lock.c
 * Authentic Windows 95 / Chicago95 Wayland Session Lock Screen
 *
 * - Uses gtk-layer-shell OVERLAY + EXCLUSIVE keyboard grab across all monitors
 * - Renders the classic Chicago95 "Welcome to Windows / Enter Password" dialog
 *   with the 48x48 Golden Key + Windows Flag icon on #008080 Teal
 * - Authenticates via PAM (instant password check + background Howdy IR face auth)
 */

#define _POSIX_C_SOURCE 200809L
#include <gtk/gtk.h>
#include <gtk-layer-shell/gtk-layer-shell.h>
#include <security/pam_appl.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/file.h>
#include "chicago_common.h"

#define MAX_LOCK_MONITORS 8

typedef struct {
    GtkWidget *windows[MAX_LOCK_MONITORS];
    int win_count;
    GtkWidget *pass_entry;
    GtkWidget *status_label;
    GtkWidget *ok_btn;
    gboolean authenticating;
    gboolean unlocked;
    char username[128];
} LockState;

static LockState g_lock;

typedef struct {
    char *username;
    char *password;
    gboolean use_howdy;
    int pam_result;
} PamTaskData;

static int pam_conv_cb(int num_msg, const struct pam_message **msg,
                       struct pam_response **resp, void *appdata_ptr) {
    const char *passwd = (const char *)appdata_ptr;
    if (num_msg <= 0 || num_msg > PAM_MAX_NUM_MSG) return PAM_CONV_ERR;

    struct pam_response *reply = calloc((size_t)num_msg, sizeof(struct pam_response));
    if (!reply) return PAM_CONV_ERR;

    for (int i = 0; i < num_msg; i++) {
        switch (msg[i]->msg_style) {
            case PAM_PROMPT_ECHO_OFF:
            case PAM_PROMPT_ECHO_ON:
                reply[i].resp = strdup(passwd ? passwd : "");
                reply[i].resp_retcode = 0;
                break;
            case PAM_ERROR_MSG:
            case PAM_TEXT_INFO:
                reply[i].resp = NULL;
                reply[i].resp_retcode = 0;
                break;
            default:
                for (int j = 0; j < i; j++) free(reply[j].resp);
                free(reply);
                return PAM_CONV_ERR;
        }
    }
    *resp = reply;
    return PAM_SUCCESS;
}

static gboolean on_pam_complete_main(gpointer data) {
    PamTaskData *td = (PamTaskData *)data;
    if (g_lock.unlocked) {
        g_free(td->username);
        g_free(td->password);
        g_free(td);
        return G_SOURCE_REMOVE;
    }

    if (td->pam_result == PAM_SUCCESS) {
        g_lock.unlocked = TRUE;
        gtk_label_set_text(GTK_LABEL(g_lock.status_label), "Unlocked! Welcome back.");
        gtk_main_quit();
    } else if (!td->use_howdy) {
        g_lock.authenticating = FALSE;
        gtk_widget_set_sensitive(g_lock.pass_entry, TRUE);
        gtk_widget_set_sensitive(g_lock.ok_btn, TRUE);
        gtk_entry_set_text(GTK_ENTRY(g_lock.pass_entry), "");
        gtk_widget_grab_focus(g_lock.pass_entry);
        gtk_label_set_markup(GTK_LABEL(g_lock.status_label),
                             "<span foreground='#800000'><b>The password you typed is incorrect. Please try again.</b></span>");
    } else {
        /* Howdy face scan timed out or skipped; user can still type password or click Face Auth */
        if (!g_lock.authenticating) {
            gtk_label_set_text(GTK_LABEL(g_lock.status_label),
                               "Type your password and press Enter to unlock.");
        }
    }

    g_free(td->username);
    if (td->password) {
        memset(td->password, 0, strlen(td->password));
        g_free(td->password);
    }
    g_free(td);
    return G_SOURCE_REMOVE;
}

static gpointer pam_worker_thread(gpointer data) {
    PamTaskData *td = (PamTaskData *)data;
    struct pam_conv conv = {
        .conv = pam_conv_cb,
        .appdata_ptr = td->password
    };
    pam_handle_t *pamh = NULL;
    /* Use "swaylock" (includes login without blocking on howdy) for typed password,
     * and "hyprlock" (which runs howdy-pam first) for IR face authentication. */
    const char *service = td->use_howdy ? "hyprlock" : "swaylock";
    int retval = pam_start(service, td->username, &conv, &pamh);
    if (retval == PAM_SUCCESS) {
        retval = pam_authenticate(pamh, 0);
    }
    if (pamh) {
        pam_end(pamh, retval);
    }
    td->pam_result = retval;
    g_idle_add(on_pam_complete_main, td);
    return NULL;
}

static void start_howdy_face_auth(void) {
    if (g_lock.unlocked) return;
    if (!g_file_test("/etc/pam.d/hyprlock", G_FILE_TEST_EXISTS)) return;

    gtk_label_set_text(GTK_LABEL(g_lock.status_label),
                       "Scanning for IR Face Unlock (Howdy) or type password...");
    PamTaskData *td = g_malloc0(sizeof(PamTaskData));
    td->username = g_strdup(g_lock.username);
    td->password = g_strdup("");
    td->use_howdy = TRUE;
    td->pam_result = PAM_AUTH_ERR;
    GThread *th = g_thread_new("chicago-pam-howdy", pam_worker_thread, td);
    g_thread_unref(th);
}

static void on_unlock_attempt(GtkWidget *w, gpointer data) {
    (void)w;
    (void)data;
    if (g_lock.authenticating || g_lock.unlocked) return;

    const char *pw = gtk_entry_get_text(GTK_ENTRY(g_lock.pass_entry));
    if (!pw || !*pw) {
        start_howdy_face_auth();
        return;
    }

    g_lock.authenticating = TRUE;
    gtk_widget_set_sensitive(g_lock.pass_entry, FALSE);
    gtk_widget_set_sensitive(g_lock.ok_btn, FALSE);
    gtk_label_set_text(GTK_LABEL(g_lock.status_label), "Verifying password...");

    PamTaskData *td = g_malloc0(sizeof(PamTaskData));
    td->username = g_strdup(g_lock.username);
    td->password = g_strdup(pw);
    td->use_howdy = FALSE;
    td->pam_result = PAM_AUTH_ERR;
    GThread *th = g_thread_new("chicago-pam-pw", pam_worker_thread, td);
    g_thread_unref(th);
}

static void on_cancel_clicked(GtkButton *btn, gpointer data) {
    (void)btn;
    (void)data;
    if (g_lock.authenticating) return;
    gtk_entry_set_text(GTK_ENTRY(g_lock.pass_entry), "");
    gtk_label_set_text(GTK_LABEL(g_lock.status_label),
                       "Type your password and press Enter to unlock.");
    gtk_widget_grab_focus(g_lock.pass_entry);
}

static void on_face_btn_clicked(GtkButton *btn, gpointer data) {
    (void)btn;
    (void)data;
    start_howdy_face_auth();
    gtk_widget_grab_focus(g_lock.pass_entry);
}

static GdkPixbuf *load_lock_key_icon(void) {
    const char *home = g_get_home_dir();
    char p1[512];
    snprintf(p1, sizeof(p1), "%s/.local/share/chicago-wayland/lockscreen/start.png", home);
    if (g_file_test(p1, G_FILE_TEST_EXISTS)) {
        GdkPixbuf *pb = gdk_pixbuf_new_from_file(p1, NULL);
        if (pb) return pb;
    }
    snprintf(p1, sizeof(p1), "%s/chicago-wayland/themes/lockscreen/start.png", home);
    if (g_file_test(p1, G_FILE_TEST_EXISTS)) {
        GdkPixbuf *pb = gdk_pixbuf_new_from_file(p1, NULL);
        if (pb) return pb;
    }
    return chicago_load_icon("dialog-password", 48);
}

static GtkWidget *build_chicago95_lock_dialog(void) {
    GtkWidget *dialog_frame = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(dialog_frame, 510, -1);
    gtk_widget_set_halign(dialog_frame, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(dialog_frame, GTK_ALIGN_CENTER);
    gtk_style_context_add_class(gtk_widget_get_style_context(dialog_frame), "win95-outset");

    GtkWidget *inner_pad = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_set_border_width(GTK_CONTAINER(inner_pad), 3);

    /* 1. Classic #000080 Navy Titlebar */
    GtkWidget *titlebar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_name(titlebar, "lock-titlebar");
    gtk_style_context_add_class(gtk_widget_get_style_context(titlebar), "lock-titlebar");

    GdkPixbuf *flag_pb = chicago_draw_win95_flag_pixbuf(16);
    GtkWidget *flag_img = gtk_image_new_from_pixbuf(flag_pb);
    if (flag_pb) g_object_unref(flag_pb);
    gtk_box_pack_start(GTK_BOX(titlebar), flag_img, FALSE, FALSE, 2);

    GtkWidget *title_lbl = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title_lbl),
                         "<span foreground='#ffffff'><b>Enter Password — Workstation Locked</b></span>");
    gtk_label_set_xalign(GTK_LABEL(title_lbl), 0.0f);
    gtk_box_pack_start(GTK_BOX(titlebar), title_lbl, TRUE, TRUE, 0);

    GtkWidget *close_btn = gtk_button_new_with_label("×");
    gtk_widget_set_size_request(close_btn, 16, 14);
    g_signal_connect(close_btn, "clicked", G_CALLBACK(on_cancel_clicked), NULL);
    gtk_box_pack_end(GTK_BOX(titlebar), close_btn, FALSE, FALSE, 2);

    gtk_box_pack_start(GTK_BOX(inner_pad), titlebar, FALSE, FALSE, 0);

    /* 2. 3-Column Chicago95 Login Body (Key Icon | Credentials Form | OK/Cancel Buttons) */
    GtkWidget *body_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
    gtk_container_set_border_width(GTK_CONTAINER(body_hbox), 14);

    /* Left Column: 48x48 Golden Key + Windows Flag Icon */
    GtkWidget *left_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GdkPixbuf *key_pb = load_lock_key_icon();
    GtkWidget *key_img = gtk_image_new_from_pixbuf(key_pb);
    if (key_pb) g_object_unref(key_pb);
    gtk_widget_set_valign(key_img, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(left_vbox), key_img, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(body_hbox), left_vbox, FALSE, FALSE, 4);

    /* Center Column: Prompt + User name + Password + Status */
    GtkWidget *center_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *prompt_lbl = gtk_label_new("This workstation is in use and has been locked.\nPlease enter your system login credentials to unlock.");
    gtk_label_set_xalign(GTK_LABEL(prompt_lbl), 0.0f);
    gtk_box_pack_start(GTK_BOX(center_vbox), prompt_lbl, FALSE, FALSE, 0);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);

    GtkWidget *u_lbl = gtk_label_new_with_mnemonic("_User name:");
    gtk_label_set_xalign(GTK_LABEL(u_lbl), 0.0f);
    GtkWidget *u_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(u_entry), g_lock.username);
    gtk_editable_set_editable(GTK_EDITABLE(u_entry), FALSE);
    gtk_widget_set_can_focus(u_entry, FALSE);
    gtk_widget_set_hexpand(u_entry, TRUE);
    gtk_grid_attach(GTK_GRID(grid), u_lbl, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), u_entry, 1, 0, 1, 1);

    GtkWidget *p_lbl = gtk_label_new_with_mnemonic("_Password:");
    gtk_label_set_xalign(GTK_LABEL(p_lbl), 0.0f);
    g_lock.pass_entry = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(g_lock.pass_entry), FALSE);
    gtk_entry_set_invisible_char(GTK_ENTRY(g_lock.pass_entry), '*');
    gtk_widget_set_hexpand(g_lock.pass_entry, TRUE);
    g_signal_connect(g_lock.pass_entry, "activate", G_CALLBACK(on_unlock_attempt), NULL);
    gtk_grid_attach(GTK_GRID(grid), p_lbl, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), g_lock.pass_entry, 1, 1, 1, 1);

    gtk_box_pack_start(GTK_BOX(center_vbox), grid, FALSE, FALSE, 4);

    g_lock.status_label = gtk_label_new("Type your password and press Enter to unlock.");
    gtk_label_set_xalign(GTK_LABEL(g_lock.status_label), 0.0f);
    gtk_label_set_line_wrap(GTK_LABEL(g_lock.status_label), TRUE);
    gtk_box_pack_start(GTK_BOX(center_vbox), g_lock.status_label, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(body_hbox), center_vbox, TRUE, TRUE, 0);

    /* Right Column: Stacked OK / Cancel / Face Auth buttons */
    GtkWidget *right_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    g_lock.ok_btn = gtk_button_new_with_label("OK");
    GtkWidget *cancel_btn = gtk_button_new_with_label("Cancel");
    gtk_widget_set_size_request(g_lock.ok_btn, 78, 24);
    gtk_widget_set_size_request(cancel_btn, 78, 24);
    g_signal_connect(g_lock.ok_btn, "clicked", G_CALLBACK(on_unlock_attempt), NULL);
    g_signal_connect(cancel_btn, "clicked", G_CALLBACK(on_cancel_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(right_vbox), g_lock.ok_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(right_vbox), cancel_btn, FALSE, FALSE, 0);

    if (g_file_test("/etc/pam.d/hyprlock", G_FILE_TEST_EXISTS)) {
        GtkWidget *face_btn = gtk_button_new_with_label("Face Auth");
        gtk_widget_set_size_request(face_btn, 78, 24);
        g_signal_connect(face_btn, "clicked", G_CALLBACK(on_face_btn_clicked), NULL);
        gtk_box_pack_start(GTK_BOX(right_vbox), face_btn, FALSE, FALSE, 4);
    }

    gtk_box_pack_start(GTK_BOX(body_hbox), right_vbox, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(inner_pad), body_hbox, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(dialog_frame), inner_pad, TRUE, TRUE, 0);

    return dialog_frame;
}

static void create_lock_surface_for_monitor(GdkMonitor *mon, int idx, gboolean is_primary) {
    if (g_lock.win_count >= MAX_LOCK_MONITORS) return;
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Chicago95 Lock Screen");
    gtk_window_set_decorated(GTK_WINDOW(win), FALSE);
    gtk_widget_set_name(win, "chicago-lock-bg");
    gtk_style_context_add_class(gtk_widget_get_style_context(win), "chicago-lock-bg");

    if (gtk_layer_is_supported()) {
        gtk_layer_init_for_window(GTK_WINDOW(win));
        gtk_layer_set_layer(GTK_WINDOW(win), GTK_LAYER_SHELL_LAYER_OVERLAY);
        if (mon) {
            gtk_layer_set_monitor(GTK_WINDOW(win), mon);
        }
        gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
        gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
        gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
        gtk_layer_set_anchor(GTK_WINDOW(win), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
        gtk_layer_set_exclusive_zone(GTK_WINDOW(win), -1);
        gtk_layer_set_keyboard_mode(GTK_WINDOW(win),
            is_primary ? GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE : GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);
        gtk_layer_set_namespace(GTK_WINDOW(win), "chicago-lock");
    } else {
        gtk_window_set_default_size(GTK_WINDOW(win), 1024, 768);
    }

    if (is_primary) {
        GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        GtkWidget *dialog = build_chicago95_lock_dialog();
        gtk_box_pack_start(GTK_BOX(outer), dialog, TRUE, TRUE, 0);
        gtk_container_add(GTK_CONTAINER(win), outer);
    } else {
        GtkWidget *blank = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_container_add(GTK_CONTAINER(win), blank);
    }

    gtk_widget_show_all(win);
    if (is_primary && g_lock.pass_entry) {
        gtk_widget_grab_focus(g_lock.pass_entry);
    }
    g_lock.windows[idx] = win;
    g_lock.win_count++;
}

int main(int argc, char **argv) {
    gboolean skip_howdy_auto = FALSE;
    for (int i = 1; i < argc; i++) {
        if (g_strcmp0(argv[i], "--no-auto-face") == 0) {
            skip_howdy_auto = TRUE;
        }
    }

    /* Prevent duplicate concurrent lock instances */
    char lock_path[256];
    const char *rt = g_getenv("XDG_RUNTIME_DIR");
    snprintf(lock_path, sizeof(lock_path), "%s/chicago-lock.lock", rt ? rt : "/tmp");
    int lock_fd = open(lock_path, O_CREAT | O_RDWR, 0600);
    if (lock_fd >= 0) {
        if (flock(lock_fd, LOCK_EX | LOCK_NB) != 0) {
            return 0;
        }
    }

    gtk_init(NULL, NULL);
    chicago_apply_theme();

    /* Add #008080 Teal background and #000080 Navy titlebar CSS for lock screen */
    GtkCssProvider *lock_css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(lock_css,
        "window.chicago-lock-bg {\n"
        "  background-color: #008080;\n"
        "}\n"
        ".lock-titlebar {\n"
        "  background-color: #000080;\n"
        "  color: #ffffff;\n"
        "  padding: 2px 3px;\n"
        "  min-height: 18px;\n"
        "}\n", -1, NULL);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(lock_css),
        GTK_STYLE_PROVIDER_PRIORITY_USER + 250);
    g_object_unref(lock_css);

    memset(&g_lock, 0, sizeof(g_lock));
    const char *u = g_get_user_name();
    snprintf(g_lock.username, sizeof(g_lock.username), "%s", u ? u : "user");

    GdkDisplay *display = gdk_display_get_default();
    int n_monitors = display ? gdk_display_get_n_monitors(display) : 0;
    if (n_monitors > 0) {
        for (int i = 0; i < n_monitors && i < MAX_LOCK_MONITORS; i++) {
            create_lock_surface_for_monitor(gdk_display_get_monitor(display, i), i, (i == 0));
        }
    } else {
        create_lock_surface_for_monitor(NULL, 0, TRUE);
    }

    if (!skip_howdy_auto) {
        start_howdy_face_auth();
    }

    gtk_main();

    if (lock_fd >= 0) {
        unlink(lock_path);
        close(lock_fd);
    }
    return 0;
}
