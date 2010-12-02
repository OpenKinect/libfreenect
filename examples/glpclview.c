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

#include "libfreenect.h"
#include "libfreenect_sync.h"
#include <stdio.h>

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

int window;
GLuint gl_rgb_tex;
int mx=-1,my=-1;        // Prevous mouse coordinates
int rotangles[2] = {0}; // Panning angles
float zoom = 1;         // zoom factor
int color = 1;          // Use the RGB texture or just draw it as color

// Do the projection from u,v,depth to X,Y,Z directly in an opengl matrix
void LoadVertexMatrix() 
{
  float f = 590.0f;
  float a = -0.0030711f;
  float b = 3.3309495f;
  float cx = 320.0f;
  float cy = 240.0f;
  float mat[4][4] = {
    {1/f,     0,  0, 0},
    {0,    -1/f,  0, 0},
    {0,       0,  0, a},
    {-cx/f,cy/f, -1, b}};
  glMultMatrixf(mat);
}

void mouseMoved(int x, int y)
{
  if (mx>=0 && my>=0) {
    rotangles[0] += y-my;
    rotangles[1] += x-mx;
  }
  mx = x;
  my = y;
}

void mousePress(int button, int state, int x, int y)
{
  if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
    mx = x;
    my = y;
  }
  if (button == GLUT_LEFT_BUTTON && state == GLUT_UP) {
    mx = -1;
    my = -1;
  }
}

void DrawGLScene()
{
  short *depth = 0;
  char *rgb = 0;
  int ts;
  freenect_sync_get_depth(&depth, &ts);
  freenect_sync_get_rgb(&rgb, &ts);
  
  static unsigned int indices[480][640];
  static short xyz[480][640][3];
  static float uv[480][640][2];
  int i,j;
  for (i = 0; i < 480; i++) {
    for (j = 0; j < 640; j++) {
      xyz[i][j][0] = j;
      xyz[i][j][1] = i;
      xyz[i][j][2] = depth[i*640+j];
      uv[i][j][0] = j/480.0f;
      uv[i][j][1] = i/640.0f;
      indices[i][j] = i*640+j; 
    }
  }
  
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glLoadIdentity();
	
  glPushMatrix();
  glScalef(zoom,zoom,1);
  glTranslatef(0,0,-3.5);
  glRotatef(rotangles[0], 1,0,0);
  glRotatef(rotangles[1], 0,1,0);
  glTranslatef(0,0,1.5);
  
  LoadVertexMatrix();
  glPointSize(1);

  glEnableClientState(GL_VERTEX_ARRAY);
  glVertexPointer(3, GL_SHORT, 0, xyz);
  glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  glTexCoordPointer(2, GL_FLOAT, 0, uv);
  
  if (color) glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, gl_rgb_tex);
	glTexImage2D(GL_TEXTURE_2D, 0, 3, 640, 480, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb);
		
  glDrawElements(GL_POINTS, 640*480, GL_UNSIGNED_INT, indices);
	glPopMatrix();
  glDisable(GL_TEXTURE_2D);
	
  free(rgb);
  free(depth);
  
	glutSwapBuffers();
}

void keyPressed(unsigned char key, int x, int y)
{
	if (key == 27) {
		glutDestroyWindow(window);
    exit(0);
	}
  if (key == 'w') zoom *= 1.1f;
  if (key == 's') zoom /= 1.1f;
  if (key == 'c') color = !color;
}

void ReSizeGLScene(int Width, int Height)
{
	glViewport(0,0,Width,Height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
  gluPerspective(60, 4/3., 0.3, 200);
	glMatrixMode(GL_MODELVIEW);
}

void InitGL(int Width, int Height)
{
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glEnable(GL_DEPTH_TEST);
	glGenTextures(1, &gl_rgb_tex);
	glBindTexture(GL_TEXTURE_2D, gl_rgb_tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	ReSizeGLScene(Width, Height);
}

int main(int argc, char **argv)
{
  glutInit(&argc, argv);

	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_ALPHA | GLUT_DEPTH);
  glutInitWindowSize(640, 480);
	glutInitWindowPosition(0, 0);

	window = glutCreateWindow("LibFreenect");

	glutDisplayFunc(&DrawGLScene);
	glutIdleFunc(&DrawGLScene);
	glutReshapeFunc(&ReSizeGLScene);
	glutKeyboardFunc(&keyPressed);
  glutMotionFunc(&mouseMoved);
  glutMouseFunc(&mousePress);

	InitGL(640, 480);

	glutMainLoop();

	return NULL;
}

