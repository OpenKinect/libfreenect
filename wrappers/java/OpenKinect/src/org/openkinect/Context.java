/*
 * This file is part of the OpenKinect Project. http://www.openkinect.org
 *
 * Copyright (c) 2010 individual OpenKinect contributors. See the CONTRIB file
 * for details.
 *
 * This code is licensed to you under the terms of the Apache License, version
 * 2.0, or, at your option, the terms of the GNU General Public License,
 * version 2.0. See the APACHE20 and GPL2 files for the text of the licenses,
 * or the following URLs:
 * http://www.apache.org/licenses/LICENSE-2.0
 * http://www.gnu.org/licenses/gpl-2.0.txt
 *
 * If you redistribute this file in source form, modified or unmodified, you
 * may:
 *   1) Leave this header intact and distribute it under the same terms,
 *      accompanying it with the APACHE20 and GPL20 files, or
 *   2) Delete the Apache 2.0 clause and accompany it with the GPL2 file, or
 *   3) Delete the GPL v2 clause and accompany it with the APACHE20 file
 * In all cases you must keep the copyright notice intact and include a copy
 * of the CONTRIB file.
 *
 * Binary distributions must follow the binary distribution requirements of
 * either License.
 */

package org.openkinect;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * @author Michael Nischt
 */
public class Context
{
    // <editor-fold defaultstate="collapsed" desc="instance">

    static
    {
        System.loadLibrary("OpenKinect");
    }

    public static Context getContext()
    {
        return instance;
    }
    private static final Context instance = new Context();

    // </editor-fold>

    private final long jni;
    private Logger logger;
    private final Map<Long, Device> jniMap = new HashMap<Long, Device>();
    
    private Context()
    {
        jni = jniInit();

        Runtime.getRuntime().addShutdownHook(new Thread()
        {
            @Override
            public void run()
            {
               List<Device> devices = new ArrayList<Device>(jniMap.values());
               for(Device device : devices)
               {
                    device.dispose();
               }
               jniShutdown();
            }
        });
    }

    public int devices()
    {
        return jniNumDevices();
    }

    public Device getDevice(int index)
    {
        long dev_ptr = jniOpenDevice(index);
        if(dev_ptr == 0)
        {
            return null;
        }
        Device device = new Device(this, dev_ptr);
        jniMap.put(dev_ptr, device);
        return device;
    }

    public boolean processEvents()
    {
        return jniProcessEvents();
    }

    public Device device(long jni)
    {
        return jniMap.get(jni);
    }

    public void log(Logger logger)
    {
        log(logger, LogLevel.INFO);
    }

    public void log(Logger logger, LogLevel level)
    {
        jniSetLogLevel(level.ordinal());
        this.logger = logger;
    }

    void disposeDevice(long dev_ptr)
    {
        jniMap.remove(dev_ptr);
    }


    // <editor-fold defaultstate="collapsed" desc="native callbacks">

    void onLog(String msg)
    {
        if(logger != null)
        {
            logger.log(msg);
        }
    }

    // </editor-fold>

    // <editor-fold defaultstate="collapsed" desc="native">

    native private long jniInit();

    native private void jniShutdown();

    native private boolean jniProcessEvents();

    native private int jniNumDevices();

    native private long jniOpenDevice(int index);

    native private void jniSetLogLevel(int level);

    // </editor-fold>
}
