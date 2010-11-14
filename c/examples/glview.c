#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "libfreenect.h"
#include <pthread.h>
#include <time.h>

#if defined(WIN32)
#include <windows.h>
#include <usb.h>
#else


#endif

#include <math.h>

// COMMENT THIS LINE OUT FOR SIMPLE NON-PTHREAD/GLUT TRANSFER TEST
#define PTHREAD_AND_GLUT

#ifdef PTHREAD_AND_GLUT

#if defined(__APPLE__)
#include <GLUT/glut.h>
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#if defined(WIN32)
#include <glut.h>
#else
#include <glut.h>
#endif
#include <GL/gl.h>
#include <GL/glu.h>
#endif

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
	static int fcnt = 0;
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

		glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH | GLUT_ALPHA );
	glutInitWindowSize(1280, 480);
	glutInitWindowPosition(0, 0);

	window = glutCreateWindow("LibFreenect");

	printf("window created!\n");

	glutDisplayFunc(&DrawGLScene);
	glutIdleFunc(&DrawGLScene);
	glutReshapeFunc(&ReSizeGLScene);
	glutKeyboardFunc(&keyPressed);

	InitGL(1280, 480);

	glutMainLoop();

		printf("here!\n");

	pthread_exit(NULL);
	return NULL;
}

uint16_t t_gamma[2048];

void depthimg(uint16_t *buf, int width, int height)
{
	int i;

	pthread_mutex_lock(&gl_backbuf_mutex);
	for (i=0; i<640*480; i++) {
		int pval = t_gamma[buf[i]];
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

void rgbimg(uint8_t *buf, int width, int height)
{

	int i;

	pthread_mutex_lock(&gl_backbuf_mutex);
	memcpy(gl_rgb_back, buf, width*height*3);
	got_frames++;
	pthread_cond_signal(&gl_frame_cond);
	pthread_mutex_unlock(&gl_backbuf_mutex);
}

#endif


int main(int argc, char **argv)
{
	int res, i;
	int die = 0;
	char c;

	for (i=0; i<2048; i++) {	t_gamma[i] = powf(i/2048.0,3)*6*256;	}

	if(init_camera_device() != FREENECT_OK)
	{
			printf("Error, couldn't open the camera device.\n");
			return -1;
	}

	#if defined(PTHREAD_AND_GLUT)
		g_argc = argc;
		g_argv = argv;

		res = pthread_create(&gl_thread, NULL, gl_threadfunc, NULL);
		if (res) {
			printf("pthread_create failed\n");
			return 1;
		}
	#endif
	#if defined(WIN32)
	Sleep(3000);
	#endif


	start_camera_device();
	#if defined(PTHREAD_AND_GLUT)
	prep_iso_transfers(depthimg, rgbimg);
	#else
	prep_iso_transfers(NULL, NULL);
	#endif

		while( die == 0 ){

			//scanf("%c", &c);
			update_isochronous_async();
		}

		printf("-- done!\n");

	#if defined(PTHREAD_AND_GLUT)
		pthread_exit(NULL);
	#endif

	return 0;
}

