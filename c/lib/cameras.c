#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <usb.h>
#include "libfreenect.h"
#include "cameras.h"


//as per last post on - http://libusb.6.n5.nabble.com/isochronous-transfer-td3250228.html#a3250228

//always have 64 transfers going?
#define NUM_XFERS 64

//each transfer is made up of 32 Packets each 1920
#define PKTS_PER_XFER 32
#define PKT_SIZE 1920

#define XFER_SIZE (PKTS_PER_XFER*PKT_SIZE)

//ignore these for know
#define DEPTH_LEN XFER_SIZE
#define RGB_LEN XFER_SIZE

static void *rgb_bufs[NUM_XFERS];
static void *depth_bufs[NUM_XFERS];

static depthcb depth_cb;
static rgbcb rgb_cb;

struct frame_hdr {
uint8_t magic[2];
uint8_t pad;
uint8_t flag;
uint8_t unk1;
uint8_t seq;
uint8_t unk2;
uint8_t unk3;
uint32_t timestamp;
};

uint8_t depth_buf[2*422400];
uint16_t depth_frame[640*480];
int depth_pos = 0;
unsigned char rgb_seq_init = 0;
unsigned char rgb_seq = 0;

//why is rgb buf 2*307200 = 2*640*480 ??????  // [From Majority] : because it's 640*480 (Bayer) but we allocate something bigger coz it could overrun
uint8_t rgb_buf[2*307200];
uint8_t rgb_frame[640*480*4];

int rgb_pos = 0;


extern const struct caminit inits[];
extern const int num_inits;

static void depth_process(uint8_t *buf, size_t len)
{
	int i;
	struct frame_hdr *hdr = (void*)buf;
	uint8_t *data = buf + sizeof(*hdr);
	int datalen = len - sizeof(*hdr);
	int bitshift = 0;

	if (len == 0)
		return;


	//printf("%02x %x\n", hdr->flag, depth_pos);
	switch (hdr->flag) {
		case 0x71:
			depth_pos = 0;
		case 0x72:
		case 0x75:
			memcpy(&depth_buf[depth_pos], data, datalen);
			depth_pos += datalen;
		break;
	}

	if (hdr->flag != 0x75)
		return;

	printf("GOT DEPTH FRAME, %d bytes\n", depth_pos);

	//memcpy(depth_frame, depth_buf, 640*480*2);

	
	for (i=0; i<(640*480); i++) {
		int idx = (i*11)/8;
		uint32_t word = (depth_buf[idx]<<16) | (depth_buf[idx+1]<<8) | depth_buf[idx+2];
		depth_frame[i] = ((word >> (13-bitshift)) & 0x7ff);
		bitshift = (bitshift + 11) % 8;
	}
	

	if(depth_cb)
		depth_cb(depth_frame, 640, 480);
}

static void rgb_process(uint8_t *buf, size_t len)
{
		
	int x,y,i,k,j;
	unsigned char newint = 0;
	struct frame_hdr *hdr = (void*)buf;
	uint8_t *data = buf+ sizeof(*hdr);
	int datalen = 1920 - sizeof(*hdr);


	if (len < sizeof(struct frame_hdr))
		return;

	//printf("%02x %x\n", hdr->flag, depth_pos);
	switch (hdr->flag) {
		case 0x81:
			{
			rgb_pos = 0;
			//rgb_seq_init = hdr->seq;
			}
		case 0x82:
		case 0x85:
			{
			//rgb_pos = hdr->seq - rgb_seq_init;
			memcpy(&rgb_buf[rgb_pos], data, datalen);
			//memcpy(&rgb_buf[rgb_pos], data, datalen);
			rgb_pos += datalen;//1920-sizeof(frame_hdr);
			}
		break;
	}
	/*
		newint = rgb_seq + 1;
		if (newint != hdr->seq)
		{
			printf("seq lost! %02x %02x \n", newint, hdr->seq);
			rgb_seq = hdr->seq;
			//return 1760;
		}
		else
		{
			//printf("in seq! %02x\n", newint);
		};

		rgb_seq = hdr->seq;
		*/
	// safety check?
	// if( rgb_pos >= 640*480){
	// rgb_pos = 0;
	// }

		//printf("hdr flag is %02x\n", hdr->flag);
	if (hdr->flag != 0x85 )
		return;

	printf("GOT RGB FRAME, %d bytes\n", rgb_pos);
	/*
	
	for(k = 0,j=0; k < 640*480; k++){
	rgb_frame[j] = rgb_buf[k];
	rgb_frame[j+1] = rgb_buf[k];
	rgb_frame[j+2] = rgb_buf[k];
	j+= 3;

	}
	*/

	// horrible bayer to RGB conversion, but does the job for now
	for (y=0; y<480; y++) {
		for (x=0; x<640; x++) {
			i = y*640+x;
			if (x&1) {
				if (y&1) {
					rgb_frame[3*i+1] = rgb_buf[i];
					rgb_frame[3*i+4] = rgb_buf[i];
				} else {
					rgb_frame[3*i] = rgb_buf[i];
					rgb_frame[3*i+3] = rgb_buf[i];
					rgb_frame[3*(i-640)] = rgb_buf[i];
					rgb_frame[3*(i-640)+3] = rgb_buf[i];
				}
			} else {
				if (y&1) {
					rgb_frame[3*i+2] = rgb_buf[i];
					rgb_frame[3*i-1] = rgb_buf[i];
					rgb_frame[3*(i+640)+2] = rgb_buf[i];
					rgb_frame[3*(i+640)-1] = rgb_buf[i];
				} else {
					rgb_frame[3*i+1] = rgb_buf[i];
					rgb_frame[3*i-2] = rgb_buf[i];
				}
			}
		}
	}

	
	
	if(rgb_cb)
		rgb_cb(rgb_frame, 640, 480);

}

//NOTE - There is something off about this approach
//in libusb_ we know the actual length of the packet - with usb we know the length we wanted our packets to be, the transfer size and the num packets
//this doesn't allow for the logic u see in the commented out one below.
static void rgb_callback(char * buf, int pktLen)
{
	int i;

	for (i=0; i<PKTS_PER_XFER; i++) {
		rgb_process(buf, pktLen);
		buf += pktLen;
	}

}


//static void rgb_callback(struct libusb_transfer *xfer)
//{
// int i;
// if(xfer->status == LIBUSB_TRANSFER_COMPLETED) {
// uint8_t *buf = (void*)xfer->buffer;
// for (i=0; i<PKTS_PER_XFER; i++) {
// rgb_process(buf, xfer->iso_packet_desc[i].actual_length);
// buf += RGB_LEN;
// }
// libusb_submit_transfer(xfer);
// } else {
// printf("Xfer error: %d\n", xfer->status);
// }
//}

static void depth_callback(char * buf, int pktLen)
{
	 int i;
	 for (i=0; i<PKTS_PER_XFER; i++) {
		 //printf("DCB %p %d\n", buf, xfer->iso_packet_desc[i].actual_length);
		 depth_process(buf, pktLen);
		 buf += pktLen;
	 }

}



static usb_dev_handle* motorHandle = NULL;

static usb_dev_handle* cameraHandle = NULL;

enum LIBFREENECT_RETURN_CODE init_camera_device(){

	struct usb_bus *busses;
	struct usb_bus *bus;
    int c, i, a;

    usb_init();
    usb_find_busses();
    usb_find_devices();

    busses = usb_get_busses();

	if(NULL != cameraHandle) return FREENECT_DEVICE_ALREADY_OPEN;

	for (bus = busses; bus; bus = bus->next) {

		struct usb_device *dev;

		for (dev = bus->devices; dev; dev = dev->next) {

			if (dev->descriptor.idProduct == 0x02ae && dev->descriptor.idVendor == 0x045E ) {

				cameraHandle = usb_open(dev);

				if(NULL == cameraHandle) return FREENECT_ERROR_DEVICE_OPEN_FAILED;
				break;

			}
		}

		if(NULL == cameraHandle) break;

	}

	if(NULL == cameraHandle) return FREENECT_ERROR_DEVICE_NOT_FOUND;

	// TODO : do something with the return code
	if(usb_set_configuration(cameraHandle, 1) < 0) return FREENECT_ERROR_DEVICE_OPEN_FAILED;
	// TODO : do something with the return code
	if(usb_claim_interface(cameraHandle, 0) < 0) return FREENECT_ERROR_DEVICE_OPEN_FAILED;
	//if(usb_set_altinterface(cameraHandle, 0) < 0) return FREENECT_ERROR_DEVICE_OPEN_FAILED;

	return FREENECT_OK;

}

struct cam_hdr {
uint8_t magic[2];
uint16_t len;
uint16_t cmd;
uint16_t tag;
};

extern const struct caminit inits[];
extern const int num_inits;

void start_camera_device(){

uint8_t obuf[0x2000];
uint8_t ibuf[0x2000];
struct cam_hdr *chdr = (void*)obuf;
struct cam_hdr *rhdr = (void*)ibuf;
int ret;
    int i, j;
printf("INIT CAMERA\n");

    ret = usb_control_msg(cameraHandle, 0x80, 0x06, 0x3ee, 0, ibuf, 0x12, 160);
    printf("First xfer: %d\n", ret);

    chdr->magic[0] = 0x47;
    chdr->magic[1] = 0x4d;

    for (i=0; i<num_inits; i++) {
        const struct caminit *ip = &inits[i];
        chdr->cmd = ip->command;
        chdr->tag = ip->tag;
        chdr->len = ip->cmdlen / 2;
        memcpy(obuf+sizeof(*chdr), ip->cmddata, ip->cmdlen);

        ret = usb_control_msg(cameraHandle, 0x40, 0, 0, 0, obuf, ip->cmdlen + sizeof(*chdr), 160);

        printf("CTL CMD %04x %04x = %d\n", chdr->cmd, chdr->tag, ret);
        //do {
		ret = usb_control_msg(cameraHandle, 0xc0, 0, 0, 0, ibuf, 0x200, 160);
        //} while (ret == 0);

        printf("CTL RES = %d\n", ret);
        if (rhdr->magic[0] != 0x52 || rhdr->magic[1] != 0x42) {
            printf("Bad magic %02x %02x\n", rhdr->magic[0], rhdr->magic[1]);
            continue;
        }
        if (rhdr->cmd != chdr->cmd) {
            printf("Bad cmd %02x != %02x\n", rhdr->cmd, chdr->cmd);
            continue;
        }
        if (rhdr->tag != chdr->tag) {
            printf("Bad tag %04x != %04x\n", rhdr->tag, chdr->tag);
            continue;
        }
        if (rhdr->len != (ret-sizeof(*rhdr))/2) {
            printf("Bad len %04x != %04x\n", rhdr->len, (int)(ret-sizeof(*rhdr))/2);
            continue;
        }
        if (rhdr->len != (ip->replylen/2) || memcmp(ibuf+sizeof(*rhdr), ip->replydata, ip->replylen)) {
            printf("Expected: ");
            for (j=0; j<ip->replylen; j++) {
                printf("%02x ", ip->replydata[j]);
            }
            printf("\nGot: ");
            for (j=0; j<(rhdr->len*2); j++) {
                printf("%02x ", ibuf[j+sizeof(*rhdr)]);
            }
            printf("\n");
        }
    }

}

void * rgb_contexts[NUM_XFERS];
void * depth_contexts[NUM_XFERS];

static int count = 0;

//HERE WE START THE QUEUE

//from -- http://libusb.6.n5.nabble.com/isochronous-transfer-td3250228.html#a3250228
void setup_isochronous_async(usb_dev_handle *dev){

	int k;
	count = 0;

	for( k = 0; k < NUM_XFERS; k++ ){
		rgb_contexts[k] = NULL;
		depth_contexts[k] = NULL;

		rgb_bufs[k] = malloc(XFER_SIZE);
		depth_bufs[k] = malloc(XFER_SIZE);

		usb_isochronous_setup_async(cameraHandle, &rgb_contexts[k], 0x81, PKT_SIZE);
		usb_isochronous_setup_async(cameraHandle, &depth_contexts[k], 0x82, PKT_SIZE);
	}

	for( k = 0; k < NUM_XFERS; k++ ){
		int ret = usb_submit_async(rgb_contexts[k], (char*)rgb_bufs[k], XFER_SIZE);
		if( ret < 0 )
		{
			printf("error: %s\n", usb_strerror());
		}
		ret = usb_submit_async(depth_contexts[k], (char*)depth_bufs[k], XFER_SIZE);
		if( ret < 0 )
		{
			printf("error: %s\n", usb_strerror());
		}
	}
}

//HERE WE UPDATE IT FROM glView.c

void update_isochronous_async(){


    //not sure if this is the right order for keeping the queue full.
    // but the logic came from - http://libusb.6.n5.nabble.com/isochronous-transfer-td3250228.html#a3250228
    // really good read!!!!!

	int read, ret;

    printf("count is %i\n", count);

	read = usb_reap_async_nocancel(depth_contexts[count], 5000);
    printf("read %d bytes\n", read);
    if( read < 0 ){
        printf("error: %s\n", usb_strerror());
        usb_cancel_async(depth_contexts[count]);
    }else if(read > 0){
        //send them to our rgb_callback - note the cb is modified as we don't have the same amount of packet info as libusb
        printf("depth_callback!\n");
        depth_callback(depth_bufs[count], PKT_SIZE);
    }


	ZeroMemory(depth_bufs[count], XFER_SIZE);
    ret = usb_submit_async(depth_contexts[count], (char*)depth_bufs[count], XFER_SIZE);
    if( ret < 0 ){
        printf("error: %s\n", usb_strerror());
        usb_cancel_async(depth_contexts[count]);
    }

    //reap any queued bytes ( ie read them in )
    read = usb_reap_async_nocancel(rgb_contexts[count], 5000);
    printf("read %d bytes\n", read);
    if( read < 0 ){
        printf("error: %s\n", usb_strerror());
        usb_cancel_async(rgb_contexts[count]);
    }else if(read > 0){
        //send them to our rgb_callback - note the cb is modified as we don't have the same amount of packet info as libusb
        printf("rgb_callback!\n");
        rgb_callback(rgb_bufs[count], PKT_SIZE);
    }

    //once we are done - we submit a new request!
	ZeroMemory(rgb_bufs[count], XFER_SIZE);
    ret = usb_submit_async(rgb_contexts[count], (char*)rgb_bufs[count], XFER_SIZE);
    if( ret < 0 ){
        printf("error: %s\n", usb_strerror());
        usb_cancel_async(rgb_contexts[count]);
    }

	
	
	
    //increase our count - ie we process one request a loop
    count++;
    if( count >= NUM_XFERS ){
        int k;
		count = 0;
/*		
		for( k = 0; k < NUM_XFERS; k++ ){
			int ret;
			
		ret = usb_submit_async(rgb_contexts[k], (char*)rgb_bufs[k], XFER_SIZE);
		if( ret < 0 )
		{
			printf("error: %s\n", usb_strerror());
		}
		
		}
		*/
    }

}


void prep_iso_transfers(depthcb dcb, rgbcb rcb){

depth_cb = dcb;
rgb_cb = rcb;


    setup_isochronous_async(cameraHandle);
}


enum LIBFREENECT_RETURN_CODE init_motor_device()
{
struct usb_bus *busses;
struct usb_bus *bus;
int c, i, a;

usb_init();
usb_find_busses();
usb_find_devices();

busses = usb_get_busses();



if(NULL != motorHandle) return FREENECT_DEVICE_ALREADY_OPEN;

for (bus = busses; bus; bus = bus->next) {

struct usb_device *dev;

for (dev = bus->devices; dev; dev = dev->next) {

if (dev->descriptor.idProduct == 0x02B0 && dev->descriptor.idVendor == 0x045E) {

motorHandle = usb_open(dev);

if(NULL == motorHandle) return FREENECT_ERROR_DEVICE_OPEN_FAILED;
break;

}
}

if(NULL == motorHandle) break;

}

if(NULL == motorHandle) return FREENECT_ERROR_DEVICE_NOT_FOUND;

// TODO : do something with the return code
if(usb_set_configuration(motorHandle, 1) < 0) return FREENECT_ERROR_DEVICE_OPEN_FAILED;
// TODO : do something with the return code
if(usb_claim_interface(motorHandle, 0) < 0) return FREENECT_ERROR_DEVICE_OPEN_FAILED;

return FREENECT_OK;

}

enum LIBFREENECT_RETURN_CODE close_motor_device()
{
if(NULL == motorHandle) return FREENECT_DEVICE_NOT_OPEN;

if(usb_release_interface(motorHandle, 0) < 0) return FREENECT_ERROR_DEVICE_CLOSE_FAILED;

if(usb_close(motorHandle) < 0) return FREENECT_ERROR_DEVICE_CLOSE_FAILED;

motorHandle = NULL;

return FREENECT_OK;
}

enum LIBFREENECT_RETURN_CODE set_led(enum KinectLEDStatus status)
{
uint8_t bytes = 0;

if(NULL == motorHandle) return FREENECT_DEVICE_NOT_OPEN;

if(usb_control_msg(motorHandle, 0x40, 0x06, (uint16_t)status, 0, &bytes, 0, 0) < 0)
return FREENECT_ERROR_TRANSFER;

return FREENECT_OK;
}

enum LIBFREENECT_RETURN_CODE set_motor_tilt(uint8_t tiltValue)
{
uint8_t bytes = 0;
uint16_t mappedValue = 0;

if(NULL == motorHandle) return FREENECT_DEVICE_NOT_OPEN;

mappedValue = (uint8_t)(0xffd0 + tiltValue / 5);

if(usb_control_msg(motorHandle, 0x40, 0x31, mappedValue, 0, &bytes, 0, 0) < 0)
return FREENECT_ERROR_TRANSFER;

return FREENECT_OK;
}

