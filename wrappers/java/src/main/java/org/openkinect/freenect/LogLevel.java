/**
 * This file is part of the OpenKinect Project. http://www.openkinect.org
 *
 * Copyright (c) 2010 individual OpenKinect contributors. See the CONTRIB file
 * for details.
 *
 * This code is licensed to you under the terms of the Apache License, version
 * 2.0, or, at your option, the terms of the GNU General Public License,
 * version 2.0. See the APACHE20 and GPL20 files for the text of the licenses,
 * or the following URLs:
 * http://www.apache.org/licenses/LICENSE-2.0
 * http://www.gnu.org/licenses/gpl-2.0.txt
 *
 * If you redistribute this file in source form, modified or unmodified,
 * you may:
 * 1) Leave this header intact and distribute it under the same terms,
 * accompanying it with the APACHE20 and GPL20 files, or
 * 2) Delete the Apache 2.0 clause and accompany it with the GPL20 file, or
 * 3) Delete the GPL v2.0 clause and accompany it with the APACHE20 file
 * In all cases you must keep the copyright notice intact and include a copy
 * of the CONTRIB file.
 * Binary distributions must follow the binary distribution requirements of
 * either License.
 */
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

    private final int value;
    private static final Map<Integer, LogLevel> MAP = new HashMap<Integer, LogLevel>(8);
    static {
        for(LogLevel v : LogLevel.values()) {
            MAP.put(v.intValue(), v);
        }
    }

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