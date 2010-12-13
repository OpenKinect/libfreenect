package org.openkinect.freenect;

public enum LedStatus {
    OFF(0),
    GREEN(1),
    RED(2),
    YELLOW(3),
    BLINK_YELLOW(4),
    BLINK_GREEN(5),
    BLINK_RED_YELLOW(6);
    private final int value;

    private LedStatus(int value) {
        this.value = value;
    }

    public int intValue() {
        return value;
    }
}