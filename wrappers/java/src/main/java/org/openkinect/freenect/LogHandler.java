package org.openkinect.freenect;

public interface LogHandler {
    void onMessage(Device dev, LogLevel level, String msg);
}