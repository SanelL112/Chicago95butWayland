#define _POSIX_C_SOURCE 200809L
#include <security/pam_appl.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "chicago_lock_auth.h"
static const struct pam_conv *conv;
static int mode, calls, ended, sentinel, face;
int pam_start(const char *service, const char *user, const struct pam_conv *conversation,
              pam_handle_t **handle) {
    assert(strcmp(user, "alice") == 0);
    face = strcmp(service, "chicago-lock-face") == 0;
    assert(face || strcmp(service, "swaylock") == 0);
    conv = conversation;
    *handle = (mode == 1 || mode == 7) ? NULL : (pam_handle_t *)(void *)&sentinel;
    return (mode == 1 || mode == 7) ? PAM_SYSTEM_ERR : PAM_SUCCESS;
}
int pam_start_confdir(const char *service, const char *user, const struct pam_conv *conversation,
                      const char *directory, pam_handle_t **handle) {
    assert(strcmp(service, "chicago-lock-face") == 0);
    assert(directory && strstr(directory, "/../config/pam"));
    return pam_start(service, user, conversation, handle);
}
int pam_authenticate(pam_handle_t *handle, int flags) {
    assert(handle && flags == PAM_DISALLOW_NULL_AUTHTOK);
    calls++;
    struct pam_message user = { PAM_PROMPT_ECHO_ON, "User:" };
    struct pam_message pass = { PAM_PROMPT_ECHO_OFF, "Password:" };
    struct pam_message info = { PAM_TEXT_INFO, "Info" };
    const struct pam_message *messages[] = { &user, &pass, &info };
    struct pam_response *reply = NULL;
    if (face) {
        if (mode == 4) {
            const struct pam_message *visible[] = { &user, &info };
            assert(conv->conv(2, visible, &reply, conv->appdata_ptr) == PAM_SUCCESS);
            assert(strcmp(reply[0].resp, "alice") == 0 && !reply[1].resp);
            free(reply[0].resp); free(reply);
            return PAM_SUCCESS; /* Howdy succeeded without requesting a password. */
        }
        if (mode == 5) {
            assert(conv->conv(3, messages, &reply, conv->appdata_ptr) == PAM_CONV_ERR);
            assert(reply == NULL); /* No password, including an empty one, is supplied. */
            return PAM_CONV_ERR;
        }
        return PAM_AUTH_ERR;
    }
    if (mode == 3) pass.msg_style = -1;
    int result = conv->conv(3, messages, &reply, conv->appdata_ptr);
    if (mode == 3) {
        assert(result == PAM_CONV_ERR && reply == NULL);
        return result;
    }
    assert(result == PAM_SUCCESS);
    assert(strcmp(reply[0].resp, "alice") == 0);
    assert(strcmp(reply[1].resp, "p$`a\"ss") == 0);
    assert(reply[2].resp == NULL);
    free(reply[0].resp); free(reply[1].resp); free(reply);
    return mode == 2 ? PAM_AUTH_ERR : PAM_SUCCESS;
}
int pam_end(pam_handle_t *handle, int result) {
    assert(handle);
    assert(((mode == 0 || mode == 4) && result == PAM_SUCCESS) ||
           (mode != 0 && mode != 4 && result != PAM_SUCCESS));
    ended++;
    return PAM_SUCCESS;
}
int main(void) {
    assert(!chicago_lock_authenticate(NULL, "x"));
    assert(!chicago_lock_authenticate("alice", ""));
    assert(!calls && !ended);
    for (mode = 0; mode < 4; mode++) {
        int ok = chicago_lock_authenticate("alice", "p$`a\"ss");
        assert(ok == (mode == 0));
    }
    assert(calls == 3 && ended == 3);
    assert(!chicago_lock_authenticate_face(NULL));
    for (mode = 4; mode < 8; mode++)
        assert(chicago_lock_authenticate_face("alice") == (mode == 4));
    assert(calls == 6 && ended == 6);
    char secret[] = "erase me";
    chicago_lock_wipe(secret, sizeof(secret));
    for (size_t i = 0; i < sizeof(secret); i++) assert(secret[i] == 0);
    puts("PAM success, denial, setup failure, conversation failure and wipe checks passed");
    return 0;
}
