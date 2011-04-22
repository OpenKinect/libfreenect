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

#include <stdlib.h>
#include "freenect_internal.h"

// :cmos_register_operation
// ?Provide simple mechanism for operations regarding the CMOS registers.
typedef struct
{
	uint16_t address;
	uint16_t value;
	uint16_t operation;
} cmos_register_operation;

// :do_cmos_register_operations()
// ?Performs the requested operations on the CMOS registers.
static int do_cmos_register_operations(freenect_device* device, uint16_t count, cmos_register_operation* targets)
{
	// result contains the send_cmd() return value
	int result = 0;

	// calculate the size required for the command and reply buffers used by send_cmd()
	//// each buffer needs the address/value pair, but also the count at the beginning (count * 2 + 1)
	size_t buffer_size = (count * 2 + 1) * sizeof(uint16_t);

	// create the buffers
	uint16_t* command = (uint16_t*)malloc(buffer_size);
	uint16_t* reply = (uint16_t*)malloc(buffer_size);

	// ensure buffers are valid
	if (command != NULL && reply != NULL)
	{
		// temp address value for enforcing selected operations
		uint16_t address = 0;

		// double for-loops inside here caused an error, so just define 'i' outside and it's good
		int i = 0;

		// set up the buffers
		*(command) = count;
		*(reply) = count;

		// set up pre-send_cmd() data
		for (i = 0; i < count; i++)
		{
			// enforce read/write operations
			address = (targets + i)->address;
			if ((targets + i)->operation == 0)
			{
				address &= 0x7fff;
			}
			if ((targets + i)->operation == 1)
			{
				address |= 0x8000;
			}

			// copy the address value to the buffers
			*(command + 1 + 2 * i) = address;
			*(reply + 1 + 2 * i) = address;

			// if writing to a register, store the value in the command buffer
			if ((targets + i)->operation == 1)
			{
				*(command + 1 + 2 * i + 1) = (targets + i)->value;
			}
		}
		// good to go (0x0095 is cmos camera's control_hdr.cmd value) --> should it be made a constant?
		result = send_cmd(device, 0x95, command, buffer_size, reply, buffer_size);

		// now for post-send_cmd() -- the read operations
		for (i = 0; i < count; i++)
		{
			if ((targets + i)->operation == 0)
			{
				(targets + i)->value = *(reply + 1 + 2 * i + 1);
			}
		}

		// clean up
		i = 0;
		address = 0;
	}

	// clean up the buffers
	if (command != NULL)
	{
		free(command);
		command = NULL;
	}
	if (reply != NULL)
	{
		free(reply);
		reply = NULL;
	}

	// don't care about the buffer size
	buffer_size = 0;

	// return the value (0) if unable to create buffers; otherwise, return what value send_cmd() returned
	return result;
}

// :read_result
// ?Storage for the read operations value and the operation's returned result.
typedef struct
{
	int result;
	uint16_t value;
} read_result;

// :read_cmos_register()
// ?Reads a value from a single CMOS register.
static read_result read_cmos_register(freenect_device* device, uint16_t address, uint8_t loop_on_zeros)
{
	// in early testing (namely, cmos_dump'ing), it was noticed that some of the registers
	//  did not return expected values; ie: 0x0000 returned 0x0000 instead of 0x148c.
	// it was observed that at the rollovers (starting at register 0x0400) the version was
	//  returned as expected.
	// so, in order to affect the expected result (non-zero value), this function
	//  will loop through (of course, by 0x400 increments) the registers to get a non-zero
	//  value, until the register address exceeds 0x7fff which is the cut-off register
	//  address for read-only access.
	// if the address' value exceeds 7fff (becomes 0x8000) then the function will terminate
	//  and the return value will be zero.

	read_result rr;
	cmos_register_operation cro;

	if (loop_on_zeros)
	{
		int i = 0;
		for (i = 0; i + address < 0x7ffff && rr.value == 0x0000; i+=0x400)
		{
			cro.address = i + address;
			cro.operation = 0;

			rr.result = do_cmos_register_operations(device, 1, &cro);
			rr.value = cro.value;
		}
	}
	else
	{
		cro.address = address;
		cro.operation = 0;

		rr.result = do_cmos_register_operations(device, 1, &cro);
		rr.value = cro.value;
	}

	return rr;
}

// :write_cmos_register()
// ?Writes a value to a single CMOS register.
static int write_cmos_register(freenect_device* device, uint16_t address, uint16_t value)
{
	cmos_register_operation cro;

	cro.address = address;
	cro.value = value;
	cro.operation = 1;

	return do_cmos_register_operations(device, 1, &cro);
}

/*
// :freenect_dump_cmos_registers()
// ?Read all the registers [0x0000, 0x400).
int freenect_dump_cmos_registers(freenect_device* device, uint8_t show_zeros)
{
	// results -- probably won't need later on; change from static_int -> static_void
	int result = 0;

	// debug-output context
	freenect_context* ctx = device->parent;

	// loop through the registers
	int i = 0;
	int j = 0;

	// temp space
	cmos_register_operation* regptr = (cmos_register_operation*)malloc(10 * sizeof(cmos_register_operation));
	//cmos_register_operation regs[10];

	uint16_t value;

	if (!regptr)
	{
		FN_DEBUG("dcro() no space!!");
		ctx = NULL;
		return 0;
	}

	for (i = 0; i < 0x400; i+=10)
	{

		for (j = 0; j < 10 || i + j < 0x400; j++)
		{
			(regptr + j)->address = i + j;
			(regptr + j)->value = 0;
			//regs[j].address = i + j;
			//regs[j].value = 0;
		}

		result = do_cmos_register_operations(device, 10, regptr);
		FN_DEBUG("dcro() result: %d", result);
		//result = do_cmos_register_operations(device, 10, &regs[0]);

		// loop through replies
		if (show_zeros == 1)
		{
			for (j = 0; j < 10; j++)
			{
				value = (regptr + j)->value;
				FN_DEBUG("[0x%04x] 0x%04x (%d)\n", i + j, value, value);
				//FN_DEBUG("[0x%04x] 0x%04x\n", i + j, regs[j].value);
			}
		}
		else
		{
			for (j = 0; j < 10 || i + j < 0x400; j++)
			{
				value = (regptr + j)->value;
				//if (regs[j].value != 0)
				if (value != 0)
				{
					FN_DEBUG("[0x%04x] 0x%04x (%d)", i + j, value, value);
					//FN_DEBUG("[0x%04x] 0x%04x\n", i + j, regs[j].value);
				}
			}
		}

	//	free(regptr);
		regptr = NULL;
	}

	// clean up
	free(regptr);
	ctx = NULL;

	return result;
}
*/

// CMOS Register: Chip Version
const uint16_t CR_CHIP_VERSION = 0x0000;

// freenect_get_cmos_chip_version()
// Gets the CMOS chip version.
uint16_t freenect_get_cmos_chip_version(freenect_device* device)
{
	return read_cmos_register(device, CR_CHIP_VERSION, 1).value;
}


// CMOS Sensor Window Dimension Registers
const uint16_t CR_ROW_START = 0x0001;      //Starting Y coordinate
const uint16_t CR_COLUMN_START = 0x0002;   //Starting X coordinate
const uint16_t CR_ROW_COUNT = 0x0003;      //Height (in rows)
const uint16_t CR_COLUMN_COUNT = 0x0004;   //Width (in columns)

// CMOS Register: CR_APERTURE_CORRECTION
const uint16_t CR_APERTURE_CORRECTION = 0x0105;

// CMOS Register R37:1 - Color Saturation Control (Read/Write)
const uint16_t CR_COLOR_SATURATION = 0x0125;

// :freenect_get_cmos_window_attribute()
// ?Gets an attribute of the CMOS window.
int freenect_get_cmos_window_attribute(freenect_device* device, freenect_window_attribute attribute)
{
	int result = -1;

	switch(attribute)
	{
		case WINDOW_ATTRIBUTE_Y:
			result = (int)read_cmos_register(device, CR_ROW_START, 1).value;
			break;

		case WINDOW_ATTRIBUTE_X:
			result = (int)read_cmos_register(device, CR_COLUMN_START, 1).value;
			break;

		case WINDOW_ATTRIBUTE_WIDTH:
			result = (int)read_cmos_register(device, CR_COLUMN_COUNT, 1).value;
			break;

		case WINDOW_ATTRIBUTE_HEIGHT:
			result = (int)read_cmos_register(device, CR_ROW_COUNT, 1).value;
			break;

		default:
			break;
	}

	return result;
}

// :freenect_set_cmos_window_attribute()
// ?Sets an attribute of the CMOS window.
void freenect_set_cmos_window_attribute(freenect_device* device, freenect_window_attribute attribute, uint16_t value)
{
	switch(attribute)
	{
		case WINDOW_ATTRIBUTE_Y:
			write_cmos_register(device, CR_ROW_START, value);
			break;

		case WINDOW_ATTRIBUTE_X:
			write_cmos_register(device, CR_COLUMN_START, value);
			break;

		case WINDOW_ATTRIBUTE_WIDTH:
			write_cmos_register(device, CR_COLUMN_COUNT, value);
			break;

		case WINDOW_ATTRIBUTE_HEIGHT:
			write_cmos_register(device, CR_ROW_COUNT, value);
			break;

		default:
			break;
	}
}

// :freenect_get_cmos_aperture_correction_sharpening_factor()
// ?Gets the aperture correction sharpening factor.
freenect_sharpening_factor freenect_get_cmos_aperture_correction_sharpening_factor(freenect_device* device)
{
	return (freenect_sharpening_factor)(read_cmos_register(device, CR_APERTURE_CORRECTION, 1).value & 0x7);
}

// :freenect_set_cmos_aperture_correction_sharpening_factor()
// ?Sets the aperture correciton sharpening factor.
void freenect_set_cmos_aperture_correction_sharpening_factor(freenect_device* device, freenect_sharpening_factor factor)
{
	write_cmos_register(device, CR_APERTURE_CORRECTION, factor);
}

void freenect_set_cmos_aperture_correction(freenect_device* device, freenect_aperture_control mode)
{
	switch (mode)
	{
		case APERTURE_MANUAL:
			write_cmos_register(device, CR_APERTURE_CORRECTION, 0x3);
			break;

		case APERTURE_AUTOMATIC:
			write_cmos_register(device, CR_APERTURE_CORRECTION, 0xB);
			break;

		default:
			break;
	}
}

// :freenect_get_cmos_color_saturation_attenuation()
// ?Gets the overall color saturation attenuation.
freenect_color_saturation_attenuation freenect_get_cmos_color_saturation_attenuation(freenect_device* device)
{
	// 5:3 mask: 111000 0x38
	return (freenect_color_saturation_attenuation)(read_cmos_register(device, CR_COLOR_SATURATION, 1).value & 0x38);
}

// :freenect_set_cmos_color_saturation_attenuation()
// ?Sets the overall color saturation attenuation.
void freenect_set_cmos_color_saturation_attenuation(freenect_device* device, freenect_color_saturation_attenuation attenuation)
{
	write_cmos_register(device, CR_COLOR_SATURATION, attenuation);
}

// :freenect_get_cmos_luminance_attenuation()
// ?Gets the high luminance level that attenuation starts at.
freenect_attenuation_luminance freenect_get_cmos_attenuation_luminance(freenect_device* device)
{
	return (freenect_attenuation_luminance)(read_cmos_register(device, CR_COLOR_SATURATION, 1).value & 0x7);
}

// :freenect_set_cmos_luminance_attenuation()
// ?Sets the high luminance level that attenuation starts at.
void freenect_set_cmos_attenuation_luminance(freenect_device* device, freenect_attenuation_luminance luminance)
{
	write_cmos_register(device, CR_COLOR_SATURATION, (freenect_get_cmos_color_saturation_attenuation(device) << 3) + luminance);
}
