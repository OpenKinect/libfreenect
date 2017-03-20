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
#include <map>
#include <list>
#include <string>

#include "org_openkinect_Context.h"


struct Data
{
    freenect_pixel *rgb;
    void *depth;

    Data() : rgb(0), depth(0) {}
};

std::list<std::string> logs;
std::map<freenect_device*, Data> data;

jobject jDevice(JNIEnv* env, jobject jContext, freenect_device *dev)
{
    jclass clazz = env->GetObjectClass(jContext);
    jmethodID methodID = env->GetMethodID(clazz, "device", "(J)Lorg/openkinect/Device;");
    return env->CallObjectMethod(jContext, methodID, (jlong) dev);
}

void log_cb(freenect_context *context, freenect_loglevel level, const char *msg)
{
   logs.push_back(msg);
}

void rgb_cb(freenect_device *dev, freenect_pixel *rgb, uint32_t timestamp)
{
    data[dev].rgb = rgb;
}

void depth_cb(freenect_device *dev, void *v_depth, uint32_t timestamp)
{
    data[dev].depth = v_depth;
}

freenect_context *context(JNIEnv *env, jobject obj)
{
    jclass clazz = env->GetObjectClass(obj);
    jfieldID fieldID = env->GetFieldID(clazz, "jni", "J");

    jlong ptr = env->GetLongField(obj, fieldID);
    return (freenect_context*) ptr;
}


JNIEXPORT jlong JNICALL Java_org_openkinect_Context_jniInit
  (JNIEnv *, jobject)
{
    freenect_context *f_ctx = 0;

    if (freenect_init(&f_ctx, NULL) < 0)
    {
        return 0;
    }

    freenect_set_log_callback(f_ctx, log_cb);

    return (jlong) f_ctx;
}

JNIEXPORT void JNICALL Java_org_openkinect_Context_jniShutdown
  (JNIEnv *env, jobject obj)
{
    freenect_context *f_ctx = context(env, obj);
    freenect_shutdown(f_ctx);
}

JNIEXPORT jboolean JNICALL Java_org_openkinect_Context_jniProcessEvents
  (JNIEnv *env, jobject jContext)
{

    freenect_context *f_ctx = context(env, jContext);

    for(std::list<std::string>::const_iterator it = logs.begin(); it != logs.end(); ++it)
    {
        jobject jString = env->NewStringUTF(it->c_str());
        jclass clazz = env->GetObjectClass(jContext);
        jmethodID methodID = env->GetMethodID(clazz, "onLog", "(Ljava/lang/String;)V");
        env->CallVoidMethod(jContext, methodID, jString);
    }
    logs.clear();

    bool success = (freenect_process_events(f_ctx) >= 0);



    for(std::map<freenect_device*, Data>::iterator it = data.begin(); it != data.end(); ++it)
    {
        freenect_device *f_dev = it->first;
        Data& d = it->second;

        if(d.rgb)
        {
            jobject buffer = env->NewDirectByteBuffer(d.rgb, FREENECT_RGB_SIZE);

            jobject device = jDevice(env, jContext, f_dev);
            jclass clazz = env->GetObjectClass(device);
            jmethodID methodID = env->GetMethodID(clazz, "onRGB", "(Ljava/nio/ByteBuffer;)V");
            env->CallVoidMethod(device, methodID, buffer);

            d.rgb = 0;
        }

        if(d.depth)
        {
            jobject buffer = env->NewDirectByteBuffer(d.depth, FREENECT_DEPTH_SIZE);

            jobject device = jDevice(env, jContext, f_dev);
            jclass clazz = env->GetObjectClass(device);
            jmethodID methodID = env->GetMethodID(clazz, "onDepth", "(Ljava/nio/ByteBuffer;)V");
            env->CallVoidMethod(device, methodID, buffer);

            d.depth = 0;
        }

        {
			freenect_update_device_state(f_dev);
			freenect_raw_device_state* f_dev_raw = freenect_get_device_state(f_dev);

            double x, y, z;
            freenect_get_mks_accel(f_dev_raw, &x, &y, &z);

            jobject device = jDevice(env, jContext, f_dev);
            jclass clazz = env->GetObjectClass(device);
            jmethodID methodID = env->GetMethodID(clazz, "onAcceleration", "(FFF)V");
            env->CallVoidMethod(device, methodID, (jfloat) x, (jfloat) y, (jfloat) z);
        }
    }

    return success;
}

JNIEXPORT jint JNICALL Java_org_openkinect_Context_jniNumDevices
  (JNIEnv *env, jobject obj)
{
    freenect_context *f_ctx = context(env, obj);
    return freenect_num_devices(f_ctx);
}

JNIEXPORT jlong JNICALL Java_org_openkinect_Context_jniOpenDevice
  (JNIEnv *env, jobject obj, jint index)
{
    freenect_context *f_ctx = context(env, obj);

    freenect_device *f_dev = 0;
    if(freenect_open_device(f_ctx, &f_dev, index) < 0)
    {
        return 0;
    }

    freenect_set_rgb_callback(f_dev, rgb_cb);
    freenect_set_depth_callback(f_dev, depth_cb);

    return (jlong) f_dev;
}

JNIEXPORT void JNICALL Java_org_openkinect_Context_jniSetLogLevel
  (JNIEnv *env, jobject obj, jint level)
{
    freenect_context *f_ctx = context(env, obj);
    freenect_set_log_level(f_ctx, (freenect_loglevel) level);
}




