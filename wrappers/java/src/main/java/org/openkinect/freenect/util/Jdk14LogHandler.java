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
package org.openkinect.freenect.util;

import org.openkinect.freenect.Device;
import org.openkinect.freenect.LogHandler;
import org.openkinect.freenect.LogLevel;

import java.util.EnumMap;
import java.util.logging.Level;
import java.util.logging.Logger;

public class Jdk14LogHandler implements LogHandler {

    private final Logger logger = Logger.getLogger("freenect");
    private final EnumMap<LogLevel, Level> levelMap = new EnumMap<LogLevel, Level>(LogLevel.class);
  
    public Jdk14LogHandler() {
        logger.setLevel(Level.ALL);
        levelMap.put(LogLevel.FATAL, Level.SEVERE);
        levelMap.put(LogLevel.ERROR, Level.SEVERE);
        levelMap.put(LogLevel.WARNING, Level.WARNING);
        levelMap.put(LogLevel.NOTICE, Level.CONFIG);
        levelMap.put(LogLevel.INFO, Level.INFO);
        levelMap.put(LogLevel.DEBUG, Level.FINE);
        levelMap.put(LogLevel.SPEW, Level.FINER);
        levelMap.put(LogLevel.FLOOD, Level.FINEST);
    }
  
    @Override
    public void onMessage(Device dev, LogLevel level, String msg) {
        logger.log(levelMap.get(level), "device " + dev.getDeviceIndex() + ": " + msg);
    }
}
