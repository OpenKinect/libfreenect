package org.openkinect.freenect;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.greaterThanOrEqualTo;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.lessThanOrEqualTo;
import static org.hamcrest.Matchers.not;
import static org.hamcrest.Matchers.nullValue;
import static org.junit.Assert.assertThat;
import static org.junit.Assume.assumeThat;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.List;

import org.hamcrest.collection.IsEmptyCollection;
import org.junit.AfterClass;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.experimental.theories.Theories;
import org.junit.runner.RunWith;

@RunWith(Theories.class)
public class FreenectTest {
    static Context ctx;
    static Device dev;
    
    @BeforeClass
    public static void initKinect() {
        ctx = Freenect.createContext();
        if (ctx.numDevices() > 0) {
            dev = ctx.openDevice(0);
            ctx.processEventsBackground();
        } else {
            System.err.println("No kinects detected, cannot test!");
        }
    }
    
    @AfterClass
    public static void shutdownKinect() {
        ctx.shutdown();
    }

    @SuppressWarnings("unchecked")
    @Test 
    public void testTiltAngle() {
        assumeThat(dev, is(not(nullValue())));
        dev.refreshTitleState();
        assertThat(dev.getTiltAngle(), is(allOf(greaterThanOrEqualTo(-22d), lessThanOrEqualTo(22d))));
        // There's currently no way to access the tilt state to
        // wait for movement to complete, so no way to verify movements.
        
        //    assertThat(dev.setTiltAngle(0), is(0));
        //    dev.refreshTitleState();
        //    assertThat(dev.getTiltAngle(), is(closeTo(0, 1)));
        //
        //    assertThat(dev.setTiltAngle(20), is(0));
        //    dev.refreshTitleState();
        //    assertThat(dev.getTiltAngle(), is(closeTo(20, 1)));
        //
        //    assertThat(dev.setTiltAngle(-20), is(0));
        //    dev.refreshTitleState();
        //    assertThat(dev.getTiltAngle(), is(closeTo(-20, 1)));
    }
    
    @Test(timeout=5000)
    public void testLogEvents() throws InterruptedException {
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
            public void onFrameReceived(VideoFormat format, ByteBuffer frame, int timestamp) {
            }
        });
        Thread.sleep(500);
        dev.stopVideo();
        ctx.setLogLevel(LogLevel.INFO);
        ctx.setLogHandler(null);
        assertThat(messages, is(not(IsEmptyCollection.<String>empty()))); // wtf hamcrest, fix this!
    }

    @Test(timeout=2000)
    public void testVideo() throws InterruptedException {
        final Object lock = new Object();
        final long start = System.nanoTime(); 
        dev.startVideo(new VideoHandler() {
            int frameCount = 0;
            @Override
            public void onFrameReceived(VideoFormat format, ByteBuffer frame, int timestamp) {
                frameCount++;
                if (frameCount == 30) {
                    synchronized (lock) {
                        lock.notify();
                        System.out.format("Got %d frames in %4.2fs%n", frameCount, 
                                (((double) System.nanoTime() - start) / 1000000));
                    }
                }
            }
        });
        synchronized (lock) {
            lock.wait(2000);
        }
    }       
}
