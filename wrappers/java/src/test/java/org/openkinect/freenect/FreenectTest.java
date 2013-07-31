package org.openkinect.freenect;

import static org.hamcrest.Matchers.*;
import static org.junit.Assert.*;
import static org.junit.Assume.*;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.List;

import org.hamcrest.collection.IsEmptyCollection;
import org.junit.AfterClass;
import org.junit.BeforeClass;
import org.junit.Test;
import org.openkinect.freenect.util.Jdk14LogHandler;

public class FreenectTest {
    static Context ctx;
    static Device dev;
    
    @BeforeClass
    public static void initKinect() {
        ctx = Freenect.createContext();
        ctx.setLogHandler(new Jdk14LogHandler());
        ctx.setLogLevel(LogLevel.SPEW);
        if (ctx.numDevices() > 0) {
            dev = ctx.openDevice(0);
        } else {
            System.err.println("WARNING: No kinects detected, hardware tests will be implicitly passed.");
        }
    }
    
    @AfterClass
    public static void shutdownKinect() {
        if (ctx != null)
            if (dev != null) {
                dev.close();
            }
        ctx.shutdown();
    }

    protected void moveAndWait(Device device, int degrees) throws InterruptedException {
        device.refreshTiltState();
        if (device.getTiltAngle() >= (degrees - 2) && device.getTiltAngle() <= (degrees + 2)) {
            return;
        }
        assertThat(device.setTiltAngle(degrees), is(0));

        while (device.getTiltStatus() == TiltStatus.STOPPED) {
            device.refreshTiltState();
        }

        if (device.getTiltStatus() == TiltStatus.MOVING) {
            while (device.getTiltStatus() == TiltStatus.MOVING) {
                device.refreshTiltState();
            }
        }

        if (device.getTiltStatus() == TiltStatus.STOPPED) {
            while (device.getTiltAngle() < -32) {
                device.refreshTiltState();
            }
        }
    }

    @Test(timeout = 10000)
    public void testSetTiltAngle() throws InterruptedException {
        assumeThat(dev, is(not(nullValue())));

        ctx.setLogLevel(LogLevel.SPEW);
        dev.refreshTiltState();

        moveAndWait(dev, 0);
        assertThat(dev.getTiltAngle(), is(closeTo(0, 2)));

        moveAndWait(dev, 20);
        assertThat(dev.getTiltAngle(), is(closeTo(20, 2)));

        moveAndWait(dev, -20);
        assertThat(dev.getTiltAngle(), is(closeTo(-20, 2)));

        moveAndWait(dev, 0);
        assertThat(dev.getTiltAngle(), is(closeTo(0, 2)));
    }

    @Test(timeout = 5000)
    public void testLogEvents() throws InterruptedException {
        assumeThat(dev, is(not(nullValue())));

        ctx.setLogLevel(LogLevel.FLOOD);
        final List<String> messages = new ArrayList<String>();
        ctx.setLogHandler(new LogHandler() {
            @Override
            public void onMessage(Device dev, LogLevel level, String msg) {
                messages.add(msg);
            }
        });
        dev.startVideo(new VideoHandler() {
            @Override
            public void onFrameReceived(FrameMode mode, ByteBuffer frame, int timestamp) {
            }
        });
        Thread.sleep(500);
        dev.stopVideo();
        ctx.setLogLevel(LogLevel.SPEW);
        ctx.setLogHandler(new Jdk14LogHandler());
        assertThat(messages, is(not(IsEmptyCollection.<String>empty()))); // wtf hamcrest, fix this!
    }

    @Test(timeout = 2000)
    public void testDepth() throws InterruptedException {
        assumeThat(dev, is(not(nullValue())));

        final Object lock = new Object();
        final long start = System.nanoTime();
        dev.startDepth(new DepthHandler() {
            int frameCount = 0;

            @Override
            public void onFrameReceived(FrameMode mode, ByteBuffer frame, int timestamp) {
                frameCount++;
                if (frameCount == 30) {
                    synchronized (lock) {
                        lock.notify();
                        System.out.format("Got %d depth frames in %4.2fs%n", frameCount,
                                (((double) System.nanoTime() - start) / 1000000000));
                    }
                }
            }
        });
        synchronized (lock) {
            lock.wait(2000);
        }
    }

    @Test(timeout = 2000)
    public void testVideo() throws InterruptedException {
        assumeThat(dev, is(not(nullValue())));

        final Object lock = new Object();
        final long start = System.nanoTime();
        dev.startVideo(new VideoHandler() {
            int frameCount = 0;

            @Override
            public void onFrameReceived(FrameMode mode, ByteBuffer frame, int timestamp) {
                frameCount++;
                if (frameCount == 30) {
                    synchronized (lock) {
                        lock.notify();
                        System.out.format("Got %d video frames in %4.2fs%n", frameCount,
                                (((double) System.nanoTime() - start) / 1000000000));
                    }
                }
            }
        });
        synchronized (lock) {
            lock.wait(2000);
        }
    }
}
