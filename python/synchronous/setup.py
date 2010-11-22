#!/usr/bin/env python

from distutils.core import setup, Extension

pykinect_c = Extension("pykinect_c", ["pykinect/pykinect.c"],
                       include_dirs=['/usr/include/libusb-1.0'],
                       libraries=['pthread', 'freenect', 'usb'])

setup(name="pykinect",
      version="0.1",
      description="A synchronous API to the OpenKinect libfreenect library",
      author="Kelvie Wong",
      author_email="kelvie@ieee.org",
      url="http://www.openkinect.org",
      license="GPL/Apache",
      packages=["pykinect"],
      ext_modules=[pykinect_c])
