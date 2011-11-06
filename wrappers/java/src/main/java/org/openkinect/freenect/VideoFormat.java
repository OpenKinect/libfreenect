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


public enum VideoFormat {
    RGB(0, Freenect.FREENECT_FRAME_W, Freenect.FREENECT_FRAME_H, Freenect.FREENECT_VIDEO_RGB_SIZE),
    BAYER(1, Freenect.FREENECT_FRAME_W, Freenect.FREENECT_FRAME_H, Freenect.FREENECT_VIDEO_BAYER_SIZE),
    IR_8BIT(2, Freenect.FREENECT_IR_FRAME_W, Freenect.FREENECT_IR_FRAME_H, Freenect.FREENECT_VIDEO_IR_8BIT_SIZE),
    IR_10BIT(3, Freenect.FREENECT_IR_FRAME_W, Freenect.FREENECT_IR_FRAME_H, Freenect.FREENECT_VIDEO_IR_10BIT_SIZE),
    IR_10BIT_PACKED(4, Freenect.FREENECT_IR_FRAME_W, Freenect.FREENECT_IR_FRAME_H, Freenect.FREENECT_VIDEO_IR_10BIT_PACKED_SIZE),
    YUV_RGB(5, Freenect.FREENECT_FRAME_W, Freenect.FREENECT_FRAME_H, Freenect.FREENECT_VIDEO_RGB_SIZE),
    YUV_RAW(6, Freenect.FREENECT_FRAME_W, Freenect.FREENECT_FRAME_H, Freenect.FREENECT_VIDEO_YUV_SIZE);
    
    private final int value;
    private final int frameSize;
    private final int width;
    private final int height;

    private VideoFormat(int value, int width, int height, int frameSize) {
        this.value = value;
        this.width = width;
        this.height = height;
        this.frameSize = frameSize;
    }
    
    public int intValue() {
        return value;
    }
    
    public int getFrameSize() {
        return frameSize;
    }
    
    public int getWidth() {
        return width;
    }
    
    public int getHeight() {
        return height;
    }
}