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

import com.sun.jna.Structure;


/**
 * User: Erwan Daubert - erwan.daubert@gmail.com
 * Date: 12/08/11
 * Time: 13:46
 */
public class DepthFrameMode extends Structure {
	public int reserved;
	public int resolution;
//	public DepthFormat format;
	public int bytes;
	public short width;
	public short height;
	public short dataBitsPerPixel;
	public short paddingBitsPerPixel;
	public short framerate;
	public short valid;

	public DepthFrameMode () {
	}

	public DepthFrameMode (int reserved, int resolution/*, DepthFormat format*/, int bytes, short width,
			short height,
			short dataBitsPerPixel, short paddingBitsPerPixel, short framerate, short valid) {
		this.reserved = reserved;
		this.resolution = resolution;
//		this.format = format;
		this.bytes = bytes;
		this.width = width;
		this.height = height;
		this.dataBitsPerPixel = dataBitsPerPixel;
		this.paddingBitsPerPixel = paddingBitsPerPixel;
		this.framerate = framerate;
		this.valid = valid;
	}


	public static class DepthFrameModeByReference extends DepthFrameMode implements Structure.ByReference {

	}
	public static class DepthFrameModeByValue extends DepthFrameMode implements Structure.ByValue {

	}
}

/*typedef struct {
	uint32_t reserved;              *//**< unique ID used internally.  The meaning of values may change without notice.  Don't touch or depend on the contents of this field.  We mean it. *//*
	freenect_resolution resolution; *//**< Resolution this freenect_frame_mode describes, should you want to find it again with freenect_find_*_frame_mode(). *//*
	union {
		int32_t dummy;
		freenect_video_format video_format;
		freenect_depth_format depth_format;
	};                              *//**< The video or depth format that this freenect_frame_mode describes.  The caller should know which of video_format or depth_format to use, since they called freenect_get_*_frame_mode() *//*
	int32_t bytes;                  *//**< Total buffer size in bytes to hold a single frame of data.  Should be equivalent to width * height * (data_bits_per_pixel+padding_bits_per_pixel) / 8 *//*
	int16_t width;                  *//**< Width of the frame, in pixels *//*
	int16_t height;                 *//**< Height of the frame, in pixels *//*
	int8_t data_bits_per_pixel;     *//**< Number of bits of information needed for each pixel *//*
	int8_t padding_bits_per_pixel;  *//**< Number of bits of padding for alignment used for each pixel *//*
	int8_t framerate;               *//**< Approximate expected frame rate, in Hz *//*
	int8_t is_valid;                *//**< If 0, this freenect_frame_mode is invalid and does not describe a supported mode.  Otherwise, the frame_mode is valid. *//*
} freenect_frame_mode;*/
