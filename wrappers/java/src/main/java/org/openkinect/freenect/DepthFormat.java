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