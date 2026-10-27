/* Chicago95 GTK interface on ext-session-lock-v1 surfaces.
 * Only successful PAM authentication may request unlock. Crash, signal and
 * protocol-failure paths never unlock. No layer-shell overlay fallback exists.
 */
#define _GNU_SOURCE
#include <gtk/gtk.h>
#include <gdk/gdkwayland.h>
#include <gtk-session-lock.h>
#include <gio/gio.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <pwd.h>
#include <signal.h>
#include <fcntl.h>
#include "chicago_common.h"
#include "chicago_lock_auth.h"
#include "chicago_lock_ready.h"

typedef struct {
    GdkMonitor *monitor;
    GtkWidget *window;
    GtkWidget *pass_entry;
    GtkWidget *status_label;
    GtkWidget *ok_btn;
    GtkWidget *face_btn;
} LockView;

typedef struct {
    GtkSessionLockLock *lock;
    GHashTable *views;
    char *username;
    gboolean locked;
    gboolean authenticating;
    gboolean finished;
    gboolean unlocked;
    int ready_fd;
    guint startup_timeout;
    guint retry_timeout;
    const char *status;
    gboolean face_enabled;
    gboolean no_auto_face;
    gboolean face_cancelled;
    GSubprocess *face_process;
    guint face_timeout;
    guint face_kill_timeout;
} LockState;
static LockState g_lock = { .ready_fd = -1 };

static void quit_loop(void) {
    if (gtk_main_level() > 0) gtk_main_quit();
}

static void notify_parent(char byte) {
    if (g_lock.ready_fd >= 0) {
        ssize_t n;
        do { n = write(g_lock.ready_fd, &byte, 1); } while (n < 0 && errno == EINTR);
        close(g_lock.ready_fd);
        g_lock.ready_fd = -1;
        if (byte == CHICAGO_LOCK_READY && n != 1) {
            /* The caller cannot know we locked. Leave the compositor locked. */
            _exit(EXIT_FAILURE);
        }
    }
}

static void update_views(const char *message) {
    g_lock.status = message;
    GHashTableIter iter;
    gpointer value;
    g_hash_table_iter_init(&iter, g_lock.views);
    while (g_hash_table_iter_next(&iter, NULL, &value)) {
        LockView *view = value;
        gboolean enabled = g_lock.locked && !g_lock.authenticating && !g_lock.finished;
        gtk_widget_set_sensitive(view->pass_entry, enabled);
        gtk_widget_set_sensitive(view->ok_btn, enabled);
        if (view->face_btn)
            gtk_widget_set_sensitive(view->face_btn, enabled && !g_lock.face_process);
        gtk_label_set_text(GTK_LABEL(view->status_label), message);
    }
}

static void clear_password_entries(void) {
    GHashTableIter iter;
    gpointer value;
    g_hash_table_iter_init(&iter, g_lock.views);
    while (g_hash_table_iter_next(&iter, NULL, &value))
        gtk_entry_set_text(GTK_ENTRY(((LockView *)value)->pass_entry), "");
}

static void signal_face_group(int signal_number) {
    if (!g_lock.face_process) return;
    const char *id = g_subprocess_get_identifier(g_lock.face_process);
    gboolean sent = FALSE;
    if (id) {
        pid_t pid = (pid_t)g_ascii_strtoll(id, NULL, 10);
        if (pid > 1) sent = kill(-pid, signal_number) == 0;
    }
    /* Avoid sending the same signal twice to handlers in the worker. */
    if (!sent) g_subprocess_send_signal(g_lock.face_process, signal_number);
}

static gboolean force_face_stop(gpointer data) {
    (void)data;
    g_lock.face_kill_timeout = 0;
    if (g_lock.face_cancelled) signal_face_group(SIGKILL);
    return G_SOURCE_REMOVE;
}

static void stop_face_scan(void) {
    gboolean already_stopping = g_lock.face_cancelled;
    g_lock.face_cancelled = TRUE;
    if (g_lock.face_process && !already_stopping) {
        /* The helper owns a process group including PAM/Howdy/compare.py. */
        signal_face_group(SIGTERM);
        g_lock.face_kill_timeout = g_timeout_add_seconds(1, force_face_stop, NULL);
    }
}

static void unlock_session(void) {
    if (g_lock.finished || !g_lock.locked) return;
    stop_face_scan();
    gtk_session_lock_lock_unlock_and_destroy(g_lock.lock);
    gdk_display_sync(gdk_display_get_default());
    g_lock.unlocked = TRUE;
    g_lock.finished = TRUE;
    quit_loop();
}

static void face_verification_complete(gboolean success) {
    if (g_lock.finished || !g_lock.locked) return;
    if (success && !g_lock.face_cancelled) unlock_session();
    else if (!g_lock.authenticating)
        update_views(g_lock.face_cancelled ? "Face scan stopped. Type your password or retry Face Auth."
                     : "Face not recognized. Type your password or retry Face Auth.");
}

static void face_process_complete(GObject *source, GAsyncResult *result, gpointer data) {
    (void)data;
    GError *error = NULL;
    gboolean success = g_subprocess_wait_check_finish(G_SUBPROCESS(source), result, &error);
    g_clear_error(&error);
    if (g_lock.face_process != G_SUBPROCESS(source)) return;
    g_clear_object(&g_lock.face_process);
    if (g_lock.face_kill_timeout) {
        g_source_remove(g_lock.face_kill_timeout);
        g_lock.face_kill_timeout = 0;
    }
    if (g_lock.face_timeout) {
        g_source_remove(g_lock.face_timeout);
        g_lock.face_timeout = 0;
    }
    face_verification_complete(success);
}

static gboolean face_scan_expired(gpointer data) {
    (void)data;
    g_lock.face_timeout = 0;
    stop_face_scan();
    return G_SOURCE_REMOVE;
}

static void face_child_setup(gpointer data) {
    (void)data;
    if (setsid() < 0) _exit(126);
}

static void start_face_scan(void) {
    if (!g_lock.locked || g_lock.finished || !g_lock.face_enabled ||
        g_lock.face_process || g_lock.authenticating) return;
    char executable[4096];
    ssize_t n = readlink("/proc/self/exe", executable, sizeof(executable) - 1);
    if (n <= 0 || (size_t)n >= sizeof(executable) - 1) return;
    executable[n] = '\0';
    char *directory = g_path_get_dirname(executable);
    char *helper = g_build_filename(directory, "chicago-lock-face", NULL);
    GSubprocessLauncher *launcher = g_subprocess_launcher_new(
        G_SUBPROCESS_FLAGS_STDOUT_SILENCE | G_SUBPROCESS_FLAGS_STDERR_SILENCE);
    g_subprocess_launcher_set_child_setup(launcher, face_child_setup, NULL, NULL);
    g_lock.face_cancelled = FALSE;
    g_lock.face_process = g_subprocess_launcher_spawn(launcher, NULL, helper, NULL);
    g_object_unref(launcher);
    g_free(helper);
    g_free(directory);
    if (!g_lock.face_process) {
        update_views("Face Auth could not start. Type your password to unlock.");
        return;
    }
    update_views("Scanning your IR camera... You can also type your password.");
    g_lock.face_timeout = g_timeout_add_seconds(15, face_scan_expired, NULL);
    g_subprocess_wait_check_async(g_lock.face_process, NULL, face_process_complete, NULL);
}

static void on_face_clicked(GtkWidget *widget, gpointer data) {
    (void)widget; (void)data;
    start_face_scan();
}

static gboolean allow_retry(gpointer data) {
    (void)data;
    g_lock.retry_timeout = 0;
    if (!g_lock.finished) {
        g_lock.authenticating = FALSE;
        update_views("The password could not be verified. Please try again.");
    }
    return G_SOURCE_REMOVE;
}

typedef struct { char *username; char *password; } AuthTask;
static void free_secret(gpointer data) {
    AuthTask *credentials = data;
    chicago_lock_wipe(credentials->password, strlen(credentials->password));
    g_free(credentials->password);
    g_free(credentials->username);
    g_free(credentials);
}

static void authenticate_worker(GTask *task, gpointer source, gpointer data, GCancellable *cancel) {
    (void)source; (void)cancel;
    AuthTask *credentials = data;
    g_task_return_boolean(task, chicago_lock_authenticate(credentials->username, credentials->password));
}

static void authentication_complete(GObject *source, GAsyncResult *result, gpointer data) {
    (void)source; (void)data;
    gboolean success = g_task_propagate_boolean(G_TASK(result), NULL);
    if (g_lock.finished || !g_lock.locked) return;
    if (success) {
        unlock_session();
    } else {
        /* PAM supplies its own failure delay; also bound UI retries. */
        update_views("The password could not be verified. Please wait...");
        g_lock.retry_timeout = g_timeout_add_seconds(1, allow_retry, NULL);
    }
}

static void on_unlock_attempt(GtkWidget *widget, gpointer data) {
    (void)widget;
    LockView *view = data;
    if (!g_lock.locked || g_lock.authenticating || g_lock.finished) return;
    const char *text = gtk_entry_get_text(GTK_ENTRY(view->pass_entry));
    if (!text || !*text) return;
    char *password = g_strdup(text);
    g_lock.authenticating = TRUE;
    stop_face_scan();
    clear_password_entries();
    update_views("Verifying password...");
    GTask *task = g_task_new(NULL, NULL, authentication_complete, NULL);
    AuthTask *credentials = g_new0(AuthTask, 1);
    credentials->username = g_strdup(g_lock.username);
    credentials->password = password;
    g_task_set_task_data(task, credentials, free_secret);
    g_task_run_in_thread(task, authenticate_worker);
    g_object_unref(task);
}

static void on_cancel_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    LockView *view = data;
    stop_face_scan();
    gtk_entry_set_text(GTK_ENTRY(view->pass_entry), "");
    if (!g_lock.authenticating) {
        gtk_label_set_text(GTK_LABEL(view->status_label), "Type your password and press Enter to unlock.");
        gtk_widget_grab_focus(view->pass_entry);
    }
}

static GdkPixbuf *load_lock_key_icon(void) {
    const char *home = g_get_home_dir();
    char p1[512];
    snprintf(p1, sizeof(p1), "%s/.local/share/chicago-wayland/lockscreen/start.png", home);
    if (g_file_test(p1, G_FILE_TEST_EXISTS)) {
        GdkPixbuf *pb = gdk_pixbuf_new_from_file(p1, NULL);
        if (pb) return pb;
    }
    char executable[4096];
    ssize_t len = readlink("/proc/self/exe", executable, sizeof(executable) - 1);
    if (len > 0) {
        executable[len] = '\0';
        char *dir = g_path_get_dirname(executable);
        char *asset = g_build_filename(dir, "..", "themes", "lockscreen", "start.png", NULL);
        GdkPixbuf *pb = gdk_pixbuf_new_from_file(asset, NULL);
        g_free(asset);
        g_free(dir);
        if (pb) return pb;
    }
    return chicago_load_icon("dialog-password", 48);
}

static GtkWidget *build_chicago95_lock_dialog(LockView *view) {
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
    g_signal_connect(close_btn, "clicked", G_CALLBACK(on_cancel_clicked), view);
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
    view->pass_entry = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(view->pass_entry), FALSE);
    gtk_entry_set_invisible_char(GTK_ENTRY(view->pass_entry), '*');
    gtk_entry_set_max_length(GTK_ENTRY(view->pass_entry), 512);
    gtk_entry_set_input_purpose(GTK_ENTRY(view->pass_entry), GTK_INPUT_PURPOSE_PASSWORD);
    gtk_entry_set_input_hints(GTK_ENTRY(view->pass_entry), GTK_INPUT_HINT_NO_SPELLCHECK);
    gtk_widget_set_hexpand(view->pass_entry, TRUE);
    g_signal_connect(view->pass_entry, "activate", G_CALLBACK(on_unlock_attempt), view);
    gtk_grid_attach(GTK_GRID(grid), p_lbl, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), view->pass_entry, 1, 1, 1, 1);

    gtk_box_pack_start(GTK_BOX(center_vbox), grid, FALSE, FALSE, 4);

    view->status_label = gtk_label_new("Type your password and press Enter to unlock.");
    gtk_label_set_xalign(GTK_LABEL(view->status_label), 0.0f);
    gtk_label_set_line_wrap(GTK_LABEL(view->status_label), TRUE);
    gtk_box_pack_start(GTK_BOX(center_vbox), view->status_label, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(body_hbox), center_vbox, TRUE, TRUE, 0);

    /* Cancel clears input; it never dismisses the session lock. */
    GtkWidget *right_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    view->ok_btn = gtk_button_new_with_label("OK");
    GtkWidget *cancel_btn = gtk_button_new_with_label("Cancel");
    gtk_widget_set_size_request(view->ok_btn, 78, 24);
    gtk_widget_set_size_request(cancel_btn, 78, 24);
    g_signal_connect(view->ok_btn, "clicked", G_CALLBACK(on_unlock_attempt), view);
    g_signal_connect(cancel_btn, "clicked", G_CALLBACK(on_cancel_clicked), view);
    gtk_box_pack_start(GTK_BOX(right_vbox), view->ok_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(right_vbox), cancel_btn, FALSE, FALSE, 0);

    if (g_lock.face_enabled) {
        view->face_btn = gtk_button_new_with_label("Face Auth");
        g_signal_connect(view->face_btn, "clicked", G_CALLBACK(on_face_clicked), NULL);
        gtk_box_pack_start(GTK_BOX(right_vbox), view->face_btn, FALSE, FALSE, 4);
    }


    gtk_box_pack_start(GTK_BOX(body_hbox), right_vbox, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(inner_pad), body_hbox, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(dialog_frame), inner_pad, TRUE, TRUE, 0);

    return dialog_frame;
}

static gboolean block_close(GtkWidget *window, GdkEvent *event, gpointer data) {
    (void)window; (void)event; (void)data;
    return TRUE;
}

static void free_view(gpointer data) {
    LockView *view = data;
    /* gtk-session-lock handles surface destruction; do not hide/unmap it first. */
    gtk_widget_destroy(view->window);
    g_clear_object(&view->monitor);
    g_free(view);
}

static void add_monitor(GdkDisplay *display, GdkMonitor *monitor, gpointer data) {
    (void)display; (void)data;
    if (g_lock.finished || g_hash_table_contains(g_lock.views, monitor)) return;
    LockView *view = g_new0(LockView, 1);
    view->monitor = g_object_ref(monitor);
    view->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(view->window), "Chicago95 Workstation Locked");
    gtk_window_set_decorated(GTK_WINDOW(view->window), FALSE);
    gtk_widget_set_name(view->window, "chicago-lock-bg");
    gtk_style_context_add_class(gtk_widget_get_style_context(view->window), "chicago-lock-bg");
    g_signal_connect(view->window, "delete-event", G_CALLBACK(block_close), NULL);
    GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(outer), build_chicago95_lock_dialog(view), TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(view->window), outer);
    /* This MUST precede realize/show. Exactly one lock surface per output. */
    gtk_session_lock_lock_new_surface(g_lock.lock, GTK_WINDOW(view->window), monitor);
    g_hash_table_insert(g_lock.views, monitor, view);
    gtk_widget_set_sensitive(view->pass_entry, g_lock.locked && !g_lock.authenticating);
    gtk_widget_set_sensitive(view->ok_btn, g_lock.locked && !g_lock.authenticating);
    if (view->face_btn)
        gtk_widget_set_sensitive(view->face_btn, g_lock.locked && !g_lock.authenticating && !g_lock.face_process);
    if (g_lock.status) gtk_label_set_text(GTK_LABEL(view->status_label), g_lock.status);
    gtk_widget_show_all(view->window);
    gtk_widget_grab_focus(view->pass_entry);
}

static void remove_monitor(GdkDisplay *display, GdkMonitor *monitor, gpointer data) {
    (void)display; (void)data;
    g_hash_table_remove(g_lock.views, monitor);
}

static void on_locked(GtkSessionLockLock *lock, gpointer data) {
    (void)lock; (void)data;
    if (g_lock.finished) return;
    g_lock.locked = TRUE;
    if (g_lock.startup_timeout) {
        g_source_remove(g_lock.startup_timeout);
        g_lock.startup_timeout = 0;
    }
    update_views("Type your password and press Enter to unlock.");
    notify_parent(CHICAGO_LOCK_READY);
    if (!g_lock.no_auto_face) start_face_scan();
}

static void on_finished(GtkSessionLockLock *lock, gpointer data) {
    (void)data;
    g_lock.finished = TRUE;
    stop_face_scan();
    /* A rejected request may be destroyed. A locked session must stay locked
     * on failure; only the successful PAM callback sends unlock_and_destroy. */
    if (!g_lock.locked) gtk_session_lock_lock_destroy(lock);
    notify_parent(CHICAGO_LOCK_FAILED);
    quit_loop();
}

static gboolean startup_expired(gpointer data) {
    (void)data;
    g_lock.startup_timeout = 0;
    g_lock.finished = TRUE;
    notify_parent(CHICAGO_LOCK_FAILED);
    quit_loop();
    return G_SOURCE_REMOVE;
}

static int run_locker(void) {
    /* Passwords should never be written to a core dump. */
    struct rlimit no_core = {0, 0};
    if (setrlimit(RLIMIT_CORE, &no_core) != 0) return 1;
    gdk_set_allowed_backends("wayland");
    if (!gtk_init_check(NULL, NULL) || !gtk_session_lock_is_supported()) {
        notify_parent(CHICAGO_LOCK_UNSUPPORTED); /* No lock request was sent. */
        return 78;
    }
    struct passwd *user = getpwuid(getuid());
    if (!user || !user->pw_name || !*user->pw_name) return 1;
    g_lock.username = g_strdup(user->pw_name);
    g_lock.face_enabled = chicago_lock_face_available();
    g_lock.views = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, free_view);
    chicago_apply_theme();
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css,
        "window.chicago-lock-bg { background-color: #008080; }"
        ".lock-titlebar { background-color: #000080; color: #ffffff; padding: 2px 3px; min-height: 18px; }",
        -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(css),
                                             GTK_STYLE_PROVIDER_PRIORITY_USER + 250);
    g_object_unref(css);
    g_lock.lock = gtk_session_lock_prepare_lock();
    if (!g_lock.lock) return 1;
    g_signal_connect(g_lock.lock, "locked", G_CALLBACK(on_locked), NULL);
    g_signal_connect(g_lock.lock, "finished", G_CALLBACK(on_finished), NULL);
    g_lock.startup_timeout = g_timeout_add_seconds(10, startup_expired, NULL);
    gtk_session_lock_lock_lock(g_lock.lock);
    GdkDisplay *display = gdk_display_get_default();
    g_signal_connect(display, "monitor-added", G_CALLBACK(add_monitor), NULL);
    g_signal_connect(display, "monitor-removed", G_CALLBACK(remove_monitor), NULL);
    for (int i = 0; i < gdk_display_get_n_monitors(display) && !g_lock.finished; i++)
        add_monitor(display, gdk_display_get_monitor(display, i), NULL);
    if (!g_lock.finished) gtk_main();
    /* No generic cleanup path ever requests unlock. */
    if (!g_lock.unlocked) notify_parent(CHICAGO_LOCK_FAILED);
    stop_face_scan();
    signal_face_group(SIGKILL); /* Do not leave a camera worker behind on exit. */
    if (g_lock.face_kill_timeout) g_source_remove(g_lock.face_kill_timeout);
    if (g_lock.face_timeout) g_source_remove(g_lock.face_timeout);
    g_clear_object(&g_lock.face_process);
    g_signal_handlers_disconnect_by_func(display, add_monitor, NULL);
    g_signal_handlers_disconnect_by_func(display, remove_monitor, NULL);
    if (g_lock.startup_timeout) g_source_remove(g_lock.startup_timeout);
    if (g_lock.retry_timeout) g_source_remove(g_lock.retry_timeout);
    g_clear_pointer(&g_lock.views, g_hash_table_unref);
    g_object_unref(g_lock.lock);
    g_free(g_lock.username);
    return g_lock.unlocked ? 0 : 1;
}

int main(int argc, char **argv) {
    gboolean foreground = FALSE;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--foreground") == 0) foreground = TRUE;
        else if (strcmp(argv[i], "--no-auto-face") == 0) g_lock.no_auto_face = TRUE;
        else return 2;
    }
    signal(SIGPIPE, SIG_IGN);
    if (foreground) return run_locker();
    int fds[2];
    if (pipe2(fds, O_CLOEXEC) != 0) return 1;
    pid_t child = fork(); /* Always before GTK, GDK or worker threads start. */
    if (child < 0) { close(fds[0]); close(fds[1]); return 1; }
    if (child > 0) {
        close(fds[1]);
        char result = chicago_lock_wait_ready(fds[0], 15000);
        close(fds[0]);
        if (result == CHICAGO_LOCK_READY) return 0;
        kill(child, SIGTERM);
        while (waitpid(child, NULL, 0) < 0 && errno == EINTR) {}
        return result == CHICAGO_LOCK_UNSUPPORTED ? 78 : 1;
    }
    close(fds[0]);
    if (setsid() < 0) _exit(1);
    g_lock.ready_fd = fds[1];
    int result = run_locker();
    notify_parent(result == 78 ? CHICAGO_LOCK_UNSUPPORTED : CHICAGO_LOCK_FAILED);
    return result;
}
