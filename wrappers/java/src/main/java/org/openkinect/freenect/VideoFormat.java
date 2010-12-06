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