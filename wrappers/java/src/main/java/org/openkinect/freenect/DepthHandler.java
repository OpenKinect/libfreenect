package org.openkinect.freenect;

import java.nio.ByteBuffer;

public interface DepthHandler {
    void onFrameReceived(DepthFormat format, ByteBuffer frame, int timestamp);
}