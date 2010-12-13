package org.openkinect.freenect;

public interface Context {
    int numDevices();
    void setLogHandler(LogHandler handler);
    void setLogLevel(LogLevel level);
    Device openDevice(int index);
    void shutdown();
}