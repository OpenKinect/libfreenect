#!/bin/sh

LIBUSB_INCLUDE=-I/usr/include/libusb-1.0
LIBUSB_LIBRARY=-lusb-1.0

LIBFREENET_INCLUDE=-I../../include/
LIBFREENET_LIBRARY=../../lib/libfreenect.a

# -- try to use installed as fallback
if [ -z ${LIBFREENET_INCLUDE} ]; then LIBFREENET_INCLUDE="/usr/include/libfreenect/"; fi
if [ -z ${LIBFREENET_LIBRARY} ]; then LIBFREENET_LIBRARY="/usr/lib/libfreenect.a"; fi


#-- if jdk home is not set try ubuntu default
if [ -z ${JDK_HOME} ]; then JDK_HOME="/usr/lib/jvm/java-6-openjdk"; fi

JNI_SRC_DIR=OpenKinectJNI
JAVA_SRC_DIR=OpenKinect/src

mkdir -p dist/javadoc

g++ -m64 -shared -fPIC -Wall -o dist/libOpenKinect.so ${JNI_SRC_DIR}/org_openkinect_Context.cpp OpenKinectJNI/org_openkinect_Device.cpp ${LIBUSB_INCLUDE} ${LIBUSB_LIBRARY} ${LIBFREENET_INCLUDE} ${LIBFREENET_LIBRARY} -I${JDK_HOME}/include/ -I${JDK_HOME}/include/linux/ -L${JDK_HOME}/lib/

mkdir -p build

javac -d build -sourcepath ${JAVA_SRC_DIR} ${JAVA_SRC_DIR}/org/openkinect/*.java
javadoc -d dist/javadoc/ -sourcepath ${JAVA_SRC_DIR} ${JAVA_SRC_DIR}/org/openkinect/*.java


jar cvf dist/OpenKinect.jar -C build .

rm -R build
