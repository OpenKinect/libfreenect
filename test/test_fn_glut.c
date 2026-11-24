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

/* Runtime check of examples/fn_glut.c (GLUT on GLFW, POSIX): window,
 * display, reshape, idle; Ctrl+C delivered as Esc; the watchdog. No Kinect.
 *   test_fn_glut normal   Ctrl+C -> Esc -> the viewer exits: exit code 0
 *   test_fn_glut blocked  the viewer is stuck after Ctrl+C: the watchdog
 *                         ends it with SIGALRM about 10 s later
 * Exits with 77 (skipped) when there is no display. */
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include "fn_glut.h"

static const char *mode;
static int frames = 0, reshapes = 0, sent = 0;
static int start_ms;

static void display(void)
{
	glClearColor(0.1f, 0.3f, 0.5f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glutSwapBuffers();
	frames++;
}

static void reshape(int w, int h)
{
	reshapes++;
	glViewport(0, 0, w, h);
	printf("reshape %dx%d\n", w, h);
}

static void idle(void)
{
	glutPostRedisplay();
	if (!sent && glutGet(GLUT_ELAPSED_TIME) - start_ms > 800) {
		sent = 1;
		printf("after %d frames: sending SIGINT (as Ctrl+C)\n", frames);
		fflush(stdout);
		raise(SIGINT);
		if (strcmp(mode, "blocked") == 0) {
			printf("idle blocks forever now\n");
			fflush(stdout);
			for (;;) sleep(1);
		}
	}
}

static void keyboard(unsigned char key, int x, int y)
{
	(void)x; (void)y;
	if (key == 27) {
		printf("Esc received (frames %d, reshapes %d)\n", frames, reshapes);
		fflush(stdout);
		if (strcmp(mode, "normal") == 0)
			exit(0);
		printf("ignoring it\n");
		fflush(stdout);
	}
}

int main(int argc, char **argv)
{
	mode = argc > 1 ? argv[1] : "normal";
	if (!getenv("DISPLAY") && !getenv("WAYLAND_DISPLAY")) {
		printf("no display: skipped\n");
		return 77;
	}
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
	glutInitWindowSize(200, 150);
	glutInitWindowPosition(0, 0);
	glutCreateWindow("fn_glut test");
#if GLFW_VERSION_MAJOR > 3 || (GLFW_VERSION_MAJOR == 3 && GLFW_VERSION_MINOR >= 4)
	printf("GLFW %s, platform %s\n", glfwGetVersionString(),
	       glfwGetPlatform() == GLFW_PLATFORM_WAYLAND ? "Wayland" : glfwGetPlatform() == GLFW_PLATFORM_X11 ? "X11" : "other");
#endif
	glutDisplayFunc(display);
	glutReshapeFunc(reshape);
	glutIdleFunc(idle);
	glutKeyboardFunc(keyboard);
	start_ms = glutGet(GLUT_ELAPSED_TIME);
	glutMainLoop();
	return 0;
}
