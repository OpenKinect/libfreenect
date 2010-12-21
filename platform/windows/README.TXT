======================================================================================

SUMMARY

1) libfreenect: incompatibilities with Visual C++
   This section is here merely for historical reasons. All of the issues documented
   there were already fixed in the libfreenect repository and CMake should produce
   a project ready to build libfreenect in Windows (see http://www.libusb.org/).
   Consider browsing this section if experiencing compilation issues in different
   platforms or compilers.

2) libusbemu: Tips, Hints and Best Practices
   The current port of libfreenect for Windows uses a libusb-1.0 emulation layer. This
   is necessary because libusb-1.0 is not yet available for Windows, even though an
   announcement of its release was made back in August 2010. Such interface emulation
   allows Windows development to keep in sync with the official development branch of
   libfreenect, without the need of dedicated drivers/implementations. The current
   status of libusbemu is quite reliable under normal usage circumstances, but by no
   means stable: caution is advised. This section provides some guidelines to avoid
   potential pitfalls and keep the application running safely.

3) Overall performance of libfreenect in Windows
   The current Windows port has some performance overheads over other platforms and
   dedicated implementations. This section contains a benchmark scenario and a also
   a discussion on the results. The conclusion is that such overhead is negligible
   and should not prevent anyone from using the official port.

======================================================================================

1) libfreenect source code: incompatibilities with Visual C++

----------------------------------------------------------------------------------
Language issues: The Microsoft C compiler does not implement all the C99 standard.
----------------------------------------------------------------------------------

An attempt to compile the current libfreenect with Visual C++ will trigger a lot
of errors. A simple workaround is to tell Visual Studio to force compilation all .c
files within the project using the C++ compiler:
  Project >> Properties >> C/C++ >> Advanced >> Compile As: Compile as C++ Code (/TP)

This will get rid of most errors, except those regarding implicit pointer casts.
Here are a few examples of such implicit pointer casts from within libfreenect:
  tilt.c      dev->raw_state.tilt_status = buf[9];
  core.c      *ctx = malloc(sizeof(freenect_context));
  cameras.c   strm->raw_buf = strm->proc_buf;
It seems that it is not possible to force Visual C++ to perform such implicit casts
(if anyone knows how, please share! :-)

Such implicit casts then have to be made explicit:
              dev->raw_state.tilt_status = (freenect_tilt_status_code)buf[9];
              *ctx = (freenect_context*)malloc(sizeof(freenect_context));
              strm->raw_buf = (uint8_t*)strm->proc_buf;
Fortunately, they are not many, and it can be done in a couple of minutes.
This should impose a minimal burden to the Win32 repository maintainers.

Another problem is that Visual C++ does not offer the <unistd.h> and <stdbool.h>
headers. However, they are pretty simple to be emulated; the <stdbool.h> will
actually be a dummy header because C++ already defines the "bool" type semantics,
while the <unistd.h> has to simply #include <stdint.h> and define the "ssize_t"
type. The implementation of these headers are located at:
  "libfreenect\platform\windows"

The "freenect_internal.h" makes use of GCC's "__attribute__" keyword, and there is no
such a thing in Visual C++. Fortunately, this header is not exposed for the library
user and is just required during the library build. There are a few simple solutions
for this issue: a) remove this keyword or b) define a dummy macro for it.

Another issue is that since all .c files were forced to be compiled as C++ code,
the "libfreenect.h" header no longer requires the "#ifdef __cpluscplus extern C"
idiom. Commenting out this guard will do the trick, but better checkings are possible.

-----------------------------------------------------------------------------------
Library issues: libfreenect uses libusb-1.0 which is not yet available for Windows.
-----------------------------------------------------------------------------------

The latest version of libusb-win32 is based on the old libusb-0.1 API. Fortunately,
the libusb-1.0 API can be emulated, at some extent, via what is provided from the
libusb-0.1 API.

The normal execution flow of libfreenect is something like this:
  Application <-> libfreenect <-> libusb-1.0 <-> Kinect
In Windows, the flow is as follows:
  Application <-> libfreenect <-> libusb-1.0-emu <-> libusb-win32 <-> Kinect
                                   (libusb-1.0)      (libusb-0.1)

The source code of the emulation layer is available at:
  libfreenect\platform\windows\libusb10emu\libusb-1.0
Keep in mind that libusb-win32 is still required, and can be downloaded from:
  http://sourceforge.net/apps/trac/libusb-win32/wiki
The latest snapshots are recommended, obtained directly from this URL:
  http://sourceforge.net/projects/libusb-win32/files/libusb-win32-snapshots

======================================================================================

2) libusbemu: Tips, Hints and Best Practices
   -----------------------------------------

----------------------------------------------------------------------------
TIP: Configure your project to build your application with a console window!
----------------------------------------------------------------------------

Since libusbemu is quite experimental, There is a fail guard within it. If for some
reason your system renders unresponsive, try to focus the console window of your
application and press [ESC].

This will trigger a special synchronization event within the libusbemu which will
interrupt the execution of any internal thread of libusbemu and prompt a message box
to the user asking for action. You can either resume execution (if you unintentionally
pressed [ESC] in the console window) or abort the libusbemu. The recent fixes in the
libusbemu don not seem to be causing any unresponsiveness, but caution is advised.

--------------------------------------------------
TIP: Do not change the default state of libusbemu.
--------------------------------------------------

The default libusbemu setup embraces the best performance currently found so far:
multi-threaded stream polling.

If one feels curious/adventurous, the USB isochronous reap strategy can be modified
within the emulator code but don't expect good results; the default ReapThreaded()
strategy is the most reliable and stable, while the others are very experimental.

----------------------------------------
TIP: Only use a single freenect context.
----------------------------------------

Multiple freenect contexts should be no problem in the future. Having more than one
device attached to the same context should work fine, but no tests were made so far.

--------------------------------------------------------------------------------------
TIP: Perform stream operations only in the thread that calls freenect_handle_events().
--------------------------------------------------------------------------------------

By stream operations I mean these:
  freenect_start_video()
  freenect_start_depth()
  freenect_stop_video()
  freenect_stop_depth()

A typical libfreenect application (check the official "examples" repository) will have
a dedicated thread to call freenect_handle_events(). This latter function is mainly
responsible for querying and dispatching streamed frames from the Kinect device to the
application (behind the scenes there is a call to libusb_handle_events()) and it is
advised that any stream operation intended by the application (like switching to some
different video format) should happen within the thread that calls the event handling
routine. The official libfreenect examples should give a good picture on how to do it
properly.

It is also advised to have only one thread calling freenect_handle_events(). Although
possible to mix such calls in different threads, there is no guarantee that libusbemu
is currently able to safely execute them in such fashion. Some tests were already been
done and it seems to work fine, but it may be pure luck... The libusbemu still lacks
in proper critical section protection within it.

======================================================================================

3) Overall performance of libfreenect in Windows
   ---------------------------------------------

Hardware:
* Notebook 
* CPU: Intel Core2 Duo 32bit [T7250] @ 2.0GHz
* RAM: 4GB RAM
* GPU: GeForce 8600M GT 256MB VRAM

Task: display of simultaneous RGB (Bayer-to-RGB) and depth streams (16bit unpadded) on
the screen through OpenGL textures. Application source code is identical for all tests
(except for the Zephod's version that required some interface adaptation).

The performance results below refer to the average frame time (one loop iteration).

Linux: Ubuntu Notebook 10.10
* compiler: gcc 4.4.5
* time measured via POSIX gettime()
  * Debug:   1.22ms (libfreenect also built in debug mode)
  * Release: 1.15ms

Win32: Windows 7 Enterprise 32bit
1) libfreenect with libusbemu:
   * compiler: VC++ 2010
   * time measured via QueryPerformanceFrequency() / QueryPerformanceCounter()
     * Debug:   4.01ms (libfreenect also built in debug mode)
     * Release: 2.75ms (run without debug)
2) Zephod's dedicated driver V16:
   http://ajaxorg.posterous.com/kinect-driver-for-windows-prototype
   * compiler: VC++ 2010
   * time measured via QueryPerformanceFrequency() / QueryPerformanceCounter()
     * Debug:   2.91ms (Zephod's driver also built in debug mode)
     * Release: 2.57ms (run without debug)

A Win32 MinGW-based (gcc/g++ 3.4.5) build of libfreenect with libusbemu using POSIX
gettime() also yielded to nearly identical performance results than VC++ 2010 with
Performance Counters. All of the Win32 performance results were also double-checked
with Fraps.

Discussion:

From these results, one may infer that Windows clearly has an overhead disadvantage.
However, this does not hold true: there is still plenty of time for the client code
to run its logic and keep a steady 60FPS (16.66ms per frame) real-time performance!
For example, from the Release mode results, in Windows one would still have about 
14ms per frame for the application, while in Linux this available frame time would
be around 15.5ms, a 1.5ms overhead difference between the platforms. Such a small
difference should not impose any special design considerations for the client code.

The overhead between libusbemu and Zephod's code is due the fact that libusbemu has
to inspect the usb packages before forwarding them to libfreenect which will then be
checked again within libfreenect. Another performance consideration is the fact that
Zephod's Bayer-to-RGB and bit-unpacking seem to be faster than the stream conversion
procedures provided by libfreenect.

Another performance impact between libusbemu and Zephod's code, is due the fact that
libusbemu makes heavy use of STL containers which in Debug mode tend to introduce a
significative overhead; such overhead disappears in Release mode since STL containers
are prone to be "inlined" and further optimized, besides the fact that many security
checks of the STL are disabled in Release builds. Moreover, libusbemu also uses lots
of synchronization directives, such as mutexes and conditional variables (events),
that impose extra overhead. Anyway, the overall overhead between the libusbemu and the
Zephod's dedicated Win32 driver is negligible (1.1ms for Debug builds and 0.2ms for
Release builds).

======================================================================================
