#include <sys/types.h>
#include <libfreenect.h>

struct proxynect_device {
    int index;
    freenect_loglevel loglevel;

    uint32_t timestamp;

    struct {
        int bufsel;
        uint32_t timestamp;
        freenect_frame_mode frame_mode[2];
        uint8_t data[2][1280*1024*3];
    } video;

    struct {
        int bufsel;
        uint32_t timestamp;
        freenect_frame_mode frame_mode[2];
        uint8_t data[2][640*480*2];
    } depth;

    freenect_raw_tilt_state raw_state;

    struct {
        double tilt_degs;
        int tilt_degs_changed;

        freenect_led_options led;
        int led_changed;

        freenect_frame_mode video_mode;
        int video_mode_changed;

        freenect_frame_mode depth_mode;
        int depth_mode_changed;
    } settings;
};

struct proxynect_device *create_device(int index, int force);
struct proxynect_device *open_device(int index);
void close_device(struct proxynect_device *device);
void destroy_device(struct proxynect_device *device);
