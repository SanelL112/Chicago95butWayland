/* Exercise the actual locker callbacks without a compositor or authentication. */
#define main locker_program_main
#include "../src/custom-components/src/chicago_lock_custom.c"
#undef main
#include <assert.h>
static int unlocks, destroys, syncs;
void __wrap_gtk_session_lock_lock_unlock_and_destroy(GtkSessionLockLock *lock) {
    (void)lock; ++unlocks;
}
void __wrap_gtk_session_lock_lock_destroy(GtkSessionLockLock *lock) {
    (void)lock; ++destroys;
}
void __wrap_gdk_display_sync(GdkDisplay *display) { (void)display; ++syncs; }

static void reset(void) {
    if (g_lock.views) g_hash_table_destroy(g_lock.views);
    if (g_lock.retry_timeout) g_source_remove(g_lock.retry_timeout);
    memset(&g_lock, 0, sizeof(g_lock));
    g_lock.ready_fd = -1;
    g_lock.lock = (GtkSessionLockLock *)(void *)&g_lock;
    g_lock.views = g_hash_table_new(g_direct_hash, g_direct_equal);
    unlocks = destroys = syncs = 0;
}
static void auth_result(gboolean ok) {
    GTask *task = g_task_new(NULL, NULL, NULL, NULL);
    g_task_return_boolean(task, ok);
    authentication_complete(NULL, G_ASYNC_RESULT(task), NULL);
    g_object_unref(task);
}
int main(void) {
    reset();
    /* Duplicate output notifications must not create a second surface. */
    GdkMonitor *monitor = (GdkMonitor *)(void *)&g_lock;
    g_hash_table_insert(g_lock.views, monitor, &g_lock);
    add_monitor(NULL, monitor, NULL);
    assert(g_hash_table_size(g_lock.views) == 1);
    remove_monitor(NULL, monitor, NULL);
    assert(g_hash_table_size(g_lock.views) == 0);
    int fds[2]; assert(pipe(fds) == 0);
    g_lock.ready_fd = fds[1];
    /* An output/window appearing cannot report success before locked. */
    assert(chicago_lock_wait_ready(fds[0], 0) == CHICAGO_LOCK_FAILED);
    on_locked(g_lock.lock, NULL);
    assert(chicago_lock_wait_ready(fds[0], 100) == CHICAGO_LOCK_READY);
    assert(g_lock.locked && !unlocks);
    close(fds[0]);
    on_finished(g_lock.lock, NULL);
    assert(g_lock.finished && !unlocks && !destroys);
    auth_result(TRUE); /* Late authentication cannot unlock a finished lock. */
    assert(!unlocks);

    reset();
    auth_result(TRUE); /* No unlock before compositor acknowledgement. */
    assert(!unlocks);
    on_unlock_attempt(NULL, NULL); /* Likewise, no auth may start before locked. */
    assert(!g_lock.authenticating);
    on_finished(g_lock.lock, NULL);
    assert(destroys == 1 && !unlocks);

    reset();
    g_lock.locked = TRUE; g_lock.authenticating = TRUE;
    on_unlock_attempt(NULL, NULL); /* No concurrent attempt; doesn't touch a view. */
    auth_result(FALSE);
    assert(!unlocks && !g_lock.unlocked);
    g_lock.finished = TRUE;
    allow_retry(NULL); /* Failure/teardown cannot reactivate input. */
    assert(g_lock.authenticating);

    reset();
    g_lock.locked = TRUE;
    auth_result(TRUE);
    assert(unlocks == 1 && syncs == 1 && g_lock.unlocked && g_lock.finished);
    auth_result(TRUE);
    assert(unlocks == 1); /* Duplicate completion must not send another unlock. */

    reset();
    g_lock.face_enabled = TRUE;
    start_face_scan(); /* Cannot activate the camera before locked. */
    assert(!g_lock.face_process);
    face_verification_complete(TRUE);
    assert(!unlocks);
    g_lock.locked = TRUE;
    g_lock.authenticating = TRUE;
    start_face_scan(); /* A password attempt prevents another face scan. */
    assert(!g_lock.face_process);
    face_verification_complete(FALSE);
    assert(g_lock.authenticating && !unlocks); /* Face failure preserves password attempt. */
    g_lock.authenticating = FALSE;
    g_lock.face_cancelled = TRUE;
    face_verification_complete(TRUE);
    assert(!unlocks); /* A canceled/timed-out match cannot unlock. */
    g_lock.face_cancelled = FALSE;
    face_verification_complete(TRUE);
    assert(unlocks == 1 && g_lock.unlocked);
    face_verification_complete(TRUE);
    assert(unlocks == 1);

    reset();
    g_lock.locked = TRUE;
    on_finished(g_lock.lock, NULL);
    g_lock.face_cancelled = FALSE;
    face_verification_complete(TRUE);
    assert(!unlocks); /* Late face completion cannot revive a finished lock. */

    reset();
    startup_expired(NULL);
    assert(g_lock.finished && !unlocks);
    assert(pipe(fds) == 0);
    close(fds[1]);
    assert(chicago_lock_wait_ready(fds[0], 100) == CHICAGO_LOCK_FAILED);
    close(fds[0]);
    assert(pipe(fds) == 0);
    assert(write(fds[1], "U", 1) == 1);
    assert(chicago_lock_wait_ready(fds[0], 100) == CHICAGO_LOCK_UNSUPPORTED);
    close(fds[0]); close(fds[1]);
    /* Use a mock worker beside this test executable, never the real camera. */
    const char *face_log = g_getenv("MOCK_FACE_LOG");
    if (face_log) {
        reset();
        g_lock.locked = TRUE;
        g_lock.face_enabled = TRUE;
        start_face_scan();
        assert(g_lock.face_process);
        GSubprocess *first = g_lock.face_process;
        start_face_scan();
        assert(g_lock.face_process == first); /* At most one scan. */
        long leader = 0, child = 0;
        int parsed = 0;
        for (int i = 0; i < 200 && parsed != 2; i++) {
            FILE *log = fopen(face_log, "r");
            if (log) {
                parsed = fscanf(log, "%ld %ld", &leader, &child);
                fclose(log);
            }
            if (parsed != 2) g_usleep(10000);
        }
        assert(parsed == 2);
        assert(leader > 1 && child > 1 && getpgid((pid_t)child) == (pid_t)leader);
        stop_face_scan();
        for (int i = 0; i < 300 && g_lock.face_process; i++) {
            while (g_main_context_iteration(NULL, FALSE)) {}
            g_usleep(10000);
        }
        assert(!g_lock.face_process && g_lock.face_cancelled && !unlocks);
        assert(kill((pid_t)child, 0) == -1 && errno == ESRCH);
    }
    g_hash_table_destroy(g_lock.views);
    puts("Lock acknowledgement, auth gating, failure paths and sync checks passed");
    return 0;
}
