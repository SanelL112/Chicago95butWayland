#ifndef CHICAGO_LOCK_READY_H
#define CHICAGO_LOCK_READY_H
#include <errno.h>
#include <poll.h>
#include <unistd.h>
/* A success byte is sent only by the compositor's locked callback. */
#define CHICAGO_LOCK_READY 'L'
#define CHICAGO_LOCK_UNSUPPORTED 'U'
#define CHICAGO_LOCK_FAILED 'F'
static inline char chicago_lock_wait_ready(int fd, int timeout_ms) {
    struct pollfd watched = { .fd = fd, .events = POLLIN };
    int status;
    do { status = poll(&watched, 1, timeout_ms); } while (status < 0 && errno == EINTR);
    if (status > 0 && (watched.revents & (POLLIN | POLLHUP))) {
        char byte = CHICAGO_LOCK_FAILED;
        ssize_t n;
        do { n = read(fd, &byte, 1); } while (n < 0 && errno == EINTR);
        if (n == 1 && (byte == CHICAGO_LOCK_READY || byte == CHICAGO_LOCK_UNSUPPORTED))
            return byte;
    }
    return CHICAGO_LOCK_FAILED;
}
#endif
