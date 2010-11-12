
#include <stdio.h>
#include <string.h>
#include <usb.h>
#include <stdint.h>

#include "libfreenect.h"


#include <pthread.h>

//#include <glut.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <math.h>
#include <Windows.h>

#ifdef PTHREAD_AND_GLUT
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

void depthimg(uint16_t *buf, int width, int height)
{
/*	FILE *f = fopen("depth.bin", "w");
	fwrite(depth_frame, 640*480, 2, f);
	fclose(f);*/

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
	int res;

	if(InitMotorDevice() != FREENECT_OK)
	{
			printf("Error, couldn't open the motor device.\n");
			return -1;
	}

	// NOTE : the return of those functions won't be OK but it will actually work - have to check why the underlying libusb functions fail although they do the job
	if(SetLED(Green) != FREENECT_OK)
	{
		printf("Error, couldn't write the LED status.\n");
	}

	if(SetMotorTilt(128) != FREENECT_OK)
	{
		printf("Error, couldn't write the motor status.\n");
	}

	Sleep(3000);

	if(SetMotorTilt(255) != FREENECT_OK)
	{
		printf("Error, couldn't write the motor status.\n");
	}

	Sleep(3000);

	if(SetMotorTilt(0) != FREENECT_OK)
	{
		printf("Error, couldn't write the motor status.\n");
	}

	if(CloseMotorDevice() != FREENECT_OK)
	{
		printf("Error, couldn't close the motor device.\n");
		return -1;
	}

	return 0;
}