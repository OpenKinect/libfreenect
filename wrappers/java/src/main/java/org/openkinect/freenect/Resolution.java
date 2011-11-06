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

/**
 * User: Erwan Daubert - erwan.daubert@gmail.com
 * Date: 12/08/11
 * Time: 13:48
 */
public enum Resolution {
	FREENECT_RESOLUTION_LOW    (0),
	FREENECT_RESOLUTION_MEDIUM (1),
	FREENECT_RESOLUTION_HIGH   (2),
	FREENECT_RESOLUTION_DUMMY  (2147483647);

	private int value;

	Resolution (int value) {
		this.value = value;
	}

	public int getValue () {
		return value;
	}
}

/*typedef enum {
	FREENECT_RESOLUTION_LOW    = 0, *//**< QVGA - 320x240 *//*
	FREENECT_RESOLUTION_MEDIUM = 1, *//**< VGA  - 640x480 *//*
	FREENECT_RESOLUTION_HIGH   = 2, *//**< SXGA - 1280x1024 *//*
	FREENECT_RESOLUTION_DUMMY  = 2147483647, *//**< Dummy value to force enum to be 32 bits wide *//*
} freenect_resolution;*/
