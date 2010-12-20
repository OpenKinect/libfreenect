======================================================================================

1) libfreenect source code: incompatibilities with Visual C++

Main problem: The Microsoft C compiler does not implement all the C99 standard.
An attempt to compile the current libfreenect with Visual C++ will trigger a lot
of errors. A simple workaround is to tell Visual Studio to compile all .c files
within a project using the C++ compiler:
  Project >> Properties >> C/C++ >> Advanced >> Compile As: Compile as C++ Code (/TP)
This will get rid of most errors, except those regarding implicit pointer casts.
Here are a few examples of such implicit pointer casts from within libfreenect:
  tilt.c      dev->raw_state.tilt_status = buf[9];
  core.c      *ctx = malloc(sizeof(freenect_context));
  cameras.c   strm->raw_buf = strm->proc_buf;
It seems that it is not possible to force Visual C++ to perform such implicit casts
(if anyone knows how, please tell me :-).
Such implicit casts then have to be made explicit:
              dev->raw_state.tilt_status = (freenect_tilt_status_code)buf[9];
              *ctx = (freenect_context*)malloc(sizeof(freenect_context));
              strm->raw_buf = (uint8_t*)strm->proc_buf;
Fortunately, they are not many, and it can be done in a couple of minutes.
This should impose a minimal burden to the Win32 repository maintainers.

Another problem is that Visual C++ does not offer the <unistd.h> and <stdbool.h>
headers. I've created them manually (they are quite simple) and placed within the
"libfreenect\platform\windows" folder. The <stdbool.h> will actually be a dummy
header because C++ already defines the "bool" type semantics. The <unistd.h> has to
include <stdint.h> and define the "ssize_t" type.

The "freenect_internal.h" makes use of GCC's "__attribute__" keyword, and there is no
such a thing in Visual C++. Fortunately, this header is not exposed for library users,
and is just required during the library build. One could simple remove this keyword or
define a dummy macro for it. I have decided to remove it.

Another issue is that since all .c files were forced to be compiled as C++ code,
the "libfreenect.h" header no longer requires the "__cpluscplus extern C" idiom.
I have just commented this guard in the header.

The final issue is regarding libusb. libfreenect uses libusb-1.0 API, which is not
yet available for Windows. The latest version of libusb-win32 is based on the old
libusb-0.1 API. Fortunately, the libusb-1.0 API can be emulated, at some extent, via
what is available from the libusb-0.1 API. The source code of the emulation layer is
available at:
  libfreenect\platform\windows\libusb10emu\libusb-1.0
Keep in mind that libusb-0.1 (libusb-win32) is required and can be downloaded from:
  http://sourceforge.net/apps/trac/libusb-win32/wiki
The latest snapshots are recommended and can be obtained directly from this URL:
  http://sourceforge.net/projects/libusb-win32/files/libusb-win32-snapshots

======================================================================================

2) libusbemu: best practices


TIP: Configure your project to build your application with a console window!

Since libusbemu is quite experimental, I've placed a fail guard within it. If for some
reason your system renders unresponsive, try to focus the console window of your
application and press [ESC].
This will trigger a special synchronization event within the libusbemu which will
interrupt the execution of any internal thread of libusbemu and prompt a message box
to the user asking for action. You can either resume execution (if you unintentionally
pressed [ESC] in the console window) or abort the libusbemu. The recent fixes in the
libusbemu should not cause any unresponsiveness, but you never know...


TIP: Do not change the default state of libusbemu.

The default setup embraces the best performance current available, that is,
multi-threaded stream polling.
If you feel curious/adventurous, go ahead and change the ReapStrategy within the
emulator and see how it performs to you. Just be warned that the default ReapThreaded
strategy is quite reliable and stable, while the others are pretty crappy and
experimental.


TIP: Only use a single freenect context.

Multiple freenect contexts should be no problem, but no tests were performed so far.


TIP: Be sure that streaming operations are only being called within the thread that
is responsible to call freenect_handle_events() in your logic.

By streaming operations I mean these:
  freenect_start_video()
  freenect_start_depth()
  freenect_stop_video()
  freenect_stop_depth()

Also, be sure that only one thread call freenect_handle_events().

Although possible to mix such calls in different threads, there is no guarantee that
libusbemu is currently able to safely execute them in such fashion. Some tests were
already been done and it seems to work fine, but it may be pure luck... The libusbemu
still lacks in proper critical section protection within it.

======================================================================================
