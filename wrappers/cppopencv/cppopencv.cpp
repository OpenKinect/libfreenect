// Based on https://openkinect.org/wiki/C%2B%2BOpenCvExample
// Use Cpp and OpenCV 2+ API
// Display RGB + D (640x480) frames and save it when pressing 's'

#include "libfreenect.hpp"
#include <iostream>
#include <pthread.h>
#include <opencv2/opencv.hpp>

using namespace cv;
using namespace std;


class myMutex {
public:
    myMutex() {
        pthread_mutex_init( &m_mutex, NULL );
    }
    void lock() {
        pthread_mutex_lock( &m_mutex );
    }
    void unlock() {
        pthread_mutex_unlock( &m_mutex );
    }
private:
    pthread_mutex_t m_mutex;
};

class MyFreenectDevice : public Freenect::FreenectDevice {
public:
    MyFreenectDevice(freenect_context *_ctx, int _index) :
            Freenect::FreenectDevice(_ctx, _index),
            m_new_rgb_frame(false), m_new_depth_frame(false),
            depthMat(Size(640,480),CV_16UC1),
            rgbMat(Size(640,480), CV_8UC3, Scalar(0)) {
        setDepthFormat(FREENECT_DEPTH_REGISTERED);
    }

    // Do not call directly even in child
    void VideoCallback(void* _rgb, uint32_t timestamp) {
        std::cout << "RGB callback " << timestamp << std::endl;
        m_rgb_mutex.lock();
        uint8_t* rgb = static_cast<uint8_t*>(_rgb);
        rgbMat.data = rgb;
        m_new_rgb_frame = true;
        m_rgb_mutex.unlock();
    };

    // Do not call directly even in child
    void DepthCallback(void* _depth, uint32_t timestamp) {
        std::cout << "Depth callback " << timestamp << std::endl;
        m_depth_mutex.lock();
        uint16_t* depth = static_cast<uint16_t*>(_depth);
        depthMat.data = (uchar*) depth;
        m_new_depth_frame = true;
        m_depth_mutex.unlock();
    }

    bool getVideo(Mat& output) {
        m_rgb_mutex.lock();
        if(m_new_rgb_frame) {
            cv::cvtColor(rgbMat, output, COLOR_RGB2BGR);
            m_new_rgb_frame = false;
            m_rgb_mutex.unlock();
            return true;
        } else {
            m_rgb_mutex.unlock();
            return false;
        }
    }

    bool getDepth(Mat& output) {
        m_depth_mutex.lock();
        if(m_new_depth_frame) {
            depthMat.copyTo(output);
            m_new_depth_frame = false;
            m_depth_mutex.unlock();
            return true;
        } else {
            m_depth_mutex.unlock();
            return false;
        }
    }
private:
    Mat depthMat;
    Mat rgbMat;
    myMutex m_rgb_mutex;
    myMutex m_depth_mutex;
    bool m_new_rgb_frame;
    bool m_new_depth_frame;
};


int main(int argc, char **argv) {
    bool die(false);
    int i_snap(0),iter(0);

    Mat depthMat(Size(640,480),CV_16UC1);
    Mat depthMatNice (Size(640, 480), CV_8UC1);
    Mat rgbMat(Size(640,480),CV_8UC3,Scalar(0));

    Freenect::Freenect freenect;

    cout << "Number of devices found: " << freenect.deviceCount() << endl;

    MyFreenectDevice& device = freenect.createDevice<MyFreenectDevice>(0);

    namedWindow("rgb", WINDOW_AUTOSIZE);
    namedWindow("depth" ,WINDOW_AUTOSIZE);
    device.startVideo();
    device.startDepth();

    while (!die) {
        device.getVideo(rgbMat);
        device.getDepth(depthMat);
        cv::imshow("rgb", rgbMat);
        depthMat.convertTo(depthMatNice, CV_8UC1, 255.0 / 5000); //display only range [0m-5m] in grayscale
        cv::imshow("depth", depthMatNice);

        char k = waitKey(5);
        if( k == 27 ){ //ESC
            destroyWindow("rgb");
            destroyWindow("depth");
            break;
        }
        if( k == 's' ) {
            std::ostringstream file_rgb;
            file_rgb << "snapshot_" << i_snap << "_rgb.png";
            cv::imwrite(file_rgb.str(), rgbMat);
            cout << "file_rgb rgb saved: " << file_rgb.str() << endl;
            std::ostringstream file_d;
            file_d << "snapshot_" << i_snap << "_depth.png";
            cv::imwrite(file_d.str(), depthMat);
            cout << "file_rgb depth saved: " << file_d.str() << endl;
            i_snap++;
        }
        iter++;
    }

    device.stopVideo();
    device.stopDepth();
    return 0;
}
