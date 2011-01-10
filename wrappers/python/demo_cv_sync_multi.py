#!/usr/bin/env python
"""This goes through each kinect on your system, grabs one frame and
displays it.  Uncomment the commented line to shut down after each frame
if your system can't handle it (will get very low FPS but it should work).
This will keep trying indeces until it finds one that doesn't work, then it
starts from 0.
"""
import freenect
import cv
import frame_convert

cv.NamedWindow('Depth')
cv.NamedWindow('Video')
ind = 0
print('%s\nPress ESC to stop' % __doc__)


def get_depth(ind):
    return frame_convert.pretty_depth_cv(freenect.sync_get_depth(ind)[0])


def get_video(ind):
    return frame_convert.video_cv(freenect.sync_get_video(ind)[0])


while 1:
    print(ind)
    try:
        depth = get_depth(ind)
        video = get_video(ind)
    except TypeError:
        ind = 0
        continue
    ind += 1
    cv.ShowImage('Depth', depth)
    cv.ShowImage('Video', video)
    if cv.WaitKey(10) == 27:
        break
    #freenect.sync_stop()  # NOTE: Uncomment if your machine can't handle it
