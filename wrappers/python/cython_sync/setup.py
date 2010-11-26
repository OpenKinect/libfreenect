from distutils.core import setup
from distutils.extension import Extension
from Cython.Distutils import build_ext

ext_modules = [Extension("freenect_sync", ["freenect_sync.pyx"],
                         libraries=['usb-1.0', 'freenect_sync'],
                         extra_compile_args=['-fPIC', '-I', '../../include/',
                                             '-I', '/usr/include/libusb-1.0/',
                                             '-I', '/usr/local/include/libusb-1.0',
                                             '-I', '/usr/local/include',
                                             '-I', '../../c_sync/'])]
setup(
  name = 'freenect_sync',
  cmdclass = {'build_ext': build_ext},
  ext_modules = ext_modules
)
