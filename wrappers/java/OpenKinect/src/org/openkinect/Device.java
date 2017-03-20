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

package org.openkinect;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * @author Michael Nischt
 */
public final class Device
{
    private final Context context;
    private long jni;

    private Image rgb, depth;
    private Acceleration acceleration;

    Device(Context context, long jni)
    {
        if(context == null)
        {
            throw new NullPointerException();
        }

        this.context = context;
        this.jni = jni;
    }

    public void light(LEDStatus led)
    {
        checkDisposed();

        jniSetLED(led.ordinal());
    }
    
    public void tilt(float deg)
    {
        checkDisposed();

        jniSetTiltDegs(deg);
    }
    
    public void color(Image image)
    {
        color(image, ColorFormat.RGB_8_8_8);
    }

    public void color(Image image, ColorFormat format)
    {
        checkDisposed();

        jniSetFormatRGB(format.ordinal());
        rgb = image;

        if(rgb != null)
        {
            jniStartCaptureRGB();
        }
        else
        {
            jniStopCaptureRGB();
        }
    }

    public void depth(Image image)
    {
        depth(image, DepthFormat.RAW_11);
    }

    public void depth(Image image, DepthFormat format)
    {
        checkDisposed();

        jniSetFormatDepth(format.ordinal());
        depth = image;

        if(depth != null)
        {
            jniStartCaptureDepth();
        }
        else
        {
            jniStopCaptureDepth();
        }
    }

    public void acceleration(Acceleration acceleration)
    {
        checkDisposed();

        this.acceleration = acceleration;
    }
    
    public void dispose()
    {
        context.disposeDevice(jni);
        jniClose();
        jni = 0;
    }

    private void checkDisposed()
    {
        if(jni == 0)
        {
            throw new IllegalStateException("Device already dispoed.");
        }
    }

    // <editor-fold defaultstate="collapsed" desc="native callbacks">

    void onRGB(ByteBuffer buffer)
    {
        if (rgb != null)
        {
            buffer.order(ByteOrder.nativeOrder());
            rgb.data(buffer);
        }
    }

    void onDepth(ByteBuffer buffer)
    {
        if (depth != null)
        {
            buffer.order(ByteOrder.nativeOrder());
            depth.data(buffer);
        }
    }

    void onAcceleration(float x, float y, float z)
    {
        if (acceleration != null)
        {
            acceleration.direction(x, y, z);
        }
    }

    // </editor-fold>

    // <editor-fold defaultstate="collapsed" desc="native">

    native private void jniClose();

    native private void jniSetLED(int light);

    native private void jniSetTiltDegs(float deg);

    native private void jniSetFormatRGB(int format);

    native private void jniSetFormatDepth(int format);

    native private void jniStartCaptureRGB();

    native private void jniStartCaptureDepth();

    native private void jniStopCaptureRGB();

    native private void jniStopCaptureDepth();

    // </editor-fold>
}
