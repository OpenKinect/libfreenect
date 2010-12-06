package org.openkinect.freenect;

public enum TiltStatus {
    STOPPED(0),
    LIMIT(1),
    MOVING(4);
    private final int value;

    private TiltStatus(int value) {
        this.value = value;
    }
    public int intValue() {
        return value;
    }
}