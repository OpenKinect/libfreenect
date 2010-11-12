#ifndef LIBFREENECT_H
#define LIBFREENECT_H

#include <stdint.h>

typedef void (*depthcb)(uint16_t *buf, int width, int height);
typedef void (*rgbcb)(uint8_t *buf, int width, int height);

enum LIBFREENECT_RETURN_CODE { FREENECT_ERROR_DEVICE_NOT_FOUND, FREENECT_ERROR_DEVICE_OPEN_FAILED, FREENECT_OK, 
	FREENECT_DEVICE_ALREADY_OPEN, FREENECT_DEVICE_NOT_OPEN, FREENECT_ERROR_DEVICE_CLOSE_FAILED, FREENECT_ERROR_TRANSFER};

enum KinectLEDStatus
    {
        Off = 0x0,
        Green = 0x1,
        Red = 0x2,
        Yellow = 0x3,
        BlinkingYellow = 0x4,
        BlinkingGreen = 0x5,
        AlternateRedYellow = 0x6,
        AlternateRedGreen = 0x7
    };


enum LIBFREENECT_RETURN_CODE InitMotorDevice();
enum LIBFREENECT_RETURN_CODE CloseMotorDevice();

enum LIBFREENECT_RETURN_CODE SetLED(enum KinectLEDStatus status);
enum LIBFREENECT_RETURN_CODE SetMotorTilt(byte tiltValue);

#endif