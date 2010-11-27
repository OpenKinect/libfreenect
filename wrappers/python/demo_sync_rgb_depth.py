import freenect


while 1:
    a, t0 = freenect.sync_get_depth_np()
    b, t1 = freenect.sync_get_rgb_np()
    print(t0, t1)
