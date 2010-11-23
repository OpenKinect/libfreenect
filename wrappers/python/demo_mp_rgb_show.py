#!/usr/bin/env python
import freenect
from demo_mp_depth_show import display
freenect.runloop(rgb=lambda *x: display(*freenect.rgb_cb_np(*x)))
