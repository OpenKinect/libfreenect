/*
 * This file is part of the OpenKinect Project. http://www.openkinect.org
 *
 * Copyright (c) 2010 individual OpenKinect contributors. See the CONTRIB file
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


#include <stdio.h>
#include <string.h>
#include <libusb.h>
#include "libfreenect.h"

#include <pthread.h>

#if defined(__APPLE__)
#include <GLUT/glut.h>
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/glut.h>
#include <GL/gl.h>
#include <GL/glu.h>
#endif

#include <math.h>

pthread_t gl_thread;
volatile int die = 0;

int g_argc;
char **g_argv;

int window;

pthread_mutex_t gl_backbuf_mutex = PTHREAD_MUTEX_INITIALIZER;

uint8_t gl_depth_front[640*480*4];
uint8_t gl_depth_back[640*480*4];

uint8_t gl_rgb_front[640*480*4];
uint8_t gl_rgb_back[640*480*4];

GLuint gl_depth_tex;
GLuint gl_rgb_tex;


pthread_cond_t gl_frame_cond = PTHREAD_COND_INITIALIZER;
int got_frames = 0;

void DrawGLScene()
{
	pthread_mutex_lock(&gl_backbuf_mutex);

	while (got_frames < 2) {
		pthread_cond_wait(&gl_frame_cond, &gl_backbuf_mutex);
	}

	memcpy(gl_depth_front, gl_depth_back, sizeof(gl_depth_back));
	memcpy(gl_rgb_front, gl_rgb_back, sizeof(gl_rgb_back));
	got_frames = 0;
	pthread_mutex_unlock(&gl_backbuf_mutex);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glLoadIdentity();

	glEnable(GL_TEXTURE_2D);

	glBindTexture(GL_TEXTURE_2D, gl_depth_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, 3, 640, 480, 0, GL_RGB, GL_UNSIGNED_BYTE, gl_depth_front);

	glBegin(GL_TRIANGLE_FAN);
	glColor4f(255.0f, 255.0f, 255.0f, 255.0f);
	glTexCoord2f(0, 0); glVertex3f(0,0,0);
	glTexCoord2f(1, 0); glVertex3f(640,0,0);
	glTexCoord2f(1, 1); glVertex3f(640,480,0);
	glTexCoord2f(0, 1); glVertex3f(0,480,0);
	glEnd();

	glBindTexture(GL_TEXTURE_2D, gl_rgb_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, 3, 640, 480, 0, GL_RGB, GL_UNSIGNED_BYTE, gl_rgb_front);

	glBegin(GL_TRIANGLE_FAN);
	glColor4f(255.0f, 255.0f, 255.0f, 255.0f);
	glTexCoord2f(0, 0); glVertex3f(640,0,0);
	glTexCoord2f(1, 0); glVertex3f(1280,0,0);
	glTexCoord2f(1, 1); glVertex3f(1280,480,0);
	glTexCoord2f(0, 1); glVertex3f(640,480,0);
	glEnd();

	glutSwapBuffers();
}

void keyPressed(unsigned char key, int x, int y)
{
	if (key == 27) {
		die = 1;
		glutDestroyWindow(window);
		pthread_exit(NULL);
	}
}

void ReSizeGLScene(int Width, int Height)
{
	glViewport(0,0,Width,Height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho (0, 1280, 480, 0, -1.0f, 1.0f);
	glMatrixMode(GL_MODELVIEW);
}

void InitGL(int Width, int Height)
{
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClearDepth(1.0);
	glDepthFunc(GL_LESS);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glShadeModel(GL_SMOOTH);
	glGenTextures(1, &gl_depth_tex);
	glBindTexture(GL_TEXTURE_2D, gl_depth_tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glGenTextures(1, &gl_rgb_tex);
	glBindTexture(GL_TEXTURE_2D, gl_rgb_tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	ReSizeGLScene(Width, Height);
}

void *gl_threadfunc(void *arg)
{
	printf("GL thread\n");

	glutInit(&g_argc, g_argv);

	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_ALPHA | GLUT_DEPTH);
	glutInitWindowSize(1280, 480);
	glutInitWindowPosition(0, 0);

	window = glutCreateWindow("LibFreenect");

	glutDisplayFunc(&DrawGLScene);
	glutIdleFunc(&DrawGLScene);
	glutReshapeFunc(&ReSizeGLScene);
	glutKeyboardFunc(&keyPressed);

	InitGL(1280, 480);

	glutMainLoop();

	pthread_exit(NULL);
	return NULL;
}

uint16_t t_gamma[2048];

void depth_cb(freenect_device *dev, freenect_depth *depth, uint32_t timestamp)
{
	int i;

	pthread_mutex_lock(&gl_backbuf_mutex);
	for (i=0; i<FREENECT_FRAME_PIX; i++) {
		int pval = t_gamma[depth[i]];
		int lb = pval & 0xff;
		switch (pval>>8) {
			case 0:
				gl_depth_back[3*i+0] = 255;
				gl_depth_back[3*i+1] = 255-lb;
				gl_depth_back[3*i+2] = 255-lb;
				break;
			case 1:
				gl_depth_back[3*i+0] = 255;
				gl_depth_back[3*i+1] = lb;
				gl_depth_back[3*i+2] = 0;
				break;
			case 2:
				gl_depth_back[3*i+0] = 255-lb;
				gl_depth_back[3*i+1] = 255;
				gl_depth_back[3*i+2] = 0;
				break;
			case 3:
				gl_depth_back[3*i+0] = 0;
				gl_depth_back[3*i+1] = 255;
				gl_depth_back[3*i+2] = lb;
				break;
			case 4:
				gl_depth_back[3*i+0] = 0;
				gl_depth_back[3*i+1] = 255-lb;
				gl_depth_back[3*i+2] = 255;
				break;
			case 5:
				gl_depth_back[3*i+0] = 0;
				gl_depth_back[3*i+1] = 0;
				gl_depth_back[3*i+2] = 255-lb;
				break;
			default:
				gl_depth_back[3*i+0] = 0;
				gl_depth_back[3*i+1] = 0;
				gl_depth_back[3*i+2] = 0;
				break;
		}
	}
	got_frames++;
	pthread_cond_signal(&gl_frame_cond);
	pthread_mutex_unlock(&gl_backbuf_mutex);
}

void rgb_cb(freenect_device *dev, freenect_pixel *rgb, uint32_t timestamp)
{
	pthread_mutex_lock(&gl_backbuf_mutex);
	got_frames++;
	memcpy(gl_rgb_back, rgb, FREENECT_RGB_SIZE);
	pthread_cond_signal(&gl_frame_cond);
	pthread_mutex_unlock(&gl_backbuf_mutex);
}

int main(int argc, char **argv)
{
	int res;
	freenect_context *f_ctx;
	freenect_device *f_dev;

	printf("Kinect camera test\n");

	int i;
	for (i=0; i<2048; i++) {
		float v = i/2048.0;
		v = powf(v, 3)* 6;
		t_gamma[i] = v*6*256;
	}
	
	g_argc = argc;
	g_argv = argv;

	if (freenect_init(&f_ctx, NULL) < 0) {
		printf("freenect_init() failed\n");
		return 1;
	}

	if (freenect_open_device(f_ctx, &f_dev, 0) < 0) {
		printf("Could not open device\n");
		return 1;
	}

	freenect_set_depth_callback(f_dev, depth_cb);
	freenect_set_rgb_callback(f_dev, rgb_cb);
	freenect_set_rgb_format(f_dev, FREENECT_FORMAT_RGB);

	res = pthread_create(&gl_thread, NULL, gl_threadfunc, NULL);
	if (res) {
		printf("pthread_create failed\n");
		return 1;
	}
	
	freenect_start_depth(f_dev);
	freenect_start_rgb(f_dev);

	while(!die && freenect_process_events(f_ctx) >= 0 );

	printf("-- done!\n");

	pthread_exit(NULL);
}
