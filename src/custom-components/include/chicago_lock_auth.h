#ifndef CHICAGO_LOCK_AUTH_H
#define CHICAGO_LOCK_AUTH_H
#include <stddef.h>
/* Fixed PAM service; never selected by an environment variable or UI field. */
int chicago_lock_authenticate(const char *username, const char *password);
int chicago_lock_face_available(void);
int chicago_lock_authenticate_face(const char *username);
void chicago_lock_wipe(void *memory, size_t length);
#endif
