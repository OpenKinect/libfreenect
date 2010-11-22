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

# Binary distributions must follow the binary distribution requirements of
# either License.

import pykinect_c as _clib
#import fake_pykinect_c as _clib

(FRAME_WIDTH, FRAME_HEIGHT) = _clib.get_frame_params()

class DepthData(object):
    """ Provides direct access to the depth data.

    The timestamp is also available in the `timestamp` property.

    Access can be done with x, y coordinates by subscripting multiple values:

    >>> print dd[0, 0] # print the topleft pixel
    >>> print dd[640,480] # print the bottomright pixel

    The raw data can be accessed with a single subscript:

    >>> assert dd[0] == dd[0, 0] # equivalent calls
    >>> print dd[641] # The first pixel on the second line (x=1, y=0)

    And the entire object can be iterated through:

    >>> for depth in depths:
    ...     if depth > threshold:
    ...         do_something(depth)
    """

    def __getitem__(self, key):
        try:
            abspos = (key[0] + key[1]*FRAME_WIDTH)
            return self.data[abspos]
        except TypeError:
            return self.data[key]

    def __iter__(self):
        return self.data.__iter__()

class ImageData(object):
    """ A light wrapper around a set of RGB data.

    The timestamp is also available in the `timestamp` property.

    Access can be done with x, y coordinates by subscripting multiple values:

    >>> print img[0, 0] # print the topleft (r,g,b) value
    >>> print img[640,480] # print the bottommost (r,g,b) value

    The raw data can be accessed with a single subscript:

    >>> print img[0] The R value of the (0,0) pixel
    >>> print img[1] The G value of the same pixel
    >>> print img[2] The B value of the same pixel
    >>> print img[3] The R value of the (1,0) pixel

    and so forth

    The entire object can be iterated through, generating (r,g,b) tuples

    >>> for rgb in img:
    ...     if rgb[1] > green_threshold:
    ...         do_something(rgb)
    """

    def __getitem__(self, key):
        # Use direct access so we don't have to reorder the data (costly for
        # each frame)
        try:
            abspos = (key[0] + key[1]*FRAME_WIDTH)*3
            return self.data[abspos:abspos+3]
        except TypeError:
            return self.data[key]

    def __iter__(self):
        i = self.data.__iter__()
        while True:
            yield (i.next(), i.next(), i.next())

if __name__ == "__main__":
    # Unit tests
    depths = object.__new__(DepthData)
    depths.data = tuple(range(640*480))

    img = object.__new__(ImageData)
    img.data = tuple(range(640*480*3))

    assert depths[0, 0] == 0
    assert img[0, 0] == (0, 1, 2)
    assert img[0] == 0

    assert depths[0] == 0

    assert img[0, 1] == (640*3, 640*3+1, 640*3+2)
    assert img[1, 0] == (3, 4, 5)

    assert depths[0, 1] == 640
    assert depths[1, 0] == 1
