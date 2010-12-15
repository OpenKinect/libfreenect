package org.openkinect.freenect;

import java.util.HashMap;
import java.util.Map;

public enum LogLevel {
    FATAL(0),
    ERROR(1),
    WARNING(2),
    NOTICE(3),
    INFO(4),
    DEBUG(5),
    SPEW(6),
    FLOOD(7);

    private static final Map<Integer, LogLevel> MAP = new HashMap<Integer, LogLevel>(8);
    static {
        for (LogLevel value : values()) {
            MAP.put(value.intValue(), value);
        }
    }

    private final int value;

    private LogLevel(int value) {
        this.value = value;
    }

    public int intValue() {
        return value;
    }

    public static LogLevel fromInt(int value) {
        return MAP.get(value);
    }
}
