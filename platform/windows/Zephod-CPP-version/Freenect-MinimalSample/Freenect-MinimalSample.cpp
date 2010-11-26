#include "../libfreenect.h"
#include <windows.h>
#include <conio.h>
#include <stdio.h>


void DepthCB(freenect_device *dev, freenect_depth *depth, uint32_t timestamp)
{
	printf("depth..\n");
};

void RGBCB(freenect_device *dev, freenect_pixel *rgb, uint32_t timestamp)
{
	printf("color..\n");
};

void FreenectLog(freenect_context *dev, freenect_loglevel level, const char *msg, ...)
{
	// nothing logged yet...
};

int main(int argc, char **argv)
{
	freenect_context *Context = NULL;
	freenect_device *Device = NULL;

	freenect_init(&Context, NULL);
	if (Context == NULL)
	{
		printf("freenect_init failed!\n");
		return 0;
	};

	freenect_set_log_level(Context, FREENECT_LOG_FLOOD);
	freenect_set_log_callback(Context, (freenect_log_cb) FreenectLog);
	
	int DeviceCount = freenect_num_devices(Context);
	
	printf("freenect_num_devices: %d\n", DeviceCount);

	if (DeviceCount > 0)
	{
		freenect_open_device(Context, &Device, 0);

		freenect_set_user(Device, (void*)0x12345678);
		if (freenect_get_user(Device) != (void*)0x12345678) 
		{
			printf("freenect_get_user failed!\n");
		};
		freenect_set_depth_callback(Device, DepthCB);
		freenect_set_rgb_callback(Device, RGBCB);
		freenect_start_depth(Device);
		freenect_start_rgb(Device);
	
		while (!_kbhit())
		{
			freenect_process_events(Context);
			Sleep(10);
		};
		_getch();

		freenect_close_device(Device);
	};

	freenect_shutdown(Context);
	return 0;
};