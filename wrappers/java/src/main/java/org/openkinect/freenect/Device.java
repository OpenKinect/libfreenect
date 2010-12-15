package org.openkinect.freenect;



public interface Device {
    double[] getAccel();
    int setLed(LedStatus status);
    void refreshTiltState();
    double getTiltAngle();
    int setTiltAngle(double angle);
    TiltStatus getTiltStatus();
    void setDepthFormat(DepthFormat fmt);
    void setVideoFormat(VideoFormat fmt);
    int startDepth(DepthHandler handler);
    int startVideo(VideoHandler handler);
    int stopDepth();
    int stopVideo();
    void close();
    public abstract int getDeviceIndex();
}
