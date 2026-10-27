/* Chicago95 locker launcher. Fall back only before a custom lock request has
 * been sent, or when --swaylock is explicitly requested. */
#define _POSIX_C_SOURCE 200809L
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <errno.h>

static int swaylock(int foreground) {
    char *args[] = { "swaylock", "--config", "/dev/null",
        "--color", "008080", "--inside-color", "c0c0c0",
        "--ring-color", "000080", "--text-color", "000000", "--font", "W95FA",
        foreground ? NULL : "--daemonize", NULL };
    execvp(args[0], args);
    perror("Cannot securely lock session: swaylock");
    return 1;
}

int main(int argc, char **argv) {
    int use_swaylock = 0, foreground = 0, no_auto_face = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            puts("Chicago95 secure custom lock screen\n"
                 "  --swaylock      Use the maintained fallback locker\n"
                 "  --foreground    Stay running until unlocked\n"
                 "  --no-auto-face  Disable the automatic face scan (keep Face Auth button)");
            return 0;
        } else if (!strcmp(argv[i], "--swaylock")) use_swaylock = 1;
        else if (!strcmp(argv[i], "--foreground")) foreground = 1;
        else if (!strcmp(argv[i], "--no-auto-face")) no_auto_face = 1;
        else {
            fprintf(stderr, "Unknown lock option: %s\n", argv[i]);
            return 2;
        }
    }
    if (use_swaylock) return swaylock(foreground);
    char path[4096];
    ssize_t n = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (n <= 0 || (size_t)n >= sizeof(path) - 1) return swaylock(foreground);
    path[n] = '\0';
    char *slash = strrchr(path, '/');
    if (!slash || (size_t)(slash - path) + sizeof("/chicago-lock-custom") > sizeof(path))
        return swaylock(foreground);
    strcpy(slash, "/chicago-lock-custom");
    pid_t child = fork();
    if (child < 0) return 1;
    if (!child) {
        char *args[4] = { path, NULL, NULL, NULL };
        int count = 1;
        if (foreground) args[count++] = "--foreground";
        if (no_auto_face) args[count++] = "--no-auto-face";
        execv(path, args);
        _exit(127);
    }
    int status;
    while (waitpid(child, &status, 0) < 0) {
        if (errno != EINTR) return 1;
    }
    if (!WIFEXITED(status)) return 1;
    int code = WEXITSTATUS(status);
    if (code == 78 || code == 127) {
        fprintf(stderr, "Custom lock unavailable before locking; using swaylock.\n");
        return swaylock(foreground);
    }
    /* Failure after a request is NOT an invitation to replace/unlock the lock. */
    return code;
}
