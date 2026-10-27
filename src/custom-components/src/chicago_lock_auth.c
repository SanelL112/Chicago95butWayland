#define _POSIX_C_SOURCE 200809L
#include "chicago_lock_auth.h"
#include <security/pam_appl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

typedef struct { const char *username; const char *password; } Credentials;

void chicago_lock_wipe(void *memory, size_t length) {
    volatile unsigned char *p = memory;
    while (length--) *p++ = 0;
}

static int conversation(int count, const struct pam_message **messages,
                        struct pam_response **responses, void *data) {
    Credentials *credentials = data;
    if (!responses || !messages || count <= 0 || count > PAM_MAX_NUM_MSG)
        return PAM_CONV_ERR;
    *responses = NULL;
    struct pam_response *reply = calloc((size_t)count, sizeof(*reply));
    if (!reply) return PAM_BUF_ERR;
    for (int i = 0; i < count; i++) {
        if (!messages[i]) goto failed;
        switch (messages[i]->msg_style) {
        case PAM_PROMPT_ECHO_OFF:
            /* Face-only transactions may never supply an empty/fallback password. */
            if (!credentials->password) goto failed;
            reply[i].resp = strdup(credentials->password);
            if (!reply[i].resp) goto failed;
            break;
        case PAM_PROMPT_ECHO_ON:
            reply[i].resp = strdup(credentials->username);
            if (!reply[i].resp) goto failed;
            break;
        case PAM_ERROR_MSG:
        case PAM_TEXT_INFO:
            break;
        default:
            goto failed;
        }
    }
    *responses = reply; /* PAM owns the responses after a successful conversation. */
    return PAM_SUCCESS;
failed:
    for (int i = 0; i < count; i++) {
        if (reply[i].resp) {
            chicago_lock_wipe(reply[i].resp, strlen(reply[i].resp));
            free(reply[i].resp);
        }
    }
    free(reply);
    return PAM_CONV_ERR;
}

static int authenticate(const char *service, const char *username, const char *password,
                        const char *confdir) {
    Credentials credentials = { username, password };
    struct pam_conv conv = { conversation, &credentials };
    pam_handle_t *handle = NULL;
    int result = confdir ? pam_start_confdir(service, username, &conv, confdir, &handle)
                         : pam_start(service, username, &conv, &handle);
    if (result == PAM_SUCCESS)
        result = pam_authenticate(handle, PAM_DISALLOW_NULL_AUTHTOK);
    if (handle) pam_end(handle, result);
    return result == PAM_SUCCESS;
}

int chicago_lock_authenticate(const char *username, const char *password) {
    if (!username || !*username || !password || !*password) return 0;
    return authenticate("swaylock", username, password, NULL);
}

static char *face_config_directory(void) {
    char executable[4096];
    ssize_t n = readlink("/proc/self/exe", executable, sizeof(executable) - 1);
    if (n <= 0 || (size_t)n >= sizeof(executable) - 1) return NULL;
    executable[n] = '\0';
    char *slash = strrchr(executable, '/');
    if (!slash) return NULL;
    *slash = '\0';
    const char suffix[] = "/../config/pam";
    size_t length = strlen(executable) + sizeof(suffix);
    char *directory = malloc(length);
    if (directory) snprintf(directory, length, "%s%s", executable, suffix);
    return directory;
}

int chicago_lock_face_available(void) {
    if (access("/usr/local/bin/howdy-pam", X_OK) != 0) return 0;
    char *directory = face_config_directory();
    if (!directory) return 0;
    const char suffix[] = "/chicago-lock-face";
    size_t length = strlen(directory) + sizeof(suffix);
    char *policy = malloc(length);
    int available = 0;
    if (policy) {
        snprintf(policy, length, "%s%s", directory, suffix);
        available = access(policy, R_OK) == 0;
        free(policy);
    }
    free(directory);
    return available;
}

int chicago_lock_authenticate_face(const char *username) {
    if (!username || !*username) return 0;
    char *directory = face_config_directory();
    if (!directory) return 0;
    /* Only Howdy participates in this transaction. No password/failure-counting
     * fallback from the system login service is included. */
    int result = authenticate("chicago-lock-face", username, NULL, directory);
    free(directory);
    return result;
}
