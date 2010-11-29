#!/bin/sh

FREENET_INCLUDE=../../include/libfreenect
FREENET_LIBRARY=../../lib/libfreenect.a


#-- if jdk home is not set try ubuntu default
if [ -z ${JDK_HOME} ]; then JDK_HOME="/usr/lib/jvm/java-6-openjdk"; fi

JNI_SRC_DIR=OpenKinectJNI
JAVA_SRC_DIR=OpenKinect/src

mkdir -p dist

g++ -shared -fPIC -Wall -o dist/libOpenKinect.so ${JNI_SRC_DIR}/org_openkinect_Context.cpp OpenKinectJNI/org_openkinect_Device.cpp  -I/usr/include/libusb-1.0/ -lusb -I${FREENET_INCLUDE} -L${FREENET_LIBRARY}  -I${JDK_HOME}/include/ -I${JDK_HOME}/include/linux/ -L${JDK_HOME}/lib/

mkdir -p build

javac -d build -sourcepath ${JAVA_SRC_DIR} ${JAVA_SRC_DIR}/org/openkinect/*.java

jar cvf dist/OpenKinect.jar -C build .

rm -R build
