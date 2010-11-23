#!/usr/bin/env python
#
# Copyright (c) 2010 Andrew Miller (amiller@dappervision.com)
#
# This code is licensed to you under the terms of the Apache License, version
# 2.0, or, at your option, the terms of the GNU General Public License,
# version 2.0. See the APACHE20 and GPL2 files for the text of the licenses,
# or the following URLs:
# http://www.apache.org/licenses/LICENSE-2.0
# http://www.gnu.org/licenses/gpl-2.0.txt
#
# If you redistribute this file in source form, modified or unmodified, you
# may:
#   1) Leave this header intact and distribute it under the same terms,
#      accompanying it with the APACHE20 and GPL20 files, or
#   2) Delete the Apache 2.0 clause and accompany it with the GPL2 file, or
#   3) Delete the GPL v2 clause and accompany it with the APACHE20 file
# In all cases you must keep the copyright notice intact and include a copy
# of the CONTRIB file.
#
# Binary distributions must follow the binary distribution requirements of
# either License.

import freenect
import numpy as np
from threading import Condition, Thread

depthcond = Condition()
rgbcond = Condition()
depthqueue = []
rgbqueue = []

# If we get a new frame, put it in the queue
def _depth_cb(dev, string, ts):
  global depthqueue
  with depthcond:
    depthqueue = [string]
    depthcond.notify()

# If we get a new frame, put it in the queue
def _rgb_cb(dev, string, ts):
  global rgbqueue
  with rgbcond:
    rgbqueue = [string]
    rgbcond.notify()

depth_cb = lambda *x: _depth_cb(*freenect.depth_cb_np(*x))
rgb_cb = lambda *x: _rgb_cb(*freenect.rgb_cb_np(*x))

def get_depth():
  """Grabs a new depth frame, blocking until the background thread provides one.
  
     Returns:
       a numpy array, with shape: (480,640), dtype: np.uint16
  """
  global depthqueue
  with depthcond:
    while not depthqueue:
      depthcond.wait(5)
    string = depthqueue[0]
    depthqueue = []
  data = np.fromstring(string, dtype=np.uint16)
  data.resize((480, 640))
  return data

def get_rgb():
  """Grabs a new RGB frame, blocking until the background thread provides one.
  
     Returns:
       a numpy array, with shape: (480,640,3), dtype: np.uint8
  """
  global rgbqueue
  with rgbcond:
    while not rgbqueue:
      rgbcond.wait(5)
    string = rgbqueue[0]
    rgbqueue = []
  data = np.fromstring(string, dtype=np.uint8)
  data.resize((480,640,3))
  return data
  

# The first time around: start a background thread to connect to run freenect. 
try:
  loopthread == ''
except:
  loopthread = Thread(target=freenect.runloop, kwargs=dict(depth=depth_cb, rgb=rgb_cb))
  loopthread.start()
  
