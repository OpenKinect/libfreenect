require 'formula'

class Libfreenect <Formula
  url 'https://github.com/OpenKinect/libfreenect/tarball/bae0ec6a347936ffcb39a285d0f7bbe4780326aa'
  version 'bae0ec6a347936ffcb39'
  homepage 'http://openkinect.org'
  md5 '3097944042d644e4d7276874271d3152'
  
  depends_on 'libusb-freenect'
  depends_on 'cmake'

  def install
    cd "c"
    mkdir "build"
    cd "build"
    system "cmake .. #{std_cmake_parameters}"
    system "make install"
  end
end
