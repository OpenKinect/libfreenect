/*
 * This file is part of the OpenKinect Project. http://www.openkinect.org
 *
 * Copyright (c) 2026 individual OpenKinect contributors. See the CONTRIB file
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

/*
 * KinectBeamMic.driver: a Core Audio server plug-in (user space, runs in
 * coreaudiod) that publishes the Kinect's beamformed microphone.
 *
 * A HAL plug-in cannot itself capture another device, so the work is split:
 * the helper app (freenect-coreaudio-mic) captures the Kinect's 4
 * microphones, beamforms them with libfreenect and plays the result into
 * this plug-in's hidden sink device. Whatever reaches the sink comes out of
 * the plug-in's microphone device, "Kinect Beamformed Microphone", which
 * applications pick like any other microphone.
 *
 * Objects: the plug-in (1), the sink device (2) with its output stream (3),
 * the microphone device (4) with its input stream (5). Both devices run at
 * 16 kHz, mono, 32-bit float, on one clock, so that a sample the sink writes
 * at sample time t is read by the microphone at sample time t.
 */

#include <CoreAudio/AudioServerPlugIn.h>
#include <CoreFoundation/CoreFoundation.h>
#include <mach/mach_time.h>
#include <pthread.h>
#include <stdatomic.h>
#include <string.h>

#define RATE 16000.0
#define RING_FRAMES 16384u          // also the zero time stamp period
#define LATENCY_FRAMES 0u
#define SAFETY_FRAMES 0u

enum {
	ID_PLUGIN = kAudioObjectPlugInObject, // 1
	ID_SINK = 2,
	ID_SINK_STREAM = 3,
	ID_MIC = 4,
	ID_MIC_STREAM = 5,
};

#define SINK_UID "org.openkinect.beam-sink"
#define MIC_UID "org.openkinect.beam-mic"
#define MODEL_UID "org.openkinect.beam-mic.model"
#define MIC_NAME "Kinect Beamformed Microphone"
#define SINK_NAME "Kinect Beamformer Input"
#define MANUFACTURER "OpenKinect"

/* ------------------------------------------------------------ state */

static AudioServerPlugInHostRef g_host;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static UInt32 g_refcount = 0;

// IO: how many clients run IO on each device, and the clock both share
static UInt32 g_io_running[2];          // sink, mic
static UInt64 g_anchor_host_time;
static UInt64 g_timestamp_count;
static Float64 g_host_ticks_per_frame;

// the loopback ring, indexed by sample time
static float g_ring[RING_FRAMES];
static _Atomic Float64 g_written_end = 0.0; // the sink wrote everything before this sample time
static _Atomic Float64 g_written_begin = 0.0;

static int is_device(AudioObjectID id) { return id == ID_SINK || id == ID_MIC; }
static int is_stream(AudioObjectID id) { return id == ID_SINK_STREAM || id == ID_MIC_STREAM; }
static int dev_index(AudioObjectID id) { return id == ID_SINK ? 0 : 1; }

static AudioStreamBasicDescription format(void)
{
	AudioStreamBasicDescription f;
	memset(&f, 0, sizeof(f));
	f.mSampleRate = RATE;
	f.mFormatID = kAudioFormatLinearPCM;
	f.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked;
	f.mBytesPerPacket = sizeof(float);
	f.mFramesPerPacket = 1;
	f.mBytesPerFrame = sizeof(float);
	f.mChannelsPerFrame = 1;
	f.mBitsPerChannel = 32;
	return f;
}

/* ------------------------------------------------------------ COM boilerplate */

static HRESULT plugin_query_interface(void* driver, REFIID uuid, LPVOID* out);
static ULONG plugin_add_ref(void* driver);
static ULONG plugin_release(void* driver);
static OSStatus plugin_initialize(AudioServerPlugInDriverRef driver, AudioServerPlugInHostRef host);
static OSStatus plugin_create_device(AudioServerPlugInDriverRef driver, CFDictionaryRef desc,
                                     const AudioServerPlugInClientInfo* client, AudioObjectID* out);
static OSStatus plugin_destroy_device(AudioServerPlugInDriverRef driver, AudioObjectID id);
static OSStatus plugin_add_device_client(AudioServerPlugInDriverRef driver, AudioObjectID id, const AudioServerPlugInClientInfo* client);
static OSStatus plugin_remove_device_client(AudioServerPlugInDriverRef driver, AudioObjectID id,
                                            const AudioServerPlugInClientInfo* client);
static OSStatus plugin_perform_config_change(AudioServerPlugInDriverRef driver, AudioObjectID id, UInt64 action, void* info);
static OSStatus plugin_abort_config_change(AudioServerPlugInDriverRef driver, AudioObjectID id, UInt64 action, void* info);
static Boolean has_property(AudioServerPlugInDriverRef driver, AudioObjectID id, pid_t pid, const AudioObjectPropertyAddress* a);
static OSStatus is_property_settable(AudioServerPlugInDriverRef driver, AudioObjectID id, pid_t pid,
                                     const AudioObjectPropertyAddress* a, Boolean* out);
static OSStatus get_property_data_size(AudioServerPlugInDriverRef driver, AudioObjectID id, pid_t pid,
                                       const AudioObjectPropertyAddress* a, UInt32 qsize, const void* qdata, UInt32* out);
static OSStatus get_property_data(AudioServerPlugInDriverRef driver, AudioObjectID id, pid_t pid, const AudioObjectPropertyAddress* a,
                                  UInt32 qsize, const void* qdata, UInt32 size, UInt32* out_size, void* out);
static OSStatus set_property_data(AudioServerPlugInDriverRef driver, AudioObjectID id, pid_t pid, const AudioObjectPropertyAddress* a,
                                  UInt32 qsize, const void* qdata, UInt32 size, const void* data);
static OSStatus start_io(AudioServerPlugInDriverRef driver, AudioObjectID id, UInt32 client);
static OSStatus stop_io(AudioServerPlugInDriverRef driver, AudioObjectID id, UInt32 client);
static OSStatus get_zero_time_stamp(AudioServerPlugInDriverRef driver, AudioObjectID id, UInt32 client, Float64* sample_time,
                                    UInt64* host_time, UInt64* seed);
static OSStatus will_do_io(AudioServerPlugInDriverRef driver, AudioObjectID id, UInt32 client, UInt32 op, Boolean* will,
                           Boolean* in_place);
static OSStatus begin_io(AudioServerPlugInDriverRef driver, AudioObjectID id, UInt32 client, UInt32 op, UInt32 frames,
                         const AudioServerPlugInIOCycleInfo* cycle);
static OSStatus do_io(AudioServerPlugInDriverRef driver, AudioObjectID id, AudioObjectID stream, UInt32 client, UInt32 op,
                      UInt32 frames, const AudioServerPlugInIOCycleInfo* cycle, void* main, void* secondary);
static OSStatus end_io(AudioServerPlugInDriverRef driver, AudioObjectID id, UInt32 client, UInt32 op, UInt32 frames,
                       const AudioServerPlugInIOCycleInfo* cycle);

static AudioServerPlugInDriverInterface g_interface = {
	NULL,
	plugin_query_interface,
	plugin_add_ref,
	plugin_release,
	plugin_initialize,
	plugin_create_device,
	plugin_destroy_device,
	plugin_add_device_client,
	plugin_remove_device_client,
	plugin_perform_config_change,
	plugin_abort_config_change,
	has_property,
	is_property_settable,
	get_property_data_size,
	get_property_data,
	set_property_data,
	start_io,
	stop_io,
	get_zero_time_stamp,
	will_do_io,
	begin_io,
	do_io,
	end_io,
};
static AudioServerPlugInDriverInterface* g_interface_ptr = &g_interface;
static AudioServerPlugInDriverRef g_driver = &g_interface_ptr;

// The factory named in Info.plist (CFPlugInFactories)
__attribute__((visibility("default"))) void* KinectBeamMic_Create(CFAllocatorRef allocator, CFUUIDRef type);

void* KinectBeamMic_Create(CFAllocatorRef allocator, CFUUIDRef type)
{
	(void)allocator;
	if (!CFEqual(type, kAudioServerPlugInTypeUUID))
		return NULL;
	return g_driver;
}

static HRESULT plugin_query_interface(void* driver, REFIID uuid, LPVOID* out)
{
	CFUUIDRef requested;
	HRESULT r = E_NOINTERFACE;
	if (driver != g_driver || !out)
		return kAudioHardwareBadObjectError;
	requested = CFUUIDCreateFromUUIDBytes(NULL, uuid);
	if (!requested)
		return kAudioHardwareIllegalOperationError;
	if (CFEqual(requested, IUnknownUUID) || CFEqual(requested, kAudioServerPlugInDriverInterfaceUUID)) {
		pthread_mutex_lock(&g_lock);
		g_refcount++;
		pthread_mutex_unlock(&g_lock);
		*out = g_driver;
		r = S_OK;
	}
	CFRelease(requested);
	return r;
}

static ULONG plugin_add_ref(void* driver)
{
	ULONG r;
	if (driver != g_driver)
		return 0;
	pthread_mutex_lock(&g_lock);
	r = ++g_refcount;
	pthread_mutex_unlock(&g_lock);
	return r;
}

static ULONG plugin_release(void* driver)
{
	ULONG r;
	if (driver != g_driver)
		return 0;
	pthread_mutex_lock(&g_lock);
	if (g_refcount > 0)
		g_refcount--;
	r = g_refcount;
	pthread_mutex_unlock(&g_lock);
	return r;
}

static OSStatus plugin_initialize(AudioServerPlugInDriverRef driver, AudioServerPlugInHostRef host)
{
	struct mach_timebase_info tb;
	if (driver != g_driver)
		return kAudioHardwareBadObjectError;
	g_host = host;
	mach_timebase_info(&tb);
	// host ticks per frame: ns per second / rate, in host ticks
	g_host_ticks_per_frame = (1.0e9 / RATE) * (Float64)tb.denom / (Float64)tb.numer;
	return kAudioHardwareNoError;
}

static OSStatus plugin_create_device(AudioServerPlugInDriverRef driver, CFDictionaryRef desc, const AudioServerPlugInClientInfo* client,
                                     AudioObjectID* out)
{
	(void)driver; (void)desc; (void)client; (void)out;
	return kAudioHardwareUnsupportedOperationError;
}

static OSStatus plugin_destroy_device(AudioServerPlugInDriverRef driver, AudioObjectID id)
{
	(void)driver; (void)id;
	return kAudioHardwareUnsupportedOperationError;
}

static OSStatus plugin_add_device_client(AudioServerPlugInDriverRef driver, AudioObjectID id, const AudioServerPlugInClientInfo* client)
{
	(void)client;
	return driver == g_driver && is_device(id) ? kAudioHardwareNoError : kAudioHardwareBadObjectError;
}

static OSStatus plugin_remove_device_client(AudioServerPlugInDriverRef driver, AudioObjectID id, const AudioServerPlugInClientInfo* client)
{
	(void)client;
	return driver == g_driver && is_device(id) ? kAudioHardwareNoError : kAudioHardwareBadObjectError;
}

static OSStatus plugin_perform_config_change(AudioServerPlugInDriverRef driver, AudioObjectID id, UInt64 action, void* info)
{
	(void)action; (void)info;
	return driver == g_driver && is_device(id) ? kAudioHardwareNoError : kAudioHardwareBadObjectError;
}

static OSStatus plugin_abort_config_change(AudioServerPlugInDriverRef driver, AudioObjectID id, UInt64 action, void* info)
{
	(void)action; (void)info;
	return driver == g_driver && is_device(id) ? kAudioHardwareNoError : kAudioHardwareBadObjectError;
}

/* ------------------------------------------------------------ properties */

static Boolean has_property(AudioServerPlugInDriverRef driver, AudioObjectID id, pid_t pid, const AudioObjectPropertyAddress* a)
{
	UInt32 size = 0;
	if (driver != g_driver || !a)
		return false;
	// a property exists exactly when its size can be told
	return get_property_data_size(driver, id, pid, a, 0, NULL, &size) == kAudioHardwareNoError;
}

static OSStatus is_property_settable(AudioServerPlugInDriverRef driver, AudioObjectID id, pid_t pid,
                                     const AudioObjectPropertyAddress* a, Boolean* out)
{
	if (!has_property(driver, id, pid, a) || !out)
		return kAudioHardwareUnknownPropertyError;
	// the format and the rate are fixed, but clients may "set" them to what
	// they are
	*out = (is_device(id) && a->mSelector == kAudioDevicePropertyNominalSampleRate) ||
	       (is_stream(id) && (a->mSelector == kAudioStreamPropertyVirtualFormat || a->mSelector == kAudioStreamPropertyPhysicalFormat));
	return kAudioHardwareNoError;
}

// the streams a device has in a scope
static UInt32 device_streams(AudioObjectID dev, AudioObjectPropertyScope scope, AudioObjectID* out)
{
	int input = dev == ID_MIC;
	if (scope == kAudioObjectPropertyScopeGlobal || (input && scope == kAudioObjectPropertyScopeInput) ||
	    (!input && scope == kAudioObjectPropertyScopeOutput)) {
		if (out)
			out[0] = input ? ID_MIC_STREAM : ID_SINK_STREAM;
		return 1;
	}
	return 0;
}

static OSStatus get_property_data_size(AudioServerPlugInDriverRef driver, AudioObjectID id, pid_t pid,
                                       const AudioObjectPropertyAddress* a, UInt32 qsize, const void* qdata, UInt32* out)
{
	UInt32 n;
	(void)pid; (void)qsize; (void)qdata;
	if (driver != g_driver || !a || !out)
		return kAudioHardwareBadObjectError;
	if (id == ID_PLUGIN) {
		switch (a->mSelector) {
		case kAudioObjectPropertyBaseClass:
		case kAudioObjectPropertyClass: *out = sizeof(AudioClassID); return 0;
		case kAudioObjectPropertyOwner: *out = sizeof(AudioObjectID); return 0;
		case kAudioObjectPropertyManufacturer: *out = sizeof(CFStringRef); return 0;
		case kAudioObjectPropertyOwnedObjects:
		case kAudioPlugInPropertyDeviceList: *out = 2 * sizeof(AudioObjectID); return 0;
		case kAudioPlugInPropertyBoxList: *out = 0; return 0;
		case kAudioPlugInPropertyTranslateUIDToDevice:
		case kAudioPlugInPropertyTranslateUIDToBox: *out = sizeof(AudioObjectID); return 0;
		case kAudioPlugInPropertyResourceBundle: *out = sizeof(CFStringRef); return 0;
		}
		return kAudioHardwareUnknownPropertyError;
	}
	if (is_device(id)) {
		switch (a->mSelector) {
		case kAudioObjectPropertyBaseClass:
		case kAudioObjectPropertyClass: *out = sizeof(AudioClassID); return 0;
		case kAudioObjectPropertyOwner: *out = sizeof(AudioObjectID); return 0;
		case kAudioObjectPropertyName:
		case kAudioObjectPropertyManufacturer:
		case kAudioDevicePropertyDeviceUID:
		case kAudioDevicePropertyModelUID: *out = sizeof(CFStringRef); return 0;
		case kAudioObjectPropertyOwnedObjects:
		case kAudioDevicePropertyStreams:
			n = device_streams(id, a->mScope, NULL);
			*out = n * sizeof(AudioObjectID);
			return 0;
		case kAudioObjectPropertyControlList: *out = 0; return 0;
		case kAudioDevicePropertyTransportType:
		case kAudioDevicePropertyClockDomain:
		case kAudioDevicePropertyDeviceIsAlive:
		case kAudioDevicePropertyDeviceIsRunning:
		case kAudioDevicePropertyDeviceCanBeDefaultDevice:
		case kAudioDevicePropertyDeviceCanBeDefaultSystemDevice:
		case kAudioDevicePropertyLatency:
		case kAudioDevicePropertySafetyOffset:
		case kAudioDevicePropertyIsHidden:
		case kAudioDevicePropertyZeroTimeStampPeriod:
		case kAudioDevicePropertyClockIsStable: *out = sizeof(UInt32); return 0;
		case kAudioDevicePropertyRelatedDevices: *out = sizeof(AudioObjectID); return 0;
		case kAudioDevicePropertyNominalSampleRate: *out = sizeof(Float64); return 0;
		case kAudioDevicePropertyAvailableNominalSampleRates: *out = sizeof(AudioValueRange); return 0;
		case kAudioDevicePropertyPreferredChannelsForStereo: *out = 2 * sizeof(UInt32); return 0;
		}
		return kAudioHardwareUnknownPropertyError;
	}
	if (is_stream(id)) {
		switch (a->mSelector) {
		case kAudioObjectPropertyBaseClass:
		case kAudioObjectPropertyClass: *out = sizeof(AudioClassID); return 0;
		case kAudioObjectPropertyOwner: *out = sizeof(AudioObjectID); return 0;
		case kAudioObjectPropertyName: *out = sizeof(CFStringRef); return 0;
		case kAudioObjectPropertyOwnedObjects: *out = 0; return 0;
		case kAudioStreamPropertyIsActive:
		case kAudioStreamPropertyDirection:
		case kAudioStreamPropertyTerminalType:
		case kAudioStreamPropertyStartingChannel:
		case kAudioStreamPropertyLatency: *out = sizeof(UInt32); return 0;
		case kAudioStreamPropertyVirtualFormat:
		case kAudioStreamPropertyPhysicalFormat: *out = sizeof(AudioStreamBasicDescription); return 0;
		case kAudioStreamPropertyAvailableVirtualFormats:
		case kAudioStreamPropertyAvailablePhysicalFormats: *out = sizeof(AudioStreamRangedDescription); return 0;
		}
		return kAudioHardwareUnknownPropertyError;
	}
	return kAudioHardwareBadObjectError;
}

#define NEED(T)                                       \
	do {                                              \
		if (size < sizeof(T))                         \
			return kAudioHardwareBadPropertySizeError; \
		*out_size = sizeof(T);                        \
	} while (0)

static CFStringRef cfstr(const char* s)
{
	return CFStringCreateWithCString(NULL, s, kCFStringEncodingUTF8); // the caller releases it
}

static OSStatus get_property_data(AudioServerPlugInDriverRef driver, AudioObjectID id, pid_t pid, const AudioObjectPropertyAddress* a,
                                  UInt32 qsize, const void* qdata, UInt32 size, UInt32* out_size, void* out)
{
	(void)pid;
	if (driver != g_driver || !a || !out_size || !out)
		return kAudioHardwareBadObjectError;

	if (id == ID_PLUGIN) {
		switch (a->mSelector) {
		case kAudioObjectPropertyBaseClass:
			NEED(AudioClassID);
			*(AudioClassID*)out = kAudioObjectClassID;
			return 0;
		case kAudioObjectPropertyClass:
			NEED(AudioClassID);
			*(AudioClassID*)out = kAudioPlugInClassID;
			return 0;
		case kAudioObjectPropertyOwner:
			NEED(AudioObjectID);
			*(AudioObjectID*)out = kAudioObjectUnknown;
			return 0;
		case kAudioObjectPropertyManufacturer:
			NEED(CFStringRef);
			*(CFStringRef*)out = cfstr(MANUFACTURER);
			return 0;
		case kAudioObjectPropertyOwnedObjects:
		case kAudioPlugInPropertyDeviceList: {
			AudioObjectID* ids = out;
			UInt32 n = size / sizeof(AudioObjectID), k = 0;
			if (k < n) ids[k++] = ID_SINK;
			if (k < n) ids[k++] = ID_MIC;
			*out_size = k * sizeof(AudioObjectID);
			return 0;
		}
		case kAudioPlugInPropertyBoxList:
			*out_size = 0;
			return 0;
		case kAudioPlugInPropertyTranslateUIDToBox:
			NEED(AudioObjectID);
			*(AudioObjectID*)out = kAudioObjectUnknown;
			return 0;
		case kAudioPlugInPropertyTranslateUIDToDevice: {
			CFStringRef uid;
			AudioObjectID found = kAudioObjectUnknown;
			NEED(AudioObjectID);
			if (qsize != sizeof(CFStringRef) || !qdata)
				return kAudioHardwareBadPropertySizeError;
			uid = *(const CFStringRef*)qdata;
			if (uid && CFStringCompare(uid, CFSTR(MIC_UID), 0) == kCFCompareEqualTo)
				found = ID_MIC;
			else if (uid && CFStringCompare(uid, CFSTR(SINK_UID), 0) == kCFCompareEqualTo)
				found = ID_SINK;
			*(AudioObjectID*)out = found;
			return 0;
		}
		case kAudioPlugInPropertyResourceBundle:
			NEED(CFStringRef);
			*(CFStringRef*)out = cfstr("");
			return 0;
		}
		return kAudioHardwareUnknownPropertyError;
	}

	if (is_device(id)) {
		int mic = id == ID_MIC;
		switch (a->mSelector) {
		case kAudioObjectPropertyBaseClass:
			NEED(AudioClassID);
			*(AudioClassID*)out = kAudioObjectClassID;
			return 0;
		case kAudioObjectPropertyClass:
			NEED(AudioClassID);
			*(AudioClassID*)out = kAudioDeviceClassID;
			return 0;
		case kAudioObjectPropertyOwner:
			NEED(AudioObjectID);
			*(AudioObjectID*)out = ID_PLUGIN;
			return 0;
		case kAudioObjectPropertyName:
			NEED(CFStringRef);
			*(CFStringRef*)out = cfstr(mic ? MIC_NAME : SINK_NAME);
			return 0;
		case kAudioObjectPropertyManufacturer:
			NEED(CFStringRef);
			*(CFStringRef*)out = cfstr(MANUFACTURER);
			return 0;
		case kAudioDevicePropertyDeviceUID:
			NEED(CFStringRef);
			*(CFStringRef*)out = cfstr(mic ? MIC_UID : SINK_UID);
			return 0;
		case kAudioDevicePropertyModelUID:
			NEED(CFStringRef);
			*(CFStringRef*)out = cfstr(MODEL_UID);
			return 0;
		case kAudioObjectPropertyOwnedObjects:
		case kAudioDevicePropertyStreams: {
			AudioObjectID ids[1];
			UInt32 n = device_streams(id, a->mScope, ids);
			if (n && size >= sizeof(AudioObjectID)) {
				*(AudioObjectID*)out = ids[0];
				*out_size = sizeof(AudioObjectID);
			} else {
				*out_size = 0;
			}
			return 0;
		}
		case kAudioObjectPropertyControlList:
			*out_size = 0;
			return 0;
		case kAudioDevicePropertyTransportType:
			NEED(UInt32);
			*(UInt32*)out = kAudioDeviceTransportTypeVirtual;
			return 0;
		case kAudioDevicePropertyClockDomain:
			NEED(UInt32);
			*(UInt32*)out = 0x4b4e3101; // both devices: one clock
			return 0;
		case kAudioDevicePropertyDeviceIsAlive:
			NEED(UInt32);
			*(UInt32*)out = 1;
			return 0;
		case kAudioDevicePropertyDeviceIsRunning:
			NEED(UInt32);
			pthread_mutex_lock(&g_lock);
			*(UInt32*)out = g_io_running[dev_index(id)] > 0;
			pthread_mutex_unlock(&g_lock);
			return 0;
		case kAudioDevicePropertyDeviceCanBeDefaultDevice:
			NEED(UInt32);
			// the microphone can be the default input; the sink is only for the helper
			*(UInt32*)out = mic && a->mScope != kAudioObjectPropertyScopeOutput;
			return 0;
		case kAudioDevicePropertyDeviceCanBeDefaultSystemDevice:
			NEED(UInt32);
			*(UInt32*)out = 0;
			return 0;
		case kAudioDevicePropertyLatency:
			NEED(UInt32);
			*(UInt32*)out = LATENCY_FRAMES;
			return 0;
		case kAudioDevicePropertySafetyOffset:
			NEED(UInt32);
			*(UInt32*)out = SAFETY_FRAMES;
			return 0;
		case kAudioDevicePropertyIsHidden:
			NEED(UInt32);
			*(UInt32*)out = !mic;
			return 0;
		case kAudioDevicePropertyZeroTimeStampPeriod:
			NEED(UInt32);
			*(UInt32*)out = RING_FRAMES;
			return 0;
		case kAudioDevicePropertyClockIsStable:
			NEED(UInt32);
			*(UInt32*)out = 1;
			return 0;
		case kAudioDevicePropertyRelatedDevices:
			NEED(AudioObjectID);
			*(AudioObjectID*)out = id;
			return 0;
		case kAudioDevicePropertyNominalSampleRate:
			NEED(Float64);
			*(Float64*)out = RATE;
			return 0;
		case kAudioDevicePropertyAvailableNominalSampleRates: {
			AudioValueRange r;
			NEED(AudioValueRange);
			r.mMinimum = r.mMaximum = RATE;
			*(AudioValueRange*)out = r;
			return 0;
		}
		case kAudioDevicePropertyPreferredChannelsForStereo:
			if (size < 2 * sizeof(UInt32))
				return kAudioHardwareBadPropertySizeError;
			((UInt32*)out)[0] = 1;
			((UInt32*)out)[1] = 1;
			*out_size = 2 * sizeof(UInt32);
			return 0;
		}
		return kAudioHardwareUnknownPropertyError;
	}

	if (is_stream(id)) {
		int mic = id == ID_MIC_STREAM;
		switch (a->mSelector) {
		case kAudioObjectPropertyBaseClass:
			NEED(AudioClassID);
			*(AudioClassID*)out = kAudioObjectClassID;
			return 0;
		case kAudioObjectPropertyClass:
			NEED(AudioClassID);
			*(AudioClassID*)out = kAudioStreamClassID;
			return 0;
		case kAudioObjectPropertyOwner:
			NEED(AudioObjectID);
			*(AudioObjectID*)out = mic ? ID_MIC : ID_SINK;
			return 0;
		case kAudioObjectPropertyName:
			NEED(CFStringRef);
			*(CFStringRef*)out = cfstr(mic ? "Beam" : "Beamformer input");
			return 0;
		case kAudioObjectPropertyOwnedObjects:
			*out_size = 0;
			return 0;
		case kAudioStreamPropertyIsActive:
			NEED(UInt32);
			*(UInt32*)out = 1;
			return 0;
		case kAudioStreamPropertyDirection:
			NEED(UInt32);
			*(UInt32*)out = mic ? 1 : 0; // 1: input
			return 0;
		case kAudioStreamPropertyTerminalType:
			NEED(UInt32);
			*(UInt32*)out = mic ? kAudioStreamTerminalTypeMicrophone : kAudioStreamTerminalTypeLine;
			return 0;
		case kAudioStreamPropertyStartingChannel:
			NEED(UInt32);
			*(UInt32*)out = 1;
			return 0;
		case kAudioStreamPropertyLatency:
			NEED(UInt32);
			*(UInt32*)out = 0;
			return 0;
		case kAudioStreamPropertyVirtualFormat:
		case kAudioStreamPropertyPhysicalFormat:
			NEED(AudioStreamBasicDescription);
			*(AudioStreamBasicDescription*)out = format();
			return 0;
		case kAudioStreamPropertyAvailableVirtualFormats:
		case kAudioStreamPropertyAvailablePhysicalFormats: {
			AudioStreamRangedDescription r;
			NEED(AudioStreamRangedDescription);
			r.mFormat = format();
			r.mSampleRateRange.mMinimum = r.mSampleRateRange.mMaximum = RATE;
			*(AudioStreamRangedDescription*)out = r;
			return 0;
		}
		}
		return kAudioHardwareUnknownPropertyError;
	}
	return kAudioHardwareBadObjectError;
}

static OSStatus set_property_data(AudioServerPlugInDriverRef driver, AudioObjectID id, pid_t pid, const AudioObjectPropertyAddress* a,
                                  UInt32 qsize, const void* qdata, UInt32 size, const void* data)
{
	(void)pid; (void)qsize; (void)qdata;
	if (driver != g_driver || !a || !data)
		return kAudioHardwareBadObjectError;
	if (is_device(id) && a->mSelector == kAudioDevicePropertyNominalSampleRate) {
		if (size < sizeof(Float64))
			return kAudioHardwareBadPropertySizeError;
		return *(const Float64*)data == RATE ? kAudioHardwareNoError : kAudioDeviceUnsupportedFormatError;
	}
	if (is_stream(id) && (a->mSelector == kAudioStreamPropertyVirtualFormat || a->mSelector == kAudioStreamPropertyPhysicalFormat)) {
		const AudioStreamBasicDescription* f = data;
		AudioStreamBasicDescription ours = format();
		if (size < sizeof(AudioStreamBasicDescription))
			return kAudioHardwareBadPropertySizeError;
		return f->mSampleRate == ours.mSampleRate && f->mFormatID == ours.mFormatID && f->mChannelsPerFrame == 1 &&
		               f->mBitsPerChannel == 32 && (f->mFormatFlags & kAudioFormatFlagIsFloat)
		           ? kAudioHardwareNoError
		           : kAudioDeviceUnsupportedFormatError;
	}
	return has_property(driver, id, pid, a) ? kAudioHardwareUnsupportedOperationError : kAudioHardwareUnknownPropertyError;
}

/* ------------------------------------------------------------ IO */

static OSStatus start_io(AudioServerPlugInDriverRef driver, AudioObjectID id, UInt32 client)
{
	(void)client;
	if (driver != g_driver || !is_device(id))
		return kAudioHardwareBadObjectError;
	pthread_mutex_lock(&g_lock);
	if (g_io_running[0] == 0 && g_io_running[1] == 0) {
		// the shared clock starts now
		g_anchor_host_time = mach_absolute_time();
		g_timestamp_count = 0;
		atomic_store(&g_written_begin, 0.0);
		atomic_store(&g_written_end, 0.0);
		memset(g_ring, 0, sizeof(g_ring));
	}
	g_io_running[dev_index(id)]++;
	pthread_mutex_unlock(&g_lock);
	return kAudioHardwareNoError;
}

static OSStatus stop_io(AudioServerPlugInDriverRef driver, AudioObjectID id, UInt32 client)
{
	(void)client;
	if (driver != g_driver || !is_device(id))
		return kAudioHardwareBadObjectError;
	pthread_mutex_lock(&g_lock);
	if (g_io_running[dev_index(id)] > 0)
		g_io_running[dev_index(id)]--;
	if (id == ID_SINK && g_io_running[0] == 0) {
		// nothing more is written: the microphone gives silence
		atomic_store(&g_written_begin, 0.0);
		atomic_store(&g_written_end, 0.0);
	}
	pthread_mutex_unlock(&g_lock);
	return kAudioHardwareNoError;
}

static OSStatus get_zero_time_stamp(AudioServerPlugInDriverRef driver, AudioObjectID id, UInt32 client, Float64* sample_time,
                                    UInt64* host_time, UInt64* seed)
{
	UInt64 now, ticks_per_ring, next;
	(void)client;
	if (driver != g_driver || !is_device(id))
		return kAudioHardwareBadObjectError;
	pthread_mutex_lock(&g_lock);
	now = mach_absolute_time();
	ticks_per_ring = (UInt64)(g_host_ticks_per_frame * RING_FRAMES);
	next = g_anchor_host_time + (g_timestamp_count + 1) * ticks_per_ring;
	if (now >= next)
		g_timestamp_count++;
	*sample_time = (Float64)(g_timestamp_count * RING_FRAMES);
	*host_time = g_anchor_host_time + g_timestamp_count * ticks_per_ring;
	*seed = 1;
	pthread_mutex_unlock(&g_lock);
	return kAudioHardwareNoError;
}

static OSStatus will_do_io(AudioServerPlugInDriverRef driver, AudioObjectID id, UInt32 client, UInt32 op, Boolean* will,
                           Boolean* in_place)
{
	(void)client;
	if (driver != g_driver || !is_device(id) || !will || !in_place)
		return kAudioHardwareBadObjectError;
	*will = (id == ID_MIC && op == kAudioServerPlugInIOOperationReadInput) ||
	        (id == ID_SINK && op == kAudioServerPlugInIOOperationWriteMix);
	*in_place = true;
	return kAudioHardwareNoError;
}

static OSStatus begin_io(AudioServerPlugInDriverRef driver, AudioObjectID id, UInt32 client, UInt32 op, UInt32 frames,
                         const AudioServerPlugInIOCycleInfo* cycle)
{
	(void)client; (void)op; (void)frames; (void)cycle;
	return driver == g_driver && is_device(id) ? kAudioHardwareNoError : kAudioHardwareBadObjectError;
}

static OSStatus do_io(AudioServerPlugInDriverRef driver, AudioObjectID id, AudioObjectID stream, UInt32 client, UInt32 op,
                      UInt32 frames, const AudioServerPlugInIOCycleInfo* cycle, void* main, void* secondary)
{
	float* buf = main;
	UInt32 i;
	(void)client; (void)secondary;
	if (driver != g_driver || !is_device(id) || !cycle)
		return kAudioHardwareBadObjectError;
	if (!buf || frames > RING_FRAMES)
		return kAudioHardwareNoError;

	if (id == ID_SINK && stream == ID_SINK_STREAM && op == kAudioServerPlugInIOOperationWriteMix) {
		Float64 t = cycle->mOutputTime.mSampleTime;
		UInt64 start = (UInt64)t;
		for (i = 0; i < frames; i++)
			g_ring[(start + i) % RING_FRAMES] = buf[i];
		if (atomic_load(&g_written_end) == 0.0 || t > atomic_load(&g_written_end) + RING_FRAMES)
			atomic_store(&g_written_begin, t); // (re)started
		atomic_store(&g_written_end, t + frames);
	} else if (id == ID_MIC && stream == ID_MIC_STREAM && op == kAudioServerPlugInIOOperationReadInput) {
		Float64 t = cycle->mInputTime.mSampleTime;
		Float64 begin = atomic_load(&g_written_begin), end = atomic_load(&g_written_end);
		UInt64 start = (UInt64)t;
		for (i = 0; i < frames; i++) {
			Float64 s = t + i;
			// only what the sink wrote, and not older than the ring holds
			buf[i] = (end > 0.0 && s >= begin && s < end && s >= end - RING_FRAMES) ? g_ring[(start + i) % RING_FRAMES] : 0.0f;
		}
	}
	return kAudioHardwareNoError;
}

static OSStatus end_io(AudioServerPlugInDriverRef driver, AudioObjectID id, UInt32 client, UInt32 op, UInt32 frames,
                       const AudioServerPlugInIOCycleInfo* cycle)
{
	(void)client; (void)op; (void)frames; (void)cycle;
	return driver == g_driver && is_device(id) ? kAudioHardwareNoError : kAudioHardwareBadObjectError;
}
