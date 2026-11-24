/*
 * This file is part of the OpenKinect Project. http://www.openkinect.org
 *
 * Copyright (c) 2026 individual OpenKinect contributors. See the CONTRIB file
 * for details.
 *
 * This code is licensed to you under the terms of the Apache License, version
 * 2.0, or, at your option, the terms of the GNU General Public License,
 * version 2.0. See the APACHE20 and GPL2 files for the text of the licenses,
 * or the following URLs:
 * http://www.apache.org/licenses/LICENSE-2.0
 * http://www.gnu.org/licenses/gpl-2.0.txt
 *
 * If you redistribute this file in source form, modified or unmodified, you
 * may:
 *   1) Leave this header intact and distribute it under the same terms,
 *      accompanying it with the APACHE20 and GPL20 files, or
 *   2) Delete the Apache 2.0 clause and accompany it with the GPL2 file, or
 *   3) Delete the GPL v2 clause and accompany it with the APACHE20 file
 * In all cases you must keep the copyright notice intact and include a copy
 * of the CONTRIB file.
 *
 * Binary distributions must follow the binary distribution requirements of
 * either License.
 */

/*
 * The GLUT calls used by the viewers, on GLFW (see fn_glut.h). Semantics
 * follow GLUT: callbacks belong to the current window, display is called
 * after glutPostRedisplay() and when the window needs repainting, reshape
 * gets the drawable size, keyboard gets ASCII characters (Esc = 27).
 */

#ifndef _WIN32
#define _DEFAULT_SOURCE /* alarm, _exit */
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#ifdef _WIN32
#include <windows.h> /* before GLFW, which otherwise defines APIENTRY itself */
#else
#include <unistd.h>
#endif
#include "fn_glut.h"

#define MAX_WINDOWS 8
#define EXIT_GRACE_S 10 /* a close request ends the process after this, whatever the viewer does; stopping two streams can take a few seconds on Windows */

typedef struct {
	GLFWwindow *w;
	void (*display)(void);
	void (*reshape)(int, int);
	void (*keyboard)(unsigned char, int, int);
	void (*mouse)(int, int, int, int);
	void (*motion)(int, int);
	int redisplay;
	int reshape_pending;
	int buttons_down;
} fn_window;

static fn_window windows[MAX_WINDOWS];
static int current = 0; /* GLUT window id, 1-based; 0 = none */
static void (*idle_func)(void) = NULL;
static int next_w = 300, next_h = 300, next_x = -1, next_y = -1;
static volatile sig_atomic_t exit_requested = 0;

/* Close requests (window closed, Ctrl+C, terminal closed) reach the viewer as
 * Esc, its normal way out. A viewer can be blocked, for example waiting for a
 * frame from a Kinect that was unplugged, so a watchdog ends the process
 * EXIT_GRACE_S seconds later in any case; a second Ctrl+C ends it at once. */
#ifdef _WIN32
static DWORD WINAPI watchdog_thread(LPVOID arg)
{
	(void)arg;
	Sleep(EXIT_GRACE_S * 1000);
	fprintf(stderr, "The viewer did not exit within %d s of the close request; ending it\n", EXIT_GRACE_S);
	ExitProcess(1);
	return 0;
}

static void arm_watchdog(void)
{
	static LONG armed = 0;
	HANDLE t;
	if (InterlockedExchange(&armed, 1) != 0)
		return;
	t = CreateThread(NULL, 0, watchdog_thread, NULL, 0, NULL);
	if (t)
		CloseHandle(t);
}

static BOOL WINAPI console_handler(DWORD type)
{
	switch (type) {
	case CTRL_C_EVENT:
	case CTRL_BREAK_EVENT:
		if (exit_requested)
			ExitProcess(130);
		exit_requested = 1;
		arm_watchdog();
		return TRUE;
	case CTRL_CLOSE_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_SHUTDOWN_EVENT:
		/* Returning would end the process at once: wait here until the viewer
		 * exits, or the watchdog ends it. */
		exit_requested = 1;
		arm_watchdog();
		Sleep((EXIT_GRACE_S + 1) * 1000);
		return TRUE;
	default:
		return FALSE;
	}
}

static void install_exit_handlers(void)
{
	SetConsoleCtrlHandler(console_handler, TRUE);
}
#else
static void arm_watchdog(void)
{
	alarm(EXIT_GRACE_S); /* SIGALRM ends the process */
}

static void signal_handler(int sig)
{
	(void)sig;
	if (exit_requested)
		_exit(130);
	exit_requested = 1;
	arm_watchdog();
}

static void install_exit_handlers(void)
{
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGHUP, signal_handler);
	signal(SIGQUIT, signal_handler);
	signal(SIGALRM, SIG_DFL);
}
#endif

static fn_window *win(int id)
{
	return (id >= 1 && id <= MAX_WINDOWS && windows[id - 1].w) ? &windows[id - 1] : NULL;
}

static int id_of(GLFWwindow *w)
{
	return (int)(size_t)glfwGetWindowUserPointer(w);
}

static void make_current(int id)
{
	fn_window *fw = win(id);
	current = id;
	if (fw)
		glfwMakeContextCurrent(fw->w);
}

static void error_cb(int code, const char *msg)
{
#ifdef GLFW_FEATURE_UNAVAILABLE
	if (code == GLFW_FEATURE_UNAVAILABLE)
		return; /* e.g. window positions on Wayland: GLUT ignores those too */
#endif
	fprintf(stderr, "GLFW error %d: %s\n", code, msg);
}

static void deliver_key(int id, unsigned char key)
{
	fn_window *fw = win(id);
	double x = 0, y = 0;
	if (!fw || !fw->keyboard)
		return;
	glfwGetCursorPos(fw->w, &x, &y);
	make_current(id);
	fw->keyboard(key, (int)x, (int)y);
}

/* Window closed, Ctrl+C, terminal closed: ask the viewer to quit as if Esc
 * was pressed, so that it stops the Kinect first. */
static void request_close(int id)
{
	int i;
	arm_watchdog();
	if (!win(id) || !win(id)->keyboard) {
		for (i = 1; i <= MAX_WINDOWS; i++)
			if (win(i) && win(i)->keyboard) { id = i; break; }
	}
	if (win(id) && win(id)->keyboard)
		deliver_key(id, 27);
	else
		exit(0);
}

static void char_cb(GLFWwindow *w, unsigned int codepoint)
{
	if (codepoint < 128)
		deliver_key(id_of(w), (unsigned char)codepoint);
}

static void key_cb(GLFWwindow *w, int key, int scancode, int action, int mods)
{
	(void)scancode; (void)mods;
	if (action == GLFW_RELEASE)
		return;
	switch (key) { /* keys with no character, but an ASCII code in GLUT */
	case GLFW_KEY_ESCAPE:    deliver_key(id_of(w), 27);  break;
	case GLFW_KEY_ENTER:
	case GLFW_KEY_KP_ENTER:  deliver_key(id_of(w), 13);  break;
	case GLFW_KEY_TAB:       deliver_key(id_of(w), 9);   break;
	case GLFW_KEY_BACKSPACE: deliver_key(id_of(w), 8);   break;
	case GLFW_KEY_DELETE:    deliver_key(id_of(w), 127); break;
	default: break;
	}
}

static void mouse_button_cb(GLFWwindow *w, int button, int action, int mods)
{
	int id = id_of(w), glut_button;
	fn_window *fw = win(id);
	double x, y;
	(void)mods;
	if (!fw)
		return;
	if (action == GLFW_PRESS) fw->buttons_down++;
	else if (fw->buttons_down > 0) fw->buttons_down--;
	if (!fw->mouse)
		return;
	switch (button) {
	case GLFW_MOUSE_BUTTON_LEFT:   glut_button = GLUT_LEFT_BUTTON; break;
	case GLFW_MOUSE_BUTTON_MIDDLE: glut_button = GLUT_MIDDLE_BUTTON; break;
	case GLFW_MOUSE_BUTTON_RIGHT:  glut_button = GLUT_RIGHT_BUTTON; break;
	default: return;
	}
	glfwGetCursorPos(w, &x, &y);
	make_current(id);
	fw->mouse(glut_button, action == GLFW_PRESS ? GLUT_DOWN : GLUT_UP, (int)x, (int)y);
}

static void scroll_cb(GLFWwindow *w, double dx, double dy)
{
	/* freeglut reports the wheel as buttons 3 (up) and 4 (down) */
	int id = id_of(w);
	fn_window *fw = win(id);
	double x, y;
	(void)dx;
	if (!fw || !fw->mouse || dy == 0)
		return;
	glfwGetCursorPos(w, &x, &y);
	make_current(id);
	fw->mouse(dy > 0 ? 3 : 4, GLUT_DOWN, (int)x, (int)y);
	fw->mouse(dy > 0 ? 3 : 4, GLUT_UP, (int)x, (int)y);
}

static void cursor_cb(GLFWwindow *w, double x, double y)
{
	int id = id_of(w);
	fn_window *fw = win(id);
	if (!fw || !fw->motion || fw->buttons_down == 0) /* GLUT motion = dragging */
		return;
	make_current(id);
	fw->motion((int)x, (int)y);
}

static void framebuffer_cb(GLFWwindow *w, int width, int height)
{
	fn_window *fw = win(id_of(w));
	(void)width; (void)height;
	if (fw) {
		fw->reshape_pending = 1;
		fw->redisplay = 1;
	}
}

static void refresh_cb(GLFWwindow *w)
{
	fn_window *fw = win(id_of(w));
	if (fw)
		fw->redisplay = 1;
}

static void close_cb(GLFWwindow *w)
{
	glfwSetWindowShouldClose(w, GLFW_FALSE); /* the viewer decides */
	request_close(id_of(w));
}

void fn_glutInit(int *argc, char **argv)
{
	(void)argc; (void)argv;
	glfwSetErrorCallback(error_cb);
#if GLFW_VERSION_MAJOR > 3 || (GLFW_VERSION_MAJOR == 3 && GLFW_VERSION_MINOR >= 4)
	{
		/* LIBFREENECT_GLFW_PLATFORM=x11 or =wayland picks the window system on
		 * Linux (GLFW prefers Wayland when both are there). */
		const char *platform = getenv("LIBFREENECT_GLFW_PLATFORM");
		if (platform && strcmp(platform, "x11") == 0)
			glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
		else if (platform && strcmp(platform, "wayland") == 0)
			glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);
	}
#endif
	if (!glfwInit()) {
		fprintf(stderr, "Cannot open a window: GLFW could not be initialised (no display?)\n");
		exit(1);
	}
	install_exit_handlers(); /* Ctrl+C, terminal closed: see request_close() */
}

void fn_glutInitDisplayMode(unsigned int mode)
{
	glfwWindowHint(GLFW_DOUBLEBUFFER, (mode & GLUT_DOUBLE) ? GLFW_TRUE : GLFW_FALSE);
	glfwWindowHint(GLFW_DEPTH_BITS, (mode & GLUT_DEPTH) ? 24 : 0);
	glfwWindowHint(GLFW_ALPHA_BITS, (mode & GLUT_ALPHA) ? 8 : 0);
}

void fn_glutInitWindowSize(int width, int height)
{
	next_w = width;
	next_h = height;
}

void fn_glutInitWindowPosition(int x, int y)
{
	next_x = x;
	next_y = y;
}

int fn_glutCreateWindow(const char *title)
{
	int i;
	for (i = 0; i < MAX_WINDOWS && windows[i].w; i++)
		;
	if (i == MAX_WINDOWS) {
		fprintf(stderr, "Too many windows\n");
		exit(1);
	}
	windows[i].w = glfwCreateWindow(next_w, next_h, title, NULL, NULL);
	if (!windows[i].w) {
		fprintf(stderr, "Cannot open a window \"%s\"\n", title);
		exit(1);
	}
	{
		/* Registered after the first window, so that it runs before the exit
		 * handlers that the GL driver installed while creating it. */
		static int registered = 0;
		if (!registered) {
			registered = 1;
			atexit(glfwTerminate);
		}
	}
	glfwSetWindowUserPointer(windows[i].w, (void *)(size_t)(i + 1));
	/* glutInitWindowPosition() is deliberately not applied: the window system
	 * places the window, which keeps its title bar on the screen. (GLUT
	 * positions a window's frame, GLFW its drawing area; at the viewers'
	 * (0, 0) the title bar ended up off the screen on Windows, also after
	 * offsetting by the frame size. Wayland ignores positions anyway.) */
	(void)next_x; (void)next_y;
	glfwSetCharCallback(windows[i].w, char_cb);
	glfwSetKeyCallback(windows[i].w, key_cb);
	glfwSetMouseButtonCallback(windows[i].w, mouse_button_cb);
	glfwSetScrollCallback(windows[i].w, scroll_cb);
	glfwSetCursorPosCallback(windows[i].w, cursor_cb);
	glfwSetFramebufferSizeCallback(windows[i].w, framebuffer_cb);
	glfwSetWindowRefreshCallback(windows[i].w, refresh_cb);
	glfwSetWindowCloseCallback(windows[i].w, close_cb);
	windows[i].redisplay = 1;       /* GLUT draws and reshapes a new window */
	windows[i].reshape_pending = 1;
	make_current(i + 1);
	glfwSwapInterval(0);
	return i + 1;
}

void fn_glutDestroyWindow(int id)
{
	fn_window *fw = win(id);
	if (!fw)
		return;
	glfwDestroyWindow(fw->w);
	memset(fw, 0, sizeof(*fw));
	if (current == id)
		current = 0;
}

void fn_glutSetWindow(int id)
{
	if (win(id))
		make_current(id);
}

int fn_glutGetWindow(void)
{
	return current;
}

void fn_glutReshapeWindow(int width, int height)
{
	if (win(current))
		glfwSetWindowSize(win(current)->w, width, height);
}

void fn_glutDisplayFunc(void (*func)(void))     { if (win(current)) win(current)->display = func; }
void fn_glutIdleFunc(void (*func)(void))        { idle_func = func; }
void fn_glutReshapeFunc(void (*func)(int, int)) { if (win(current)) win(current)->reshape = func; }
void fn_glutKeyboardFunc(void (*func)(unsigned char, int, int)) { if (win(current)) win(current)->keyboard = func; }
void fn_glutMouseFunc(void (*func)(int, int, int, int)) { if (win(current)) win(current)->mouse = func; }
void fn_glutMotionFunc(void (*func)(int, int))  { if (win(current)) win(current)->motion = func; }

void fn_glutPostRedisplay(void)
{
	if (win(current))
		win(current)->redisplay = 1;
}

void fn_glutSwapBuffers(void)
{
	if (win(current))
		glfwSwapBuffers(win(current)->w);
}

int fn_glutGet(int what)
{
	if (what == GLUT_ELAPSED_TIME)
		return (int)(glfwGetTime() * 1000.0);
	return 0;
}

void fn_glutMainLoop(void)
{
	int i;
	for (;;) {
		if (exit_requested == 1) {
			exit_requested = 2; /* delivered; a second Ctrl+C still ends the process */
			request_close(current);
		}

		if (idle_func)
			glfwPollEvents();
		else
			glfwWaitEventsTimeout(0.05); /* nothing to do until an event (or a close request) */

		if (idle_func) {
			int saved = current;
			idle_func();
			if (win(saved))
				make_current(saved);
		}

		for (i = 1; i <= MAX_WINDOWS; i++) {
			fn_window *fw = win(i);
			if (!fw)
				continue;
			if (fw->reshape_pending) {
				int width, height;
				fw->reshape_pending = 0;
				glfwGetFramebufferSize(fw->w, &width, &height);
				make_current(i);
				if (fw->reshape)
					fw->reshape(width, height);
				else
					glViewport(0, 0, width, height);
			}
			fw = win(i); /* a callback may have destroyed it */
			if (fw && fw->redisplay && fw->display) {
				fw->redisplay = 0;
				make_current(i);
				fw->display();
			}
		}
	}
}
