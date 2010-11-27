#!/usr/bin/env python
import freenect
import matplotlib.pyplot as mp

mp.ion()
mp.figure(1)
image_depth = mp.imshow(freenect.sync_get_depth_np()[0], interpolation='nearest', animated=True)
mp.figure(2)
image_rgb = mp.imshow(freenect.sync_get_rgb_np()[0], interpolation='nearest', animated=True)

while 1:
    mp.figure(1)
    image_depth.set_data(freenect.sync_get_depth_np()[0])
    mp.figure(2)
    image_rgb.set_data(freenect.sync_get_rgb_np()[0])
    mp.draw()
