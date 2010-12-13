package org.openkinect.freenect.util;

import java.util.EnumMap;
import java.util.logging.Level;
import java.util.logging.Logger;

import org.openkinect.freenect.Device;
import org.openkinect.freenect.LogHandler;
import org.openkinect.freenect.LogLevel;

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
