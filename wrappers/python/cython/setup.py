from distutils.core import setup
from distutils.extension import Extension
from Cython.Distutils import build_ext

ext_modules = [Extension("freenect", ["freenect.pyx"],
                         extra_objects=["../../c/build/lib/libfreenect.a"],
                         libraries=['usb-1.0'],
                         extra_compile_args=['-fPIC', '-I', '../../c/include/',
                                             '-I', '/usr/include/libusb-1.0/',
                                             '-I', '/usr/local/include/libusb-1.0',
                                             '-I', '/usr/local/include'])]
setup(
  name = 'freenect',
  cmdclass = {'build_ext': build_ext},
  ext_modules = ext_modules
)
