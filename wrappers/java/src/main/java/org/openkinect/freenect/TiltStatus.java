package org.openkinect.freenect;

import java.util.HashMap;
import java.util.Map;

public enum TiltStatus {
    STOPPED(0),
    LIMIT(1),
    MOVING(4);
    private final int value;

    static final Map<Integer, TiltStatus> MAP = new HashMap<Integer, TiltStatus>(3);
    static {
      for (TiltStatus ts : TiltStatus.values()) {
        MAP.put(ts.intValue(), ts);
      }
    }

    private TiltStatus(int value) {
        this.value = value;
    }

    public int intValue() {
        return value;
    }

    public static TiltStatus fromInt(int value) {
      return MAP.get(value);
    }
}
