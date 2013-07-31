/**
 * This file is part of the OpenKinect Project. http://www.openkinect.org
 *
 * Copyright (c) 2010 individual OpenKinect contributors. See the CONTRIB file
 * for details.
 *
 * This code is licensed to you under the terms of the Apache License, version
 * 2.0, or, at your option, the terms of the GNU General Public License,
 * version 2.0. See the APACHE20 and GPL20 files for the text of the licenses,
 * or the following URLs:
 * http://www.apache.org/licenses/LICENSE-2.0
 * http://www.gnu.org/licenses/gpl-2.0.txt
 *
 * If you redistribute this file in source form, modified or unmodified,
 * you may:
 * 1) Leave this header intact and distribute it under the same terms,
 * accompanying it with the APACHE20 and GPL20 files, or
 * 2) Delete the Apache 2.0 clause and accompany it with the GPL20 file, or
 * 3) Delete the GPL v2.0 clause and accompany it with the APACHE20 file
 * In all cases you must keep the copyright notice intact and include a copy
 * of the CONTRIB file.
 * Binary distributions must follow the binary distribution requirements of
 * either License.
 */
package org.openkinect.freenect;

import com.sun.jna.Structure;
import java.util.List;
import java.util.Arrays;

public class FrameMode extends Structure {
    /* All fields are public because Structure requires it.
       However, fields should NOT be altered by external code. */
    public int reserved;
    public int resolution;
    public int format;
    public int bytes;
    public short width, height;
    public byte dataBitsPerPixel, paddingBitsPerPixel;
    public byte framerate, valid;

    public FrameMode() {
        valid = 0;
    }

    protected List getFieldOrder() {
        return Arrays.asList(new String[] {"reserved", "resolution", "format",
                "bytes", "width", "height", "dataBitsPerPixel",
                "paddingBitsPerPixel", "framerate", "valid"});
    }

    public Resolution getResolution() {
        return Resolution.fromInt(resolution);
    }

    public DepthFormat getDepthFormat() {
        return DepthFormat.fromInt(format);
    }

    public VideoFormat getVideoFormat() {
        return VideoFormat.fromInt(format);
    }

    public int getFrameSize() {
        return bytes;
    }

    public short getWidth() {
        return width;
    }

    public short getHeight() {
        return height;
    }

    public int getFrameRate() {
        return framerate;
    }

    public boolean isValid() {
        return (valid != 0);
    }

    public static class ByValue extends FrameMode implements Structure.ByValue { }
}
