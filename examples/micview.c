/*
 * This file is part of the OpenKinect Project. http://www.openkinect.org
 *
 * Copyright (c) 2011 individual OpenKinect contributors. See the CONTRIB file
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
#include "libfreenect-audio.h"
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <GL/glut.h>

pthread_t freenect_thread;
volatile int die = 0;

static freenect_context* f_ctx;
static freenect_device* f_dev;

typedef struct {
	int32_t* buffers[4];
	int max_samples;
	int current_idx;  // index to the oldest data in the buffer (equivalently, where the next new data will be placed)
	int new_data;
} capture;

capture state;

int paused = 0;

pthread_mutex_t audiobuf_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t audiobuf_cond = PTHREAD_COND_INITIALIZER;

int win_h, win_w;

int have_opengl_vbo = 0;
GLint opengl_buffer[2];
GLint opengl_projection_matrix_location;
GLint opengl_microphone_number_location;


void in_callback(freenect_device* dev, int num_samples,
                 int32_t* mic1, int32_t* mic2,
                 int32_t* mic3, int32_t* mic4,
                 int16_t* cancelled, void *unknown) {
	pthread_mutex_lock(&audiobuf_mutex);
	capture* c = (capture*)freenect_get_user(dev);
	if(num_samples < c->max_samples - c->current_idx) {
		memcpy(&(c->buffers[0][c->current_idx]), mic1, num_samples*sizeof(int32_t));
		memcpy(&(c->buffers[1][c->current_idx]), mic2, num_samples*sizeof(int32_t));
		memcpy(&(c->buffers[2][c->current_idx]), mic3, num_samples*sizeof(int32_t));
		memcpy(&(c->buffers[3][c->current_idx]), mic4, num_samples*sizeof(int32_t));
	} else {
		int first = c->max_samples - c->current_idx;
		int left = num_samples - first;
		memcpy(&(c->buffers[0][c->current_idx]), mic1, first*sizeof(int32_t));
		memcpy(&(c->buffers[1][c->current_idx]), mic2, first*sizeof(int32_t));
		memcpy(&(c->buffers[2][c->current_idx]), mic3, first*sizeof(int32_t));
		memcpy(&(c->buffers[3][c->current_idx]), mic4, first*sizeof(int32_t));
		memcpy(c->buffers[0], &mic1[first], left*sizeof(int32_t));
		memcpy(c->buffers[1], &mic2[first], left*sizeof(int32_t));
		memcpy(c->buffers[2], &mic3[first], left*sizeof(int32_t));
		memcpy(c->buffers[3], &mic4[first], left*sizeof(int32_t));
	}
	c->current_idx = (c->current_idx + num_samples) % c->max_samples;
	c->new_data = 1;
	pthread_cond_signal(&audiobuf_cond);
	pthread_mutex_unlock(&audiobuf_mutex);
}

void* freenect_threadfunc(void* arg) {
	while(!die && freenect_process_events(f_ctx) >= 0) {
		// If we did anything else in the freenect thread, it might go here.
	}
	freenect_stop_audio(f_dev);
	freenect_close_device(f_dev);
	freenect_shutdown(f_ctx);
	return NULL;
}

void DrawMicData() {
	if (paused)
		return;
	pthread_mutex_lock(&audiobuf_mutex);
	while(!state.new_data)
		pthread_cond_wait(&audiobuf_cond, &audiobuf_mutex);
	state.new_data = 0;
	// Draw:
	glClear(GL_COLOR_BUFFER_BIT);
	if (!have_opengl_vbo) {
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
	}

	float xIncr = (float)win_w / state.max_samples;
	float x = 0.;
	int i;
	int base_idx = state.current_idx;

	// Technically, we should hold the lock until we're done actually drawing
	// the lines, but this is sufficient to ensure that the drawings align
	// provided we don't reallocate buffers.
	pthread_mutex_unlock(&audiobuf_mutex);

	// We use either fast OpenGL Buffer Objects (VBO) where hardware
	// supports it, or slow old-style glBegin/glVertex/glEnd 
	int mic;
	for(mic = 0; mic < 4; mic++) {
		if (have_opengl_vbo) {
#if GL_VERSION_2_0
			glUniform1i(opengl_microphone_number_location, mic);
#elif GL_ARB_shader_objects
			glUniform1iARB(opengl_microphone_number_location, mic);
#endif
#if GL_VERSION_1_5
			glBindBuffer(GL_ARRAY_BUFFER, opengl_buffer[1]);
#elif GL_ARB_vertex_buffer_object
			glBindBufferARB(GL_ARRAY_BUFFER_ARB, opengl_buffer[1]);
#endif
			// Avoid copying to array and then to OpenGL
			if (base_idx) {
#if GL_VERSION_1_5
				glBufferSubData(GL_ARRAY_BUFFER, sizeof(GLint) * (state.max_samples - base_idx), sizeof(GLint) * base_idx, state.buffers[mic]);
				glBufferSubData(GL_ARRAY_BUFFER, sizeof(GLint) * base_idx, sizeof(GLint) * (state.max_samples - base_idx), state.buffers[mic]);
#elif GL_ARB_vertex_buffer_object
				glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, sizeof(GLint) * (state.max_samples - base_idx), sizeof(GLint) * base_idx, state.buffers[mic]);
				glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, sizeof(GLint) * base_idx, sizeof(GLint) * (state.max_samples - base_idx), state.buffers[mic]);
#endif
			} else {
#if GL_VERSION_1_5
				glBufferData(GL_ARRAY_BUFFER, sizeof(GLint) * state.max_samples, state.buffers[mic], GL_DYNAMIC_DRAW);
#elif GL_ARB_vertex_buffer_object
				glBufferDataARB(GL_ARRAY_BUFFER_ARB, sizeof(GLint) * state.max_samples, state.buffers[mic], GL_DYNAMIC_DRAW);
#endif
			}
#if GL_VERSION_1_1
			glDrawArrays(GL_LINE_STRIP, 0, state.max_samples);
#endif
		} else {
			glBegin(GL_LINE_STRIP);
			glColor4f(1.0f, 1.0f, 1.0f, 0.7f);
			for(x = 0, i = 0; i < state.max_samples; i++) {
				glVertex3f(x, ((float)win_h * (float)(2*mic + 1) / 8. ) + (float)(state.buffers[mic][(base_idx + i) % state.max_samples]) * ((float)win_h/4) /2147483647. , 0);
				x += xIncr;
			}
			glEnd();
		}
	}
	glutSwapBuffers();
}

void Reshape(int w, int h) {
	win_w = w;
	win_h = h;
	glViewport(0, 0, w, h);
	if (have_opengl_vbo) {
		float matrix[16];
		matrix[0] = 2.0f/(float)state.max_samples;
		matrix[1] = 0.0f;
		matrix[2] = 0.0f;
		matrix[3] = -1.0f;

		matrix[4] = 0.0f;
		matrix[5] = -2.0f;
		matrix[6] = 0.0f;
		matrix[7] = 1.0f;

		matrix[8]  = 0.0f;
		matrix[9]  = 0.0f;
		matrix[10] = -1.0f;
		matrix[11] = -1.0f;

		matrix[12] = 0.0f;
		matrix[13] = 0.0f;
		matrix[14] = 0.0f;
		matrix[15] = 1.0f;
		glUniformMatrix4fv(opengl_projection_matrix_location, 1, GL_TRUE, matrix);
	} else {
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0.0, (float)w, (float)h, 0.0, -1.0, 1.0);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
	}
}

void Keyboard(unsigned char key, int x, int y) {
	if(key == 'q') {
		die = 1;
		pthread_exit(NULL);
	}
	if(key == 32) {
		paused = !paused;
	}
}

int main(int argc, char** argv) {
	if (freenect_init(&f_ctx, NULL) < 0) {
		printf("freenect_init() failed\n");
		return 1;
	}
	freenect_set_log_level(f_ctx, FREENECT_LOG_INFO);
	freenect_select_subdevices(f_ctx, FREENECT_DEVICE_AUDIO);

	int nr_devices = freenect_num_devices (f_ctx);
	printf ("Number of devices found: %d\n", nr_devices);
	if (nr_devices < 1) {
		freenect_shutdown(f_ctx);
		return 1;
	}

	int user_device_number = 0;
	if (freenect_open_device(f_ctx, &f_dev, user_device_number) < 0) {
		printf("Could not open device\n");
		freenect_shutdown(f_ctx);
		return 1;
	}

	state.max_samples = 256 * 60;
	state.current_idx = 0;
	state.buffers[0] = malloc(state.max_samples * sizeof(int32_t));
	state.buffers[1] = malloc(state.max_samples * sizeof(int32_t));
	state.buffers[2] = malloc(state.max_samples * sizeof(int32_t));
	state.buffers[3] = malloc(state.max_samples * sizeof(int32_t));
	memset(state.buffers[0], 0, state.max_samples * sizeof(int32_t));
	memset(state.buffers[1], 0, state.max_samples * sizeof(int32_t));
	memset(state.buffers[2], 0, state.max_samples * sizeof(int32_t));
	memset(state.buffers[3], 0, state.max_samples * sizeof(int32_t));
	freenect_set_user(f_dev, &state);

	freenect_set_audio_in_callback(f_dev, in_callback);
	freenect_start_audio(f_dev);

	int res = pthread_create(&freenect_thread, NULL, freenect_threadfunc, NULL);
	if (res) {
		printf("pthread_create failed\n");
		freenect_shutdown(f_ctx);
		return 1;
	}
	printf("This is the libfreenect microphone waveform viewer.  Press 'q' to quit or spacebar to pause/unpause the view.\n");

	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_ALPHA );
	glutInitWindowSize(800, 600);
	glutInitWindowPosition(0, 0);
	glutCreateWindow("Microphones");
	glClearColor(0.0, 0.0, 0.0, 0.0);

	// Check OpenGL version to see whether VBO and shaders are supported
	GLint opengl_major_version, opengl_minor_version;
	glGetIntegerv(GL_MAJOR_VERSION, &opengl_major_version);
	glGetIntegerv(GL_MINOR_VERSION, &opengl_minor_version);
	have_opengl_vbo = (opengl_major_version >= 2);
	if (!have_opengl_vbo) {
		// Check for GL_ARB_VERTEX_BUFFER_OBJECT, GL_ARB_VERTEX_SHADER,
		// and GL_ARB_FRAGMENT_SHADER extensions
		int required_extension_count = 0;
		int opengl_extension_count = glGetIntegerv(GL_NUM_EXTENSIONS);
		for (int i = 0; i < opengl_extension_count; i++) {
			if (!strcmp("GL_ARB_VERTEX_BUFFER_OBJECT", glGetStringi(GL_EXTENSIONS, i))) {
				required_extension_count++;
			}
			if (!strcmp("GL_ARB_VERTEX_SHADER", glGetStringi(GL_EXTENSIONS, i))) {
				required_extension_count++;
			}
			if (!strcmp("GL_ARB_FRAGMENT_SHADER", glGetStringi(GL_EXTENSIONS, i))) {
				required_extension_count++;
			}
			if (!strcmp("GL_ARB_SHADER_OBJECTS", glGetStringi(GL_EXTENSIONS, i))) {
				required_extension_count++;
			}
		}
		// All extensions are present and OpenGL has version 1.1 for glDrawArrays
		if (required_extension_count == 4 &&

			(opengl_major_version >= 2 || (opengl_major_version == 1 && opengl_minor_version >= 1))) {
			have_opengl_vbo = 1;
		}
	}

	if (have_opengl_vbo) {

const char *vertex_shader_source = "#version 150 core\n"
"uniform mat4 projectionMatrix;"
"uniform int microphoneNumber;"
// Separate X and Y vertex coordinates
// X remains constant for different data
"in float vertexCoordinate_x;"
// Y changes for each shader call as we get different samples
"in int vertexCoordinate_y;"

"void main() {"
"	float x = vertexCoordinate_x;"
"	float y = ( vertexCoordinate_y / 8589934592. ) + ((2*microphoneNumber + 1) / 8. );"
"	gl_Position = projectionMatrix * vec4(x, y, -0.5, 1.0);"
"}";

const char *fragment_shader_source = "#version 150 core\n"
"void main() {"
"	gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);"
"}";
		GLuint program, vertex_shader, fragment_shader;
#if GL_VERSION_2_0
		program = glCreateProgram();
		vertex_shader = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
		glCompileShader(vertex_shader);
		glAttachShader(program, vertex_shader);
		fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL);
		glCompileShader(fragment_shader);
		glAttachShader(program, fragment_shader);
		glBindAttribLocation(program, 0, "vertexCoordinate_x");
		glBindAttribLocation(program, 1, "vertexCoordinate_y");
		glLinkProgram(program);
		glValidateProgram(program);
		glUseProgram(program);

		opengl_projection_matrix_location = glGetUniformLocation(program, "projectionMatrix");
		opengl_microphone_number_location = glGetUniformLocation(program, "microphoneNumber");
#elif GL_ARB_shader_objects
		program = glCreateProgramObjectARB();
		vertex_shader = glCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
		glShaderSourceARB(vertex_shader, 1, &vertex_shader_source, NULL);
		glCompileShaderARB(vertex_shader);
		glAttachObjectARB(program, vertex_shader);
		fragment_shader = glCreateShaderObjectARB(GL_FRAGMENT_SHADER);
		glShaderSourceARB(fragment_shader, 1, &fragment_shader_source, NULL);
		glCompileShaderARB(fragment_shader);
		glAttachObjectARB(program, fragment_shader);
		glBindAttribLocationARB(program, 0, "vertexCoordinate_x");
		glBindAttribLocationARB(program, 1, "vertexCoordinate_y");
		glLinkProgramARB(program);
		glValidateProgramARB(program);
		glUseProgramObjectARB(program);

		opengl_projection_matrix_location = glGetUniformLocationARB(program, "projectionMatrix");
		opengl_microphone_number_location = glGetUniformLocationARB(program, "microphoneNumber");
#endif

		// Prepare X coordinates for shader (the same for all microphones)
		GLfloat *array = malloc(sizeof(GLfloat) * state.max_samples);
		for(int i = 0; i < state.max_samples; i++) {
			array[i] = i;
		}
#if GL_VERSION_1_5
		glGenBuffers(2, opengl_buffer);
		glBindBuffer(GL_ARRAY_BUFFER, opengl_buffer[0]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * state.max_samples, array, GL_STATIC_DRAW);

		glVertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, 0, NULL);
		glEnableVertexAttribArray(0);

		glBindBuffer(GL_ARRAY_BUFFER, opengl_buffer[1]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(GLint) * state.max_samples, NULL, GL_DYNAMIC_DRAW);
		glVertexAttribIPointer(1, 1, GL_INT, GL_FALSE, 0, NULL);
		glEnableVertexAttribArray(1);
#elif GL_ARB_vertex_buffer_object
		glGenBuffersARB(2, opengl_buffer);
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, opengl_buffer[0]);
		glBufferDataARB(GL_ARRAY_BUFFER_ARB, sizeof(GLfloat) * state.max_samples, array, GL_STATIC_DRAW_ARB);

		glVertexAttribPointerARB(0, 1, GL_FLOAT, GL_FALSE, 0, NULL);
		glEnableVertexAttribArrayARB(0);

		glBindBufferARB(GL_ARRAY_BUFFER_ARB, opengl_buffer[1]);
		glBufferDataARB(GL_ARRAY_BUFFER_ARB, sizeof(GLint) * state.max_samples, NULL, GL_DYNAMIC_DRAW_ARB);
		glVertexAttribIPointerARB(1, 1, GL_INT, GL_FALSE, 0, NULL);
		glEnableVertexAttribArrayARB(1);
#endif
		free(array);
	} else {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	Reshape(800, 600);
	glutReshapeFunc(Reshape);
	glutDisplayFunc(DrawMicData);
	glutIdleFunc(DrawMicData);
	glutKeyboardFunc(Keyboard);

	glutMainLoop();

	return 0;
}
