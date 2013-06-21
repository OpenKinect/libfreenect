// This file is a public domain test case for the C++ wrapper libfreenect.hpp

#include "libfreenect.hpp"


static std::string exe_name;

void depthSlot(Freenect::DepthStream::Frame frame) {
	std::cout << exe_name <<  ": received a depth frame at " << frame.timestamp << " - " << frame.mode.width << "x" << frame.mode.height << std::endl;
}

void videoSlot(Freenect::VideoStream::Frame frame) {
	std::cout << exe_name << ": received a video frame at " << frame.timestamp << " - " << frame.mode.width << "x" << frame.mode.height << std::endl;
}

void motorTest(std::shared_ptr<Freenect::Device> device, unsigned int sleep_seconds = 3) {
	std::cout << exe_name << ": setting LED to red" << std::endl;
	device->motor->setLED(LED_RED);
	
	std::default_random_engine rng(std::chrono::system_clock::now().time_since_epoch().count());
	std::uniform_int_distribution<int> angle_range(device->motor->MIN_ANGLE, device->motor->MAX_ANGLE);
	int angle = angle_range(rng);
	std::cout << exe_name << ": setting motor angle to " << angle << std::endl;
	device->motor->setAngle(angle);
	
	std::this_thread::sleep_for(std::chrono::seconds(sleep_seconds));
}

void streamTest(std::shared_ptr<Freenect::Device> device, unsigned int sleep_seconds = 4) {
	boost::signals2::scoped_connection depth_stream(device->depth->connect(&depthSlot));
	boost::signals2::scoped_connection video_stream(device->video->connect(&videoSlot));
	
	std::this_thread::sleep_for(std::chrono::seconds(sleep_seconds));
}

int main(int argc, char* argv[]) {
	exe_name = argv[0];
	
	unsigned int i = 0;
	for (auto device : Freenect::devices()) {
		std::cout << std::endl << exe_name << ": testing device " << i++ << std::endl;
		streamTest(device);
		if (device->motor)
			motorTest(device);
	}
	
	if (i == 0) {
		std::cerr << "No devices found" << std::endl;
		return 1;
	}
	
	return 0;
}
