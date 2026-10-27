/* Isolated face-only PAM worker. No Wayland/GTK connection or password input. */
#define _POSIX_C_SOURCE 200809L
#include "chicago_lock_auth.h"
#include <pwd.h>
#include <unistd.h>
#include <string.h>
#include <sys/resource.h>
int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--check") == 0)
        return chicago_lock_face_available() ? 0 : 1;
    if (argc != 1 || !chicago_lock_face_available()) return 1;
    struct rlimit no_core = {0, 0};
    if (setrlimit(RLIMIT_CORE, &no_core) != 0) return 1;
    struct passwd *user = getpwuid(getuid());
    return user && chicago_lock_authenticate_face(user->pw_name) ? 0 : 1;
}
