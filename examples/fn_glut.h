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
 * Window layer of the OpenGL viewers.
 *
 * With FN_USE_GLFW (the default build), the part of the GLUT API that the
 * viewers use is implemented on GLFW (fn_glut.c), which runs natively on
 * Wayland as well as X11, macOS and Windows. Without it, this is plain GLUT
 * (freeglut, or the GLUT framework on macOS).
 *
 * With GLFW, closing a window, Ctrl+C, or closing the terminal is delivered to
 * the viewer as the Esc key, so that it stops the Kinect and exits through its
 * own clean shutdown path.
 */

#pragma once

#if defined(__APPLE__) && !defined(GL_SILENCE_DEPRECATION)
#define GL_SILENCE_DEPRECATION /* OpenGL is deprecated, but still present, on macOS */
#endif

#ifdef FN_USE_GLFW

#ifdef FN_GLUT_GLU
#define GLFW_INCLUDE_GLU /* gluPerspective(), gluLookAt() */
#endif
#include <GLFW/glfw3.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GLUT_RGB           0
#define GLUT_RGBA          0
#define GLUT_DOUBLE        2
#define GLUT_ALPHA         8
#define GLUT_DEPTH         16
#define GLUT_LEFT_BUTTON   0
#define GLUT_MIDDLE_BUTTON 1
#define GLUT_RIGHT_BUTTON  2
#define GLUT_DOWN          0
#define GLUT_UP            1
#define GLUT_ELAPSED_TIME  700

void fn_glutInit(int *argc, char **argv);
void fn_glutInitDisplayMode(unsigned int mode);
void fn_glutInitWindowSize(int width, int height);
void fn_glutInitWindowPosition(int x, int y);
int  fn_glutCreateWindow(const char *title);
void fn_glutDestroyWindow(int window);
void fn_glutSetWindow(int window);
int  fn_glutGetWindow(void);
void fn_glutReshapeWindow(int width, int height);
void fn_glutDisplayFunc(void (*func)(void));
void fn_glutIdleFunc(void (*func)(void));
void fn_glutReshapeFunc(void (*func)(int width, int height));
void fn_glutKeyboardFunc(void (*func)(unsigned char key, int x, int y));
void fn_glutMouseFunc(void (*func)(int button, int state, int x, int y));
void fn_glutMotionFunc(void (*func)(int x, int y));
void fn_glutPostRedisplay(void);
void fn_glutSwapBuffers(void);
int  fn_glutGet(int what);
void fn_glutMainLoop(void);

#define glutInit             fn_glutInit
#define glutInitDisplayMode  fn_glutInitDisplayMode
#define glutInitWindowSize   fn_glutInitWindowSize
#define glutInitWindowPosition fn_glutInitWindowPosition
#define glutCreateWindow     fn_glutCreateWindow
#define glutDestroyWindow    fn_glutDestroyWindow
#define glutSetWindow        fn_glutSetWindow
#define glutGetWindow        fn_glutGetWindow
#define glutReshapeWindow    fn_glutReshapeWindow
#define glutDisplayFunc      fn_glutDisplayFunc
#define glutIdleFunc         fn_glutIdleFunc
#define glutReshapeFunc      fn_glutReshapeFunc
#define glutKeyboardFunc     fn_glutKeyboardFunc
#define glutMouseFunc        fn_glutMouseFunc
#define glutMotionFunc       fn_glutMotionFunc
#define glutPostRedisplay    fn_glutPostRedisplay
#define glutSwapBuffers      fn_glutSwapBuffers
#define glutGet              fn_glutGet
#define glutMainLoop         fn_glutMainLoop

#ifdef __cplusplus
}
#endif

#else /* GLUT */

#if defined(__APPLE__)
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#endif
