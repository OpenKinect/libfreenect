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

import com.sun.jna.*;
import com.sun.jna.ptr.PointerByReference;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.DoubleBuffer;

public class Freenect implements Library {
	static {
		try {
			NativeLibrary.addSearchPath("freenect", "/usr/local/lib");
			NativeLibrary instance = NativeLibrary.getInstance("freenect");
			System.err.println("Loaded " + instance.getName() + " from " + instance.getFile().getCanonicalPath());
			Native.register(instance);
		} catch (IOException e) {
			throw new AssertionError(e);
		}
	}

	protected Freenect () {
	}

	public static Context createContext () {
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

		public NativeContext () {
		}

		private LogHandler logHandler;
		private final NativeLogCallback logCallback = new NativeLogCallback() {
			@Override
			public void callback (NativeDevice dev, int level, String msg) {
				logHandler.onMessage(dev, LogLevel.fromInt(level), msg);
			}
		};

		protected NativeContext (Pointer ptr) {
			super(ptr);
		}

		@Override
		public void setLogHandler (LogHandler handler) {
			this.logHandler = handler;
			if (logHandler == null) {
				freenect_set_log_callback(this, null);
			} else {
				freenect_set_log_callback(this, logCallback);
			}
		}

		@Override
		public void setLogLevel (LogLevel level) {
			freenect_set_log_level(this, level.intValue());
		}

		@Override
		public int numDevices () {
			return freenect_num_devices(this);
		}

		@Override
		public Device openDevice (int index) {
			PointerByReference devicePtr = new PointerByReference();
			int rval = freenect_open_device(this, devicePtr, index);
			if (rval != 0) {
				throw new IllegalStateException("freenect_open_device() returned " + rval);
			}
			return new NativeDevice(devicePtr.getValue(), index);
		}

		protected void processEvents () {
			freenect_process_events(this);
		}

		protected void startEventThread () {
			if (eventThread == null || !eventThread.isAlive()) {
				eventThread = new EventThread(this);
				eventThread.start();
			}
		}

		protected void stopEventThread () {
			if (eventThread != null) {
				eventThread.kill();
				eventThread = null;
			}
		}

		@Override
		public void shutdown () {
			stopEventThread();
			freenect_shutdown(this);
		}
	}

	protected static class NativeDevice extends PointerType implements Device {
		private FrameMode videoMode;
		private ByteBuffer videoBuffer;
		private VideoHandler videoHandler;

		private FrameMode depthMode;
		private ByteBuffer depthBuffer;
		private DepthHandler depthHandler;

		private final DoubleBuffer accelX = DoubleBuffer.allocate(1);
		private final DoubleBuffer accelY = DoubleBuffer.allocate(1);
		private final DoubleBuffer accelZ = DoubleBuffer.allocate(1);

		private int index;

		private TiltState rawTiltState;
		private TiltStatus tiltStatus;
		private double tiltAngle;
		private double[] accel;

		private final NativeVideoCallback videoCallback = new NativeVideoCallback() {
			@Override
			public void callback (Pointer dev, Pointer depth, int timestamp) {
				videoHandler.onFrameReceived(videoMode, videoBuffer, timestamp);
			}
		};

		private final NativeDepthCallback depthCallback = new NativeDepthCallback() {
			@Override
			public void callback (Pointer dev, Pointer depth, int timestamp) {
				depthHandler.onFrameReceived(depthMode, depthBuffer, timestamp);
			}
		};

		public NativeDevice () {
		}

		protected NativeDevice (Pointer ptr, int index) {
			super(ptr);
			this.index = index;
			this.rawTiltState = freenect_get_tilt_state(this);
			this.accel = new double[3];
			refreshTiltState();
			setVideoFormat(VideoFormat.RGB);
			setDepthFormat(DepthFormat.D10BIT);
		}

		protected void setDeviceIndex (int index) {
			this.index = index;
		}

		@Override
		public int getDeviceIndex () {
			return index;
		}

		public void close () {
			freenect_close_device(this);
		}

		@Override
		public void setDepthFormat (DepthFormat fmt) {
		    setDepthFormat(fmt, Resolution.MEDIUM);
		}

		@Override
		public void setVideoFormat (VideoFormat fmt) {
		    setVideoFormat(fmt, Resolution.MEDIUM);
		}

        @Override
        public void setDepthFormat (DepthFormat fmt, Resolution res) {
            FrameMode.ByValue mode = freenect_find_depth_mode(res.intValue(), fmt.intValue());
            if (mode.isValid()) {
				freenect_set_depth_mode(this, mode);
				depthBuffer = ByteBuffer.allocateDirect(mode.getFrameSize());
				freenect_set_depth_buffer(this, depthBuffer);
				this.depthMode = mode;
			}
        }

        @Override
        public void setVideoFormat (VideoFormat fmt, Resolution res) {
            FrameMode.ByValue mode = freenect_find_video_mode(res.intValue(), fmt.intValue());
			if (mode.isValid()) {
				freenect_set_video_mode(this, mode);
				videoBuffer = ByteBuffer.allocateDirect(mode.getFrameSize());
				freenect_set_video_buffer(this, videoBuffer);
				this.videoMode = mode;
			}
        }

        @Override
        public FrameMode getDepthMode() {
            return depthMode;
        }

        @Override
        public FrameMode getVideoMode() {
            return videoMode;
        }

		@Override
		public int setLed (LedStatus status) {
			return freenect_set_led(this, status.intValue());
		}

		@Override
		public void refreshTiltState () {
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
		public double getTiltAngle () {
			return tiltAngle;
		}

		@Override
		public int setTiltAngle (double angle) {
			return freenect_set_tilt_degs(this, angle);
		}

		@Override
		public TiltStatus getTiltStatus () {
			return tiltStatus;
		}

		@Override
		public double[] getAccel () {
			return new double[]{accelX.get(0), accelY.get(0), accelZ.get(0)};
		}

		@Override
		public int startVideo (VideoHandler handler) {
			this.videoHandler = handler;
			freenect_set_video_callback(this, videoCallback);
			return freenect_start_video(this);
		}

		@Override
		public int stopVideo () {
			int rval = freenect_stop_video(this);
			freenect_set_video_callback(this, null);
			this.videoHandler = null;
			return rval;
		}

		@Override
		public int startDepth (DepthHandler handler) {
			this.depthHandler = handler;
			freenect_set_depth_callback(this, depthCallback);
			return freenect_start_depth(this);
		}

		@Override
		public int stopDepth () {
			int rval = freenect_stop_depth(this);
			freenect_set_depth_callback(this, null);
			this.depthHandler = null;
			return rval;
		}
	}

	private static class EventThread extends Thread {
		private final NativeContext ctx;
		private volatile boolean alive = true;

		public EventThread (NativeContext ctx) {
			this.ctx = ctx;
			setDaemon(true);
			setName("FreenectEventThread");
		}

		public void kill () {
			this.alive = false;
		}

		@Override
		public void run () {
			while (alive) {
				freenect_process_events(ctx);
			}
		}
	}

	;

	protected static class TiltState extends PointerType {
		public TiltState () {
		}
	}

	// function prototypes from libfreenect.h
	// These must match the names used in the library!

	public interface NativeLogCallback extends Callback {
		void callback (NativeDevice dev, int logLevel, String msg);
	}

	private interface NativeDepthCallback extends Callback {
		void callback (Pointer dev, Pointer depth, int timestamp);
	}

	;

	private interface NativeVideoCallback extends Callback {
		void callback (Pointer dev, Pointer frame, int timestamp);
	}

	;

	private static native int freenect_init (PointerByReference ctx, Pointer usb_ctx);

	private static native int freenect_shutdown (NativeContext ctx);

	private static native void freenect_set_log_level (NativeContext ctx, int level);

	private static native void freenect_set_log_callback (NativeContext ctx, NativeLogCallback cb);

	private static native int freenect_process_events (NativeContext ctx);

	private static native int freenect_num_devices (NativeContext ctx);

	private static native int freenect_open_device (NativeContext ctx, PointerByReference dev, int index);

	private static native int freenect_close_device (NativeDevice dev);

	private static native void freenect_set_user (NativeDevice dev, Pointer user);

	private static native Pointer freenect_get_user (NativeDevice dev);

	private static native void freenect_set_depth_callback (NativeDevice dev, NativeDepthCallback cb);

	private static native void freenect_set_video_callback (NativeDevice dev, NativeVideoCallback cb);

	private static native int freenect_set_depth_buffer (NativeDevice dev, ByteBuffer buf);

	private static native int freenect_set_video_buffer (NativeDevice dev, ByteBuffer buf);

	private static native int freenect_start_depth (NativeDevice dev);

	private static native int freenect_start_video (NativeDevice dev);

	private static native int freenect_stop_depth (NativeDevice dev);

	private static native int freenect_stop_video (NativeDevice dev);

	private static native int freenect_update_tilt_state (NativeDevice dev);

	private static native TiltState freenect_get_tilt_state (NativeDevice dev);

	private static native byte freenect_get_tilt_status (TiltState tiltState);

	private static native double freenect_get_tilt_degs (TiltState tiltState);

	private static native int freenect_set_tilt_degs (NativeDevice dev, double angle);

	private static native int freenect_set_led (NativeDevice dev, int option);

	private static native void freenect_get_mks_accel (TiltState tiltState, DoubleBuffer x, DoubleBuffer y,
			DoubleBuffer z);

	private static native int freenect_select_subdevices (NativeContext ctx, int flag);

	private static native int freenect_get_video_mode_count ();

	private static native FrameMode.ByValue freenect_get_video_mode (int mode_num);

	private static native FrameMode.ByValue freenect_get_current_video_mode (NativeDevice dev);

	private static native FrameMode.ByValue freenect_find_video_mode (int res, int fmt);

	private static native int freenect_set_video_mode (NativeDevice dev, FrameMode.ByValue mode);

	private static native int freenect_get_depth_mode_count ();

	private static native FrameMode.ByValue freenect_get_depth_mode (int mode_num);

	private static native FrameMode.ByValue freenect_get_current_depth_mode (NativeDevice dev);

	private static native FrameMode.ByValue freenect_find_depth_mode (int res, int fmt);

	private static native int freenect_set_depth_mode (NativeDevice dev, FrameMode.ByValue mode);

}