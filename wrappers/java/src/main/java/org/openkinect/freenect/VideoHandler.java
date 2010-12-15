package org.openkinect.freenect;

import java.nio.ByteBuffer;

public interface VideoHandler {
    void onFrameReceived(VideoFormat format, ByteBuffer frame, int timestamp);
}