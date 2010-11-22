/*
 * This file is part of the OpenKinect Project. http://www.openkinect.org
 *
 * Copyright (c) 2010 Kelvie Wong <kelvie@ieee.org>
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

#include "Python.h"
#include "libfreenect.h"
#include <pthread.h>

#define DEPTH_SIZE sizeof(freenect_depth)*FREENECT_FRAME_PIX
#define RGB_SIZE sizeof(freenect_pixel)*FREENECT_RGB_SIZE

/* Globals */
static freenect_context *current_ctx = NULL;
static freenect_device *current_dev = NULL;
static uint32_t depth_timestamp = 0;
static freenect_depth *current_depths = NULL;
static pthread_mutex_t depth_mutex = PTHREAD_MUTEX_INITIALIZER;
static freenect_pixel *current_rgb = NULL;
static uint32_t rgb_timestamp = 0;
static pthread_mutex_t rgb_mutex = PTHREAD_MUTEX_INITIALIZER;

/* mutex for the freenect library; not sure if it is thread safe */
static pthread_mutex_t freenect_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t capture_thread;
static int capture_started = 0;


static void capture_depth(freenect_device *dev, freenect_depth *depth, uint32_t timestamp)
{
    pthread_mutex_lock(&depth_mutex);

    /* Save the depth data */
    depth_timestamp = timestamp;
    memcpy(current_depths, depth, DEPTH_SIZE);

    pthread_mutex_unlock(&depth_mutex);
}

static void capture_rgb(freenect_device *dev, freenect_pixel *rgb, uint32_t timestamp)
{
    pthread_mutex_lock(&rgb_mutex);

    /* Save the rgb data */
    rgb_timestamp = timestamp;
    memcpy(current_rgb, rgb, RGB_SIZE);

    pthread_mutex_unlock(&rgb_mutex);
}

/* Process the captures; this runs in its own thread */
static void *do_capture(void *arg)
{
    pthread_mutex_lock(&freenect_mutex);
    freenect_set_depth_callback(current_dev, capture_depth);
    freenect_set_rgb_callback(current_dev, capture_rgb);

    freenect_start_depth(current_dev);
    freenect_start_rgb(current_dev);

    pthread_mutex_unlock(&freenect_mutex);

    while (capture_started) {
        pthread_mutex_lock(&freenect_mutex);
        if (freenect_process_events(current_ctx) < 0) {
            pthread_mutex_unlock(&freenect_mutex);
            break;
        }
        pthread_mutex_unlock(&freenect_mutex);
    }

}

/* Module functions */
static PyObject *
pykinect_get_frame_params(PyObject *self, PyObject *args)
{
    (void)args;
    return (PyObject *) Py_BuildValue("ii", FREENECT_FRAME_W, FREENECT_FRAME_H);
}

static PyObject *
pykinect_init(PyObject *self, PyObject *args)
{
    int index = 0;

    if (current_dev || current_ctx)
        return Py_BuildValue("s", "Kinect already initialized.");

    index = (int) PyLong_AsLong(args);

    if (freenect_init(&current_ctx, NULL) < 0) {
        current_ctx = NULL;
        return Py_BuildValue("s", "Failed to initialize Kinect.");
    }

    if (freenect_open_device(current_ctx, &current_dev, index) < 0) {
        current_dev = NULL;
        return Py_BuildValue("s", "Failed to open device.");
    }
    freenect_set_rgb_format(current_dev, FREENECT_FORMAT_RGB);
	freenect_set_depth_format(current_dev, FREENECT_FORMAT_11_BIT);
    return Py_None;
}

static PyObject *
pykinect_start_cap(PyObject *self, PyObject *args)
{
    if (!current_dev)
        return Py_BuildValue("s", "Device not initialized.");

    if (capture_started)
        return Py_BuildValue("s", "Capture already started.");

    if (!current_depths)
        current_depths = malloc(DEPTH_SIZE);
    if (!current_rgb)
        current_rgb = malloc(RGB_SIZE);

    capture_started = 1;
    pthread_create(&capture_thread, NULL, do_capture, NULL);

    return Py_None;
}

static PyObject *
pykinect_stop_cap(PyObject *self, PyObject *args)
{
    capture_started = 0;
    pthread_join(capture_thread, NULL);

    free(current_depths);
    free(current_rgb);
    return Py_None;
}

static PyObject *
pykinect_get_depths(PyObject *self, PyObject *args)
{
    freenect_depth *my_depths;
    uint32_t my_timestamp;
    int i;
    PyObject *tuple;
    PyObject *result;

    /* Detach the data quickly */
    pthread_mutex_lock(&depth_mutex);
    my_depths = current_depths;
    my_timestamp = depth_timestamp;
    current_depths = malloc(DEPTH_SIZE);
    pthread_mutex_unlock(&depth_mutex);

    /* Now pump them into the tuples */
    tuple = PyTuple_New(DEPTH_SIZE);
    for (i=0; i<DEPTH_SIZE; i++)
        PyTuple_SET_ITEM(tuple, i, PyInt_FromLong(my_depths[i]));

    free(my_depths);

    /* Result tuple is (data, timestamp, error message) */
    result = PyTuple_New(3);
    PyTuple_SET_ITEM(result, 0, tuple);
    PyTuple_SET_ITEM(result, 1, PyInt_FromLong(my_timestamp));
    PyTuple_SET_ITEM(result, 2, Py_None);

    return result;
}

static PyObject *
pykinect_get_image(PyObject *self, PyObject *args)
{
    freenect_pixel *my_rgb;
    uint32_t my_timestamp;
    int i;
    PyObject *tuple;
    PyObject *result;

    /* Detach the data quickly */
    pthread_mutex_lock(&rgb_mutex);
    my_rgb = current_rgb;
    my_timestamp = rgb_timestamp;
    current_rgb = malloc(RGB_SIZE);
    pthread_mutex_unlock(&rgb_mutex);

    /* Now pump them into the tuples */
    tuple = PyTuple_New(RGB_SIZE);
    for (i=0; i<RGB_SIZE; i++)
        PyTuple_SET_ITEM(tuple, i, PyInt_FromLong(my_rgb[i]));

    free(my_rgb);

    /* Result tuple is (data, timestamp, error message) */
    result = PyTuple_New(3);
    PyTuple_SET_ITEM(result, 0, tuple);
    PyTuple_SET_ITEM(result, 1, PyInt_FromLong(my_timestamp));
    PyTuple_SET_ITEM(result, 2, Py_None);

    return result;
}

static PyObject *
pykinect_setled(PyObject *self, PyObject *args)
{
    pthread_mutex_lock(&freenect_mutex);
    // TODO
    pthread_mutex_unlock(&freenect_mutex);
    return Py_None;
}

static PyObject *
pykinect_getled(PyObject *self, PyObject *args)
{
    pthread_mutex_lock(&freenect_mutex);
    // TODO
    pthread_mutex_unlock(&freenect_mutex);
    return Py_None;
}

static PyObject *
pykinect_settilt(PyObject *self, PyObject *args)
{
    pthread_mutex_lock(&freenect_mutex);
    // TODO
    pthread_mutex_unlock(&freenect_mutex);
    return Py_None;
}

static PyObject *
pykinect_gettilt(PyObject *self, PyObject *args)
{
    pthread_mutex_lock(&freenect_mutex);
    // TODO
    pthread_mutex_unlock(&freenect_mutex);
    return Py_None;
}

/* Method table */
static PyMethodDef PyKinectMethods[] = {
    {"init", pykinect_init, METH_O,
     "Initialize the Kinect"},
    {"set_led", pykinect_setled, METH_O,
     "Set the LED status"},
    {"get_led", pykinect_getled, METH_NOARGS,
     "Get the LED status"},
    {"set_tilt", pykinect_setled, METH_O,
     "Set the horizontal tilt (between 0-30 in degrees)"},
    {"get_tilt", pykinect_setled, METH_NOARGS,
     "Return the horizontal tilt (between 0-30 in degrees)"},
    {"get_frame_params", pykinect_get_frame_params, METH_NOARGS,
     "Return the width and height of the frame captured"},
    {"get_depths", pykinect_get_depths, METH_NOARGS,
     "Return a tuple with the raw depth data for the latest frame and the "
     "timestamp."},
    {"get_image", pykinect_get_image, METH_NOARGS,
     "Return a tuple with the raw RGB data for the latest frame and the "
     "timestamp."},
    {"start_cap", pykinect_start_cap, METH_NOARGS,
     "Start video/depth capture.  Must be called before the get_* functions"},
    {"stop_cap", pykinect_start_cap, METH_NOARGS,
     "Stop video/depth capture."},

    {NULL, NULL, 0, NULL} /* sentinel */
};

/* Init function */
PyMODINIT_FUNC
initpykinect_c(void)
{
    (void) Py_InitModule3("pykinect_c", PyKinectMethods,
                          "A Python interface to the OpenKinect Kinect library.");
}
