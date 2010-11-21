from freenect import *
import matplotlib.pyplot as mp

mp.ion()
image = None


def display(dev, data, timestamp):
    global image
    if image:
        image.set_data(data)
    else:
        image = mp.imshow(data, interpolation='nearest', animated=True)
    mp.draw()

if __name__ == '__main__':
    runloop(depth=depth_cb_np_factory(display))
