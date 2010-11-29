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

#include <libfreenect.h>

#include "org_openkinect_Device.h"

freenect_device *device(JNIEnv *env, jobject obj)
{
    jclass clazz = env->GetObjectClass(obj);
    jfieldID fieldID = env->GetFieldID(clazz, "jni", "J");

    jlong ptr = env->GetLongField(obj, fieldID);
    return (freenect_device*) ptr;
}

JNIEXPORT void JNICALL Java_org_openkinect_Device_jniClose
  (JNIEnv *env, jobject obj)
{
    freenect_device *f_dev = device(env, obj);
    freenect_close_device(f_dev);
}

JNIEXPORT void JNICALL Java_org_openkinect_Device_jniSetLED
  (JNIEnv *env, jobject obj, jint led)
{
    freenect_device *f_dev = device(env, obj);
    freenect_set_led(f_dev, (freenect_led_options) led);
}

JNIEXPORT void JNICALL Java_org_openkinect_Device_jniSetTiltDegs
  (JNIEnv *env, jobject obj, jfloat deg)
{
    freenect_device *f_dev = device(env, obj);
    freenect_set_tilt_degs(f_dev, (double) deg);
}

JNIEXPORT void JNICALL Java_org_openkinect_Device_jniSetFormatRGB
  (JNIEnv *env, jobject obj, jint format)
{
    freenect_device *f_dev = device(env, obj);
    freenect_set_rgb_format(f_dev, (freenect_rgb_format) format);
}

JNIEXPORT void JNICALL Java_org_openkinect_Device_jniSetFormatDepth
  (JNIEnv *env, jobject obj, jint format)
{
    freenect_device *f_dev = device(env, obj);
    freenect_set_depth_format(f_dev, (freenect_depth_format) format);
}


JNIEXPORT void JNICALL Java_org_openkinect_Device_jniStartCaptureRGB
  (JNIEnv *env, jobject obj)
{
    freenect_device *f_dev = device(env, obj);
    freenect_start_rgb(f_dev);
}

JNIEXPORT void JNICALL Java_org_openkinect_Device_jniStartCaptureDepth
  (JNIEnv *env, jobject obj)
{
    freenect_device *f_dev = device(env, obj);
    freenect_start_depth(f_dev);
}

JNIEXPORT void JNICALL Java_org_openkinect_Device_jniStopCaptureRGB
(JNIEnv *env, jobject obj)
{
    freenect_device *f_dev = device(env, obj);
    freenect_stop_rgb(f_dev);
}

JNIEXPORT void JNICALL Java_org_openkinect_Device_jniStopCaptureDepth
  (JNIEnv *env, jobject obj)
{
    freenect_device *f_dev = device(env, obj);
    freenect_stop_depth(f_dev);
}
