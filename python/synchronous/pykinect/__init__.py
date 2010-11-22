# This file is part of the OpenKinect Project. http://www.openkinect.org
#
# Copyright (c) 2010 Kelvie Wong <kelvie@ieee.org>
#
# This code is licensed to you under the terms of the Apache License, version
# 2.0, or, at your option, the terms of the GNU General Public License,
# version 2.0. See the APACHE20 and GPL20 files for the text of the licenses,
# or the following URLs:
# http://www.apache.org/licenses/LICENSE-2.0
# http://www.gnu.org/licenses/gpl-2.0.txt
#
# If you redistribute this file in source form, modified or unmodified,
# you may:
# 1) Leave this header intact and distribute it under the same terms,
# accompanying it with the APACHE20 and GPL20 files, or
# 2) Delete the Apache 2.0 clause and accompany it with the GPL20 file, or
# 3) Delete the GPL v2.0 clause and accompany it with the APACHE20 file
# In all cases you must keep the copyright notice intact and include a copy
# of the CONTRIB file.
#
# Binary distributions must follow the binary distribution requirements of
# either License.

import pykinect_c as _clib
#import fake_pykinect_c as _clib

from datastructs import DepthData, ImageData

def _checkcall(func, *args):
    """ Check the function's return value (or last return value, if multiple
    values are returned).  If an error message is returned, raise a RuntimeError
    with it. """

    result = func(*args)

    err = result[-1] if isinstance(result, tuple) else result
    if err:
        raise RuntimeError(err)

class Kinect(object):
    """ A synchronous interface to the OpenKinect freekinect library

    Usage:

    >>> k = Kinect()
    >>> depths = k.get_depths() # Get image data
    >>> img = k.get_image()

    >>> k.led = 'yellow' # set the LED
    >>> k.tilt = 25 # set the tilt angle
    """

    def __init__(self, autostart=True, device_index=0):
        self.started = False

        _checkcall(_clib.init, device_index)

        if autostart:
            self.start()

    led_vals = {
        'off': 0,
        'green': 1,
        'red': 2,
        'yellow': 3,
        'blink yellow': 4,
        'blink green': 5,
        'blink red yellow': 6,
        }

    def get_led(self):
        # TODO
        pass

    def set_led(self, led):
        value = ledvals.get(led, led)
        _checkcall(_clib.set_led, value)

    def get_tilt(self):
        return _clib.get_tilt()

    def set_tilt(self, tilt):
        _checkcall(_clib.set_tilt, tilt)

    led = property(fget=get_led, fset=set_led,
                   doc="""The LED value. Possible values are: %s """ %
                   ", ".join(led_vals.keys()))

    tilt = property(fget=get_tilt, fset=set_tilt,
                    doc="Units are in degrees from horizontal.")

    def get_depths(self):
        """ Returns the depth data of the most current frame.

        This returns a DepthData object, whose raw data can be iterated through:
        >>> dd = k.get_depths()
        >>> for pixel in dd:
        ...    assert isinstance(pixel, int)

        or directly accessed with two subscripts:
        >>> dd = k.get_depths()
        >>> dd[1, 2]
        52
        >>> dd[13]
        33

        See DepthData for more information.
        """
        result = DepthData()
        data, timestamp, err = _clib.get_depths()
        if data:
            result.data = data
            result.timestamp = timestamp
        else:
            raise RuntimeError(err)

    def get_image(self):
        """ Returns the RGB data of the most current frame

        This returns an ImageData object, whose raw data can be iterated
        through:
        >>> img = k.get_image()
        >>> for pixel in img:
        ...    assert isinstance(pixel, (int, int, int))

        or directly accessed with two subscripts:
        >>> img = a.get_image()
        >>> img[1, 2]
        (43, 92, 61)o
        >>> img[13]
        33

        See ImageData for more information.
        """
        result = ImageData()
        data, timestamp, err = _clib.get_image()
        if data:
            result.data = data
            result.timestamp = timestamp
        else:
            raise RuntimeError(err)
        return result

    def start(self):
        """ Starts capture """
        _checkcall(_clib.start_cap)
        self.started = True

    def stop(self):
        """ Stops capture """
        _checkcall(_clib.stop_cap)

if __name__ == "__main__":
    # Unit tests

    # Check _checkcall
    def return_error():
        return "ERROR"

    try:
        _checkcall(return_error)
    except RuntimeError, e:
        assert str(e) == "ERROR", "Wrong RuntimeError raised"
    else:
        assert False, "RuntimeError was not raised"
