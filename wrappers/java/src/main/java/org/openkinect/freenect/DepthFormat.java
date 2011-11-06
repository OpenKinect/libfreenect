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


public enum DepthFormat {
    D11BIT(0, Freenect.FREENECT_FRAME_W, Freenect.FREENECT_FRAME_H, Freenect.FREENECT_DEPTH_11BIT_SIZE),
    D10BIT(1, Freenect.FREENECT_FRAME_W, Freenect.FREENECT_FRAME_H, Freenect.FREENECT_DEPTH_10BIT_SIZE),
    D11BIT_PACKED(2, Freenect.FREENECT_FRAME_W, Freenect.FREENECT_FRAME_H, Freenect.FREENECT_DEPTH_11BIT_PACKED_SIZE),
    D10BIT_PACKED(3, Freenect.FREENECT_FRAME_W, Freenect.FREENECT_FRAME_H, Freenect.FREENECT_DEPTH_10BIT_PACKED_SIZE);
    private int value;
    private int frameSize;
    private int width;
    private int height;

    private DepthFormat(int value, int width, int height, int frameSize) {
        this.value = value;
        this.width = width;
        this.height = height;
        this.frameSize = frameSize;
    }
    public int intValue() {
        return value;
    }
    public int getWidth() {
        return width;
    }
    public int getHeight() {
        return height;
    }
    public int getFrameSize() {
        return frameSize;
    }
}
