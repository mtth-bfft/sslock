#include <security/pam_appl.h>
#include <security/pam_misc.h>

void sslock_fail(const char* fmt, ...);
void sslock_log(const char* fmt, ...);

int sslock_pam_callback(int num_msg, const struct pam_message **msg,
                       struct pam_response **resp, void *unused);

int sslock_authenticate();

