cmake_minimum_required(VERSION 3.8.2)

set(THREADS_USE_PTHREADS_WIN32 true)
find_package(Threads)
find_package( OpenCV REQUIRED )

include_directories(. ${THREADS_PTHREADS_INCLUDE_DIR})

add_executable( freenect-cppopencv cppopencv.cpp)
target_compile_features(freenect-cppopencv PUBLIC cxx_std_11)
target_link_libraries( freenect-cppopencv freenect ${CMAKE_THREAD_LIBS_INIT} ${OpenCV_LIBS} )