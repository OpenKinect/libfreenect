package org.openkinect.freenect;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.DoubleBuffer;

import com.sun.jna.Callback;
import com.sun.jna.Library;
import com.sun.jna.Native;
import com.sun.jna.NativeLibrary;
import com.sun.jna.Pointer;
import com.sun.jna.PointerType;
import com.sun.jna.ptr.PointerByReference;

public class Freenect implements Library {
    static {
        try {
            NativeLibrary.addSearchPath("freenect", "../../build/lib");
            NativeLibrary.addSearchPath("freenect", "/usr/local/lib");
            NativeLibrary instance = NativeLibrary.getInstance("freenect");
            System.err.println("Loaded " + instance.getName() + " from " + instance.getFile().getCanonicalPath());
            Native.register(instance);
        } catch (IOException e) {
            throw new AssertionError(e);
        }
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
            NativeContext ctx = new NativeContext(ctxPtr.getValue());
            ctx.startEventThread();
            return ctx;
        }
        throw new IllegalStateException("init() returned " + rval);
    }

    protected static class NativeContext extends PointerType implements Context {
        private EventThread eventThread;

        public NativeContext() {}

        private LogHandler logHandler;
        private final NativeLogCallback logCallback = new NativeLogCallback() {
            @Override
            public void callback(NativeDevice dev, int level, String msg) {
                logHandler.onMessage(dev, LogLevel.fromInt(level), msg);
            }
        };

        protected NativeContext(Pointer ptr) {
            super(ptr);
        }

        @Override
        public void setLogHandler(LogHandler handler) {
            this.logHandler = handler;
            if (logHandler == null) {
                freenect_set_log_callback(this, null);
            } else {
                freenect_set_log_callback(this, logCallback);
            }
        }

        @Override
        public void setLogLevel(LogLevel level) {
            freenect_set_log_level(this, level.intValue());
        }

        @Override
        public int numDevices() {
            return freenect_num_devices(this);
        }

        @Override
        public Device openDevice(int index) {
            PointerByReference devicePtr = new PointerByReference();
            int rval = freenect_open_device(this, devicePtr, index);
            if (rval != 0) {
                throw new IllegalStateException("freenect_open_device() returned " + rval);
            }
            return new NativeDevice(devicePtr.getValue(), index);
        }

        protected void processEvents() {
            freenect_process_events(this);
        }

        protected void startEventThread() {
            if (eventThread == null || !eventThread.isAlive()) {
                eventThread = new EventThread(this);
                eventThread.start();
            }
        }

        protected void stopEventThread() {
            if (eventThread != null) {
                eventThread.kill();
                eventThread = null;
            }
        }

        @Override
        public void shutdown() {
            stopEventThread();
            freenect_shutdown(this);
        }
    }

    protected static class NativeDevice extends PointerType implements Device {
        private VideoFormat videoFormat;
        private ByteBuffer videoBuffer;
        private DepthFormat depthFormat;
        private ByteBuffer depthBuffer;
        private final DoubleBuffer accelX = DoubleBuffer.allocate(1);
        private final DoubleBuffer accelY = DoubleBuffer.allocate(1);
        private final DoubleBuffer accelZ = DoubleBuffer.allocate(1);
        private VideoHandler videoHandler;
        private DepthHandler depthHandler;
        private int index;

        private TiltState rawTiltState;
        private TiltStatus tiltStatus;
        private double tiltAngle;
        private double[] accel;

        private final NativeVideoCallback videoCallback = new NativeVideoCallback() {
            @Override
            public void callback(Pointer dev, Pointer depth, int timestamp) {
                videoHandler.onFrameReceived(videoFormat, videoBuffer, timestamp);
            }
        };

        private final NativeDepthCallback depthCallback = new NativeDepthCallback() {
            @Override
            public void callback(Pointer dev, Pointer depth, int timestamp) {
                depthHandler.onFrameReceived(depthFormat, depthBuffer, timestamp);
            }
        };

        public NativeDevice() {}

        protected NativeDevice(Pointer ptr, int index) {
            super(ptr);
            this.index = index;
            this.rawTiltState = freenect_get_tilt_state(this);
            this.accel = new double[3];
            refreshTiltState();
            setVideoFormat(VideoFormat.RGB);
            setDepthFormat(DepthFormat.D11BIT);
        }

        protected void setDeviceIndex(int index) {
            this.index = index;
        }

        @Override
        public int getDeviceIndex() {
            return index;
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
        public void refreshTiltState() {
            freenect_update_tilt_state(this);
            this.rawTiltState = freenect_get_tilt_state(this);
            this.tiltAngle = freenect_get_tilt_degs(rawTiltState);
            this.tiltStatus = TiltStatus.fromInt(freenect_get_tilt_status(rawTiltState));
            freenect_get_mks_accel(rawTiltState, accelX, accelY, accelZ);
            this.accel[0] = accelX.get(0);
            this.accel[1] = accelY.get(0);
            this.accel[2] = accelZ.get(0);
        }

        @Override
        public double getTiltAngle() {
            return tiltAngle;
        }

        @Override
        public int setTiltAngle(double angle) {
            return freenect_set_tilt_degs(this, angle);
        }

        @Override
        public TiltStatus getTiltStatus() {
            return tiltStatus;
        }

        @Override
        public double[] getAccel() {
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

    protected static class TiltState extends PointerType {
        public TiltState() {}
    }

    // function prototypes from libfreenect.h
    // These must match the names used in the library!

    public interface NativeLogCallback extends Callback {
        void callback(NativeDevice dev, int logLevel, String msg);
    }

    private interface NativeDepthCallback extends Callback {
        void callback(Pointer dev, Pointer depth, int timestamp);
    };

    private interface NativeVideoCallback extends Callback {
        void callback(Pointer dev, Pointer frame, int timestamp);
    };

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
    private static native TiltState freenect_get_tilt_state(NativeDevice dev);
    private static native byte freenect_get_tilt_status(TiltState tiltState);
    private static native double freenect_get_tilt_degs(TiltState tiltState);
    private static native int freenect_set_tilt_degs(NativeDevice dev, double angle);
    private static native int freenect_set_led(NativeDevice dev, int option);
    private static native void freenect_get_mks_accel(TiltState tiltState, DoubleBuffer x, DoubleBuffer y, DoubleBuffer z);
}
