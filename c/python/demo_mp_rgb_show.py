from freenect import *
from demo_mp_depth_show import display
runloop(lambda *x: None, rgb_cb_factory(display))
