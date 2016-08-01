#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <errno.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include "sslock.h"

#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

#define SSLOCK_MAX_PWD_LEN 100

// Service name sent to PAM when starting session.
// su is good enough, asks for a password for all users except root.
static const char* SSLOCK_PAM_SRV_NAME = "su";

// Abort program execution with the given error message
void sslock_fail(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	// TODO: beep to signal user that lock has failed.
	exit(1);
}

// Log handler only enabled when DEBUG is on
void sslock_log(const char* fmt, ...)
{
#ifdef DEBUG
	char buffer[23];
	time_t unix_now = time(0);
	struct tm *utc_now = gmtime(&unix_now);
	strftime(buffer, sizeof(buffer), "[%d/%m/%Y %H:%M:%S] ", utc_now);
	printf("%s", buffer);

	va_list args;
	va_start(args, fmt);
	vfprintf(stdout, fmt, args);
	va_end(args);
	printf("\n");
#else
	UNUSED(fmt);
#endif
}

// Actual lock and event handling function
void sslock_do_lock(const char* owner_login)
{
	char keypress_buffer[SSLOCK_MAX_PWD_LEN];
	unsigned int keypress_count = 0;

	Display *display = XOpenDisplay(NULL);
	if (display == NULL)
		sslock_fail("Error: could not grab display");

	Window root = DefaultRootWindow(display);
	sslock_log("Locking keyboard and mouse");
	XGrabPointer(display, root, 1, ButtonPress, GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
	XGrabKeyboard(display, root, 0, GrabModeAsync, GrabModeAsync, CurrentTime);
	XSelectInput(display, root, KeyPressMask);

	XEvent ev;
	while (XNextEvent(display, &ev), 1) {
		if (ev.type != KeyPress)
			continue;

		KeySym keysym;
		XComposeStatus compose;
		if (keypress_count > sizeof(keypress_buffer)-2)
			keypress_count = 0;
		XLookupString(&ev.xkey, keypress_buffer + keypress_count, 10, &keysym, &compose);
		keypress_buffer[++keypress_count] = 0;
		sslock_log("Received key, current buffer: '%s'", keypress_buffer);
		if (keysym == XK_Return) {
			if (sslock_authenticate(owner_login, keypress_buffer))
				break;
			else
				sslock_log("Authentication failed");
			keypress_count = 0;
		}
	}

	sslock_log("Unlocking keyboard and mouse");
	XUngrabKeyboard(display, CurrentTime);
	XUngrabPointer(display, CurrentTime);
	XCloseDisplay(display);

	// Dispose of the password buffer securely
	for(volatile int i = 0; i < SSLOCK_MAX_PWD_LEN; i++)
		keypress_buffer[i] = 0;
}

// Called by PAM
int sslock_pam_callback(int num_msg, const struct pam_message **msgs,
                       struct pam_response **resps, void *arg)
{
	if (num_msg < 1 || resps == NULL)
		return PAM_CONV_ERR;

	*resps = (struct pam_response*)calloc(sizeof(struct pam_response), num_msg);
	if (*resps == NULL)
		sslock_fail("Error: out of memory\n");

	for (int msg_idx = 0; msg_idx < num_msg; msg_idx++) {
		const struct pam_message *msg = msgs[msg_idx];
		struct pam_response *resp = &(*resps[msg_idx]);
		char* password = (char*)arg;
		resp->resp_retcode = 0;
		switch(msg->msg_style) {
		case PAM_PROMPT_ECHO_OFF:
		case PAM_PROMPT_ECHO_ON:
			resp->resp = (password == NULL || strlen(password) == 0 ? "" : password);
			sslock_log("PAM prompted for text with prompt '%s', input '%s'",
				(msg->msg_style == PAM_PROMPT_ECHO_ON ? msg->msg : NULL),
				resp->resp);
			break;
		case PAM_ERROR_MSG:
			sslock_log("PAM sent an error message: '%s'", msg->msg);
			break;
		case PAM_TEXT_INFO:
			sslock_log("PAM sent an info message: '%s'", msg->msg);
			break;
		default:
			sslock_fail("Error: unknown PAM message style %d\n",
				msg->msg_style);
		}
	}
	return PAM_SUCCESS;
}

int sslock_authenticate(const char* login, const char* password)
{
	pam_handle_t *pamh;
	sslock_log("Starting new PAM session as %s for user %s", SSLOCK_PAM_SRV_NAME, login);

	struct pam_conv sslock_conversation = { &sslock_pam_callback, (void*)password };
	int res = pam_start(SSLOCK_PAM_SRV_NAME, login, &sslock_conversation, &pamh);
	if (res != PAM_SUCCESS)
		sslock_fail("Error: unable to start PAM session (error %d)", res);

	sslock_log("Asking PAM to authenticate user");
	res = pam_authenticate(pamh, 0);
	if (res != PAM_SUCCESS) {
		sslock_log("Authentication failure (code %d)", res);
		goto close_session;
	}

	sslock_log("Asking PAM if account is authorized to login");
	res = pam_acct_mgmt(pamh, PAM_DISALLOW_NULL_AUTHTOK);
	if (res != PAM_SUCCESS) {
		sslock_log("Authentication failure, unable to refresh authentication (code %d)", res);
		goto close_session;
	}

close_session: ; // cleanup
	int end_res = pam_end(pamh, res);
	if (end_res != PAM_SUCCESS)
		sslock_log("Error: unable to end PAM session, code %d", end_res);
	return (res == PAM_SUCCESS);
}

int main()
{
	uid_t uid = getuid();
	if (uid == 0)
		sslock_fail("Please don't leave your screen locked from a root shell.");

	struct passwd *pw = getpwuid(uid);
	if (pw == NULL)
		sslock_fail("Error: could not find user with getpwuid(%d)", uid);

	sslock_do_lock(pw->pw_name);

	return 0;
}
