package org.openkinect.freenect;

import java.nio.ByteBuffer;
import java.nio.DoubleBuffer;


import com.ochafik.lang.jnaerator.runtime.LibraryExtractor;
import com.sun.jna.Callback;
import com.sun.jna.Library;
import com.sun.jna.Native;
import com.sun.jna.NativeLibrary;
import com.sun.jna.Pointer;
import com.sun.jna.PointerType;
import com.sun.jna.ptr.PointerByReference;

public class Freenect implements Library {
    public static final java.lang.String JNA_LIBRARY_NAME = LibraryExtractor.getLibraryPath("freenect", true, org.openkinect.freenect.Freenect.class);
    public static final NativeLibrary JNA_NATIVE_LIB = NativeLibrary.getInstance(org.openkinect.freenect.Freenect.JNA_LIBRARY_NAME, com.ochafik.lang.jnaerator.runtime.MangledFunctionMapper.DEFAULT_OPTIONS);
    static {
        Native.register(org.openkinect.freenect.Freenect.JNA_LIBRARY_NAME);
    }

    // constants from libfreenect.h
    static final int FREENECT_FRAME_W = 640;
    static final int FREENECT_FRAME_H = 480;
    static final int FREENECT_FRAME_PIX = (FREENECT_FRAME_H * FREENECT_FRAME_W);
    static final int FREENECT_IR_FRAME_W = 640;
    static final int FREENECT_IR_FRAME_H = 488;
    static final int FREENECT_IR_FRAME_PIX = (FREENECT_IR_FRAME_H * FREENECT_IR_FRAME_W);
    static final int FREENECT_VIDEO_RGB_SIZE = (FREENECT_FRAME_PIX * 3);
    static final int FREENECT_VIDEO_BAYER_SIZE = (FREENECT_FRAME_PIX);
    static final int FREENECT_VIDEO_YUV_SIZE = (FREENECT_FRAME_PIX * 2);
    static final int FREENECT_VIDEO_IR_8BIT_SIZE = (FREENECT_IR_FRAME_PIX);
    static final int FREENECT_VIDEO_IR_10BIT_SIZE = (FREENECT_IR_FRAME_PIX * 2);
    static final int FREENECT_VIDEO_IR_10BIT_PACKED_SIZE = 390400;
    static final int FREENECT_DEPTH_11BIT_SIZE = (FREENECT_FRAME_PIX * 2);
    static final int FREENECT_DEPTH_10BIT_SIZE = FREENECT_DEPTH_11BIT_SIZE;
    static final int FREENECT_DEPTH_11BIT_PACKED_SIZE = 422400;
    static final int FREENECT_DEPTH_10BIT_PACKED_SIZE = 384000;

    protected Freenect() {}

    public static Context createContext() {
        PointerByReference ctxPtr = new PointerByReference();
        int rval = freenect_init(ctxPtr, Pointer.NULL);
        if (rval == 0) {
            return new NativeContext(ctxPtr.getValue());
        }
        throw new IllegalStateException("init() returned " + rval);
    }

    public interface NativeLogCallback extends Callback {
        void callback(Device dev, int logLevel, String msg);
    }
    
    protected static class NativeContext extends PointerType implements Context {
        private EventThread eventThread;

        public NativeContext() {}

        private LogHandler logHandler;
        private final NativeLogCallback logCallback = new NativeLogCallback() {
            @Override
            public void callback(Device dev, int level, String msg) {
                logHandler.onMessage(dev, LogLevel.fromInt(level), msg);
            }
        };
        
        protected NativeContext(Pointer ptr) {
            super(ptr);
        }

        public void setLogHandler(LogHandler handler) {
            this.logHandler = handler;
            if (logHandler == null) {
                freenect_set_log_callback(this, null);
            } else {
                freenect_set_log_callback(this, logCallback);
            }
        }
        
        public void setLogLevel(LogLevel level) {
            freenect_set_log_level(this, level.intValue());
        }
        
        public int numDevices() {
            return freenect_num_devices(this);
        }

        public Device openDevice(int index) {
            PointerByReference devicePtr = new PointerByReference();
            int rval = freenect_open_device(this, devicePtr, index);
            if (rval != 0) {
                throw new IllegalStateException("freenect_open_device() returned " + rval);
            }
            return new NativeDevice(devicePtr.getValue());
        }

        public void closeDevice(Device dev) {
            freenect_close_device((NativeDevice) dev);
        }

        public void processEvents() {
            freenect_process_events(this);
        }

        public void processEventsBackground() {
            if (eventThread == null || !eventThread.isAlive()) {
                eventThread = new EventThread(this);
                eventThread.start();
            }
        }

        @Override
        public void stopEventThread() {
            eventThread.kill();
        }
    }

    private interface NativeDepthCallback extends Callback {
        void invoke(Pointer dev, Pointer depth, int timestamp);
    };

    private interface NativeVideoCallback extends Callback {
        void invoke(Pointer dev, Pointer frame, int timestamp);
    };

    protected static class NativeDevice extends PointerType implements Device {
        private Pointer tiltState;
        private VideoFormat videoFormat;
        private ByteBuffer videoBuffer;
        private DepthFormat depthFormat;
        private ByteBuffer depthBuffer;
        private final DoubleBuffer accelX = DoubleBuffer.allocate(1);
        private final DoubleBuffer accelY = DoubleBuffer.allocate(1);
        private final DoubleBuffer accelZ = DoubleBuffer.allocate(1);
        private VideoHandler videoHandler;
        private DepthHandler depthHandler;

        private final NativeVideoCallback videoCallback = new NativeVideoCallback() {
            @Override
            public void invoke(Pointer dev, Pointer depth, int timestamp) {
                videoHandler.onFrameReceived(videoFormat, videoBuffer, timestamp);
            }
        };

        private final NativeDepthCallback depthCallback = new NativeDepthCallback() {
            @Override
            public void invoke(Pointer dev, Pointer depth, int timestamp) {
                depthHandler.onFrameReceived(depthFormat, depthBuffer, timestamp);
            }
        };

        public NativeDevice() {}

        protected NativeDevice(Pointer ptr) {
            super(ptr);
            tiltState = freenect_get_tilt_state(this);
            setVideoFormat(VideoFormat.RGB);
            setDepthFormat(DepthFormat.D11BIT);
        }

        public void close() {
            freenect_close_device(this);
        }

        @Override
        public void setDepthFormat(DepthFormat fmt) {
            freenect_set_depth_format(this, fmt.intValue());
            depthBuffer = ByteBuffer.allocateDirect(fmt.getFrameSize());
            freenect_set_depth_buffer(this, depthBuffer);
            this.depthFormat = fmt;
        }

        @Override
        public void setVideoFormat(VideoFormat fmt) {
            freenect_set_video_format(this, fmt.intValue());
            videoBuffer = ByteBuffer.allocateDirect(fmt.getFrameSize());
            freenect_set_video_buffer(this, videoBuffer);
            this.videoFormat = fmt;
        }

        @Override
        public int setLed(LedStatus status) {
            return freenect_set_led(this, status.intValue());
        }

        @Override
        public void refreshTitleState() {
            freenect_update_tilt_state(this);
        }
        
        @Override
        public double getTiltAngle() {
            // TODO should this get called automatically?
            // freenect_update_tilt_state(this);
            return freenect_get_tilt_degs(tiltState);
        }

        @Override
        public int setTiltAngle(double angle) {
            return freenect_set_tilt_degs(this, angle);
        }

        @Override
        public TiltStatus getTiltStatus() {
            // TODO not exposed by freenect
            return TiltStatus.STOPPED;
        }

        @Override
        public double[] getAccel() {
            // TODO should this get called automatically?
            // freenect_update_tilt_state(this);
            freenect_get_mks_accel(tiltState, accelX, accelY, accelZ);
            return new double[] { accelX.get(0), accelY.get(0), accelZ.get(0) };
        }

        @Override
        public int startVideo(VideoHandler handler) {
            this.videoHandler = handler;
            freenect_set_video_callback(this, videoCallback);
            return freenect_start_video(this);
        }

        @Override
        public int stopVideo() {
            int rval = freenect_stop_video(this);
            freenect_set_video_callback(this, null);
            this.videoHandler = null;
            return rval;
        }

        @Override
        public int startDepth(DepthHandler handler) {
            this.depthHandler = handler;
            freenect_set_depth_callback(this, depthCallback);
            return freenect_start_depth(this);
        }

        @Override
        public int stopDepth() {
            int rval = freenect_stop_depth(this);
            freenect_set_depth_callback(this, null);
            this.depthHandler = null;
            return rval;
        }
    }

    private static class EventThread extends Thread {
        private final NativeContext ctx;
        private volatile boolean alive = true;

        public EventThread(NativeContext ctx) {
            this.ctx = ctx;
            setDaemon(true);
            setName("FreenectEventThread");
        }

        public void kill() {
            this.alive = false;
        }

        @Override
        public void run() {
            while (alive) {
                freenect_process_events(ctx);
            }
        }
    };
    
    // function prototypes from libfreenect.h
    // These must match the names used in the library!

    private static native int freenect_init(PointerByReference ctx, Pointer usb_ctx);
    private static native int freenect_shutdown(NativeContext ctx);
    private static native void freenect_set_log_level(NativeContext ctx, int level);
    private static native void freenect_set_log_callback(NativeContext ctx, NativeLogCallback cb);
    private static native int freenect_process_events(NativeContext ctx);
    private static native int freenect_num_devices(NativeContext ctx);
    private static native int freenect_open_device(NativeContext ctx, PointerByReference dev, int index);
    private static native int freenect_close_device(NativeDevice dev);
    private static native void freenect_set_user(NativeDevice dev, Pointer user);
    private static native Pointer freenect_get_user(NativeDevice dev);
    private static native void freenect_set_depth_callback(NativeDevice dev, NativeDepthCallback cb);
    private static native void freenect_set_video_callback(NativeDevice dev, NativeVideoCallback cb);
    private static native int freenect_set_depth_format(NativeDevice dev, int i);
    private static native int freenect_set_video_format(NativeDevice dev, int i);
    private static native int freenect_set_depth_buffer(NativeDevice dev, ByteBuffer buf);
    private static native int freenect_set_video_buffer(NativeDevice dev, ByteBuffer buf);
    private static native int freenect_start_depth(NativeDevice dev);
    private static native int freenect_start_video(NativeDevice dev);
    private static native int freenect_stop_depth(NativeDevice dev);
    private static native int freenect_stop_video(NativeDevice dev);
    private static native int freenect_update_tilt_state(NativeDevice dev);
    private static native Pointer freenect_get_tilt_state(NativeDevice dev);
    private static native double freenect_get_tilt_degs(Pointer tiltState);
    private static native int freenect_set_tilt_degs(NativeDevice dev, double angle);
    private static native int freenect_set_led(NativeDevice dev, int option);
    private static native void freenect_get_mks_accel(Pointer tiltState, DoubleBuffer x, DoubleBuffer y, DoubleBuffer z);
}
