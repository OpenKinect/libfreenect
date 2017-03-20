import time

def init(index):
    return None

def set_led(led):
    return None

def get_tilt():
    return 15

def set_tilt(tilt):
    return None

def get_frame_params():
    return (640, 480)

def get_depths():
    return (tuple(range(640*480)), time.time(), None)

def get_image():
    return (tuple(range(640*480*3)), time.time(), None)

def start_cap():
    return None

def stop_cap():
    return None
