# cython: language_level=3
#
# This file is part of the OpenKinect Project. http://www.openkinect.org
#
# Copyright (c) 2026 individual OpenKinect contributors. See the CONTRIB file
# for details.
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

"""Skeleton, hand and gesture tracking from the Kinect's depth stream
(libfreenect_nui.h).

    import freenect, freenect_nui
    tracker = freenect_nui.Tracker()            # needs the models (see below)
    depth, _ = freenect.sync_get_depth(format=freenect.DEPTH_MM)
    frame = tracker.process(depth)
    for user in frame.tracked:
        head = user.joints[freenect_nui.HEAD]   # metres, camera space
        print(user.tracking_id, head, user.hand_state)

Skeletons have 32 joints: the Kinect SDK's 20 body joints, then each hand's
palm and five fingertips (fingers are seen within about 1.6 m). Coordinates
are the SDK's camera space in metres: x towards the user's right as they face
the Kinect, y up, z away from the Kinect.

The tracker uses Microsoft's body-part forest, which is not part of
libfreenect: fetch it once with `fwfetcher.py --nui --user` (Linux, macOS) or
`freenect-driver-install --fetch-nui-models` (Windows).
"""

from libc.stdint cimport uint8_t, uint16_t, uint32_t
from libc.string cimport memset
import numpy as np

cdef extern from "libfreenect_nui.h":
    enum:
        FREENECT_NUI_BODY_JOINTS
        FREENECT_NUI_JOINTS
        FREENECT_NUI_MAX_USERS
        FREENECT_NUI_SEATED
        FREENECT_NUI_HEADS
        FREENECT_NUI_MAX_HEADS
        FREENECT_NUI_EVENT_SWIPE_LEFT
        FREENECT_NUI_EVENT_SWIPE_RIGHT
        FREENECT_NUI_EVENT_SWIPE_UP
        FREENECT_NUI_EVENT_SWIPE_DOWN
        FREENECT_NUI_EVENT_PRESS
        FREENECT_NUI_EVENT_PRESS_END
        FREENECT_NUI_EVENT_GRIP
        FREENECT_NUI_EVENT_GRIP_END
    ctypedef int freenect_nui_hand_state
    ctypedef int freenect_nui_joint_state
    ctypedef int freenect_nui_user_state
    ctypedef int freenect_nui_grip

    ctypedef struct freenect_nui_point:
        float x
        float y
        float z
    ctypedef struct freenect_nui_rotation:
        float matrix[3][3]
        float quaternion[4]
    ctypedef struct freenect_nui_skeleton:
        freenect_nui_user_state state
        uint32_t tracking_id
        freenect_nui_point position
        freenect_nui_point joints[32]
        freenect_nui_joint_state joint_state[32]
        uint32_t quality
        freenect_nui_hand_state hand_state[2]
        int hand_holding[2]
        freenect_nui_rotation hand_orientation[2]
        int hands_clasped
    ctypedef struct freenect_nui_frame:
        uint32_t frame_number
        uint32_t timestamp
        float floor_plane[4]
        freenect_nui_skeleton users[6]
    ctypedef struct freenect_nui
    ctypedef struct freenect_nui_hand:
        freenect_nui_hand_state state
        float confidence
        freenect_nui_point palm
        float palm_radius
        float extent
        int fingers_valid
        int num_fingers
        freenect_nui_point fingertips[5]
        int finger_found[5]
        int pixels
        int holding
        freenect_nui_point direction
        freenect_nui_point normal
        freenect_nui_rotation orientation
        int finger_towards_kinect
    ctypedef struct freenect_nui_clasp:
        int clasped
        freenect_nui_hand_state state
        freenect_nui_point center
        freenect_nui_point tip
        freenect_nui_point direction
        int towards_kinect
    ctypedef struct freenect_nui_manipulation:
        int active
        int began
        int ended
        freenect_nui_grip grip
        freenect_nui_point center
        float distance
        float scale
        float rotation[4]
        freenect_nui_point translation
        float total_scale
        float total_rotation[4]
        freenect_nui_point total_translation
    ctypedef struct freenect_nui_hand_pointer:
        int active
        int primary
        float x
        float y
        float press
        int pressed
        int gripped
        uint32_t events
    ctypedef struct freenect_nui_head:
        uint32_t id
        int observed
        int rank
        float score
        freenect_nui_point position
        int bbox[4]
    ctypedef struct freenect_nui_smoothing:
        float smoothing
        float correction
        float prediction
        float jitter_radius
        float max_deviation_radius
    ctypedef struct freenect_nui_smoother
    ctypedef struct freenect_nui_bone:
        int end_joint
        int start_joint
        freenect_nui_rotation hierarchical
        freenect_nui_rotation absolute

    freenect_nui* freenect_nui_create(const char* models_dir, int flags)
    void freenect_nui_destroy(freenect_nui* nui)
    void freenect_nui_reset(freenect_nui* nui)
    int freenect_nui_process(freenect_nui* nui, const uint16_t* depth_mm, uint32_t timestamp, freenect_nui_frame* out) nogil
    int freenect_nui_get_player_index(freenect_nui* nui, uint8_t* out)
    int freenect_nui_set_max_tracked(freenect_nui* nui, int n)
    int freenect_nui_set_tracked_users(freenect_nui* nui, const uint32_t* ids, int n)
    int freenect_nui_get_hands(freenect_nui* nui, int user, freenect_nui_hand* hands)
    int freenect_nui_get_clasp(freenect_nui* nui, int user, freenect_nui_clasp* out)
    int freenect_nui_get_manipulation(freenect_nui* nui, int user, freenect_nui_manipulation* out)
    int freenect_nui_set_grip(freenect_nui* nui, freenect_nui_grip grip)
    int freenect_nui_get_hand_pointers(freenect_nui* nui, int user, freenect_nui_hand_pointer* out)
    int freenect_nui_get_heads(freenect_nui* nui, freenect_nui_head* heads)
    freenect_nui_smoother* freenect_nui_smoother_create()
    void freenect_nui_smoother_destroy(freenect_nui_smoother* s)
    int freenect_nui_smooth(freenect_nui_smoother* s, freenect_nui_frame* frame, const freenect_nui_smoothing* params)
    int freenect_nui_bone_orientations(const freenect_nui_skeleton* user, freenect_nui_bone* bones)

BODY_JOINTS = FREENECT_NUI_BODY_JOINTS
JOINTS = FREENECT_NUI_JOINTS
MAX_USERS = FREENECT_NUI_MAX_USERS
JOINT_NAMES = (
    "hip_center", "spine", "shoulder_center", "head",
    "shoulder_left", "elbow_left", "wrist_left", "hand_left",
    "shoulder_right", "elbow_right", "wrist_right", "hand_right",
    "hip_left", "knee_left", "ankle_left", "foot_left",
    "hip_right", "knee_right", "ankle_right", "foot_right",
    "palm_left", "thumb_left", "index_left", "middle_left", "ring_left", "little_left",
    "palm_right", "thumb_right", "index_right", "middle_right", "ring_right", "little_right",
)
for _i, _n in enumerate(JOINT_NAMES):
    globals()[_n.upper()] = _i
del _i, _n
# the bones of the body, (parent, joint), as the SDK draws them
BONES = ((0, 1), (1, 2), (2, 3), (2, 4), (4, 5), (5, 6), (6, 7), (2, 8), (8, 9), (9, 10), (10, 11),
         (0, 12), (12, 13), (13, 14), (14, 15), (0, 16), (16, 17), (17, 18), (18, 19))

JOINT_NOT_TRACKED, JOINT_INFERRED, JOINT_TRACKED = 0, 1, 2
USER_NOT_TRACKED, USER_POSITION_ONLY, USER_TRACKED = 0, 1, 2
HAND_UNKNOWN, HAND_OPEN, HAND_CLOSED, HAND_POINTING, HAND_GUN = 0, 1, 2, 3, 4
HAND_STATE_NAMES = ("unknown", "open", "closed", "pointing", "gun")
CLIPPED_RIGHT, CLIPPED_LEFT, CLIPPED_TOP, CLIPPED_BOTTOM = 1, 2, 4, 8
GRIP_NONE, GRIP_CLOSED, GRIP_HOLD, GRIP_TRIANGLE, GRIP_ANY = 0, 1, 2, 4, 7
EVENT_SWIPE_LEFT = FREENECT_NUI_EVENT_SWIPE_LEFT
EVENT_SWIPE_RIGHT = FREENECT_NUI_EVENT_SWIPE_RIGHT
EVENT_SWIPE_UP = FREENECT_NUI_EVENT_SWIPE_UP
EVENT_SWIPE_DOWN = FREENECT_NUI_EVENT_SWIPE_DOWN
EVENT_PRESS = FREENECT_NUI_EVENT_PRESS
EVENT_PRESS_END = FREENECT_NUI_EVENT_PRESS_END
EVENT_GRIP = FREENECT_NUI_EVENT_GRIP
EVENT_GRIP_END = FREENECT_NUI_EVENT_GRIP_END
EVENT_NAMES = {EVENT_SWIPE_LEFT: "swipe_left", EVENT_SWIPE_RIGHT: "swipe_right", EVENT_SWIPE_UP: "swipe_up",
               EVENT_SWIPE_DOWN: "swipe_down", EVENT_PRESS: "press", EVENT_PRESS_END: "press_end",
               EVENT_GRIP: "grip", EVENT_GRIP_END: "grip_end"}

cdef inline tuple _pt(freenect_nui_point p):
    return (p.x, p.y, p.z)

cdef object _rot(freenect_nui_rotation* r):
    m = np.array([[r.matrix[i][j] for j in range(3)] for i in range(3)], dtype=np.float32)
    q = np.array([r.quaternion[0], r.quaternion[1], r.quaternion[2], r.quaternion[3]], dtype=np.float32)
    return m, q


class Skeleton:
    """One user of a frame. joints: float32 (32, 3) in metres; joint_state:
    (32,) JOINT_*; hand_state, hand_holding: (left, right); hand_orientation:
    (2, 3, 3) rotation matrices (rows: X, Y along the hand towards the
    fingers, Z the palm's normal) and hand_quaternion (2, 4) (x, y, z, w)."""
    __slots__ = ("index", "state", "tracking_id", "position", "joints", "joint_state", "quality", "hand_state",
                 "hand_holding", "hand_orientation", "hand_quaternion", "hands_clasped")

    def __repr__(self):
        return "<Skeleton user %d (id %d), %s, %d joints tracked>" % (
            self.index, self.tracking_id, ("not tracked", "position only", "tracked")[self.state],
            int((self.joint_state == JOINT_TRACKED).sum()))

    def joint(self, name):
        """A joint by name ("head", "index_right", ...): (x, y, z), or None if not tracked."""
        i = JOINT_NAMES.index(name)
        return tuple(self.joints[i]) if self.joint_state[i] else None


cdef object _skeleton(int index, freenect_nui_skeleton* s):
    cdef int j
    k = Skeleton()
    k.index = index
    k.state = s.state
    k.tracking_id = s.tracking_id
    k.position = np.array(_pt(s.position), dtype=np.float32)
    joints = np.empty((FREENECT_NUI_JOINTS, 3), dtype=np.float32)
    states = np.empty(FREENECT_NUI_JOINTS, dtype=np.int8)
    for j in range(FREENECT_NUI_JOINTS):
        joints[j, 0] = s.joints[j].x
        joints[j, 1] = s.joints[j].y
        joints[j, 2] = s.joints[j].z
        states[j] = s.joint_state[j]
    k.joints = joints
    k.joint_state = states
    k.quality = s.quality
    k.hand_state = (int(s.hand_state[0]), int(s.hand_state[1]))
    k.hand_holding = (bool(s.hand_holding[0]), bool(s.hand_holding[1]))
    m0, q0 = _rot(&s.hand_orientation[0])
    m1, q1 = _rot(&s.hand_orientation[1])
    k.hand_orientation = np.stack([m0, m1])
    k.hand_quaternion = np.stack([q0, q1])
    k.hands_clasped = bool(s.hands_clasped)
    return k


cdef class Frame:
    """One tracked frame: frame_number, timestamp, floor_plane (A, B, C, D
    with A x + B y + C z + D = 0, or None), users (all 6 slots; users[i] is
    player index i + 1) and tracked (the users with a full skeleton)."""
    cdef freenect_nui_frame f

    @property
    def frame_number(self):
        return self.f.frame_number

    @property
    def timestamp(self):
        return self.f.timestamp

    @property
    def floor_plane(self):
        if self.f.floor_plane[0] == 0 and self.f.floor_plane[1] == 0 and self.f.floor_plane[2] == 0:
            return None
        return (self.f.floor_plane[0], self.f.floor_plane[1], self.f.floor_plane[2], self.f.floor_plane[3])

    @property
    def users(self):
        return [_skeleton(i, &self.f.users[i]) for i in range(FREENECT_NUI_MAX_USERS)]

    @property
    def tracked(self):
        return [_skeleton(i, &self.f.users[i]) for i in range(FREENECT_NUI_MAX_USERS)
                if self.f.users[i].state == USER_TRACKED]

    def bone_orientations(self, int user):
        """The 20 body bones of a tracked user, as the SDK's
        NuiSkeletonCalculateBoneOrientations: a dict with start_joint,
        end_joint (20,), hierarchical and absolute rotation matrices
        (20, 3, 3) and quaternions (20, 4). None if not tracked."""
        cdef freenect_nui_bone bones[20]
        cdef int b
        if user < 0 or user >= FREENECT_NUI_MAX_USERS:
            raise IndexError(user)
        if freenect_nui_bone_orientations(&self.f.users[user], bones) < 0:
            return None
        out = dict(start_joint=np.empty(20, np.int8), end_joint=np.empty(20, np.int8),
                   hierarchical=np.empty((20, 3, 3), np.float32), hierarchical_quaternion=np.empty((20, 4), np.float32),
                   absolute=np.empty((20, 3, 3), np.float32), absolute_quaternion=np.empty((20, 4), np.float32))
        for b in range(20):
            out["start_joint"][b] = bones[b].start_joint
            out["end_joint"][b] = bones[b].end_joint
            out["hierarchical"][b], out["hierarchical_quaternion"][b] = _rot(&bones[b].hierarchical)
            out["absolute"][b], out["absolute_quaternion"][b] = _rot(&bones[b].absolute)
        return out


cdef class Smoother:
    """The SDK's joint smoothing (NuiTransformSmooth, a Holt filter). Pass
    each Frame through smooth() in order; params: (smoothing, correction,
    prediction, jitter_radius, max_deviation_radius), default the SDK's
    (0.5, 0.5, 0.5, 0.05, 0.04)."""
    cdef freenect_nui_smoother* _s

    def __cinit__(self):
        self._s = freenect_nui_smoother_create()
        if self._s == NULL:
            raise MemoryError()

    def __dealloc__(self):
        if self._s != NULL:
            freenect_nui_smoother_destroy(self._s)

    def smooth(self, Frame frame, params=None):
        cdef freenect_nui_smoothing p
        if params is None:
            freenect_nui_smooth(self._s, &frame.f, NULL)
        else:
            p.smoothing, p.correction, p.prediction, p.jitter_radius, p.max_deviation_radius = params
            freenect_nui_smooth(self._s, &frame.f, &p)
        return frame


cdef object _hand(freenect_nui_hand* h):
    cdef int i
    m, q = _rot(&h.orientation)
    tips = {}
    for i in range(5):
        if h.finger_found[i]:
            tips[("thumb", "index", "middle", "ring", "little")[i]] = _pt(h.fingertips[i])
    return dict(state=int(h.state), confidence=h.confidence, palm=_pt(h.palm), palm_radius=h.palm_radius,
                extent=h.extent, fingers_valid=bool(h.fingers_valid), num_fingers=h.num_fingers, fingertips=tips,
                pixels=h.pixels, holding=bool(h.holding), direction=_pt(h.direction), normal=_pt(h.normal),
                orientation=m, quaternion=q, finger_towards_kinect=bool(h.finger_towards_kinect))


cdef class Tracker:
    """The skeleton tracker. Tracker(models_dir=None, seated=False, heads=False):
    models_dir is where DATABASE.WSDF is (None: where the fetchers install
    it, or $LIBFREENECT_NUI_MODELS); seated tracks only the upper body and
    runs the head detector (heads() ; heads=True runs it when standing too)."""
    cdef freenect_nui* _n
    cdef Frame _last

    def __cinit__(self, models_dir=None, seated=False, heads=False):
        cdef bytes d
        cdef int flags = (FREENECT_NUI_SEATED if seated else 0) | (FREENECT_NUI_HEADS if heads else 0)
        if models_dir is None:
            self._n = freenect_nui_create(NULL, flags)
        else:
            d = str(models_dir).encode()
            self._n = freenect_nui_create(d, flags)
        if self._n == NULL:
            raise FileNotFoundError(
                "the skeleton tracking model (DATABASE.WSDF) is not installed; fetch it with "
                "'fwfetcher.py --nui --user' (Linux, macOS) or 'freenect-driver-install --fetch-nui-models' (Windows), "
                "or set LIBFREENECT_NUI_MODELS")

    def __dealloc__(self):
        if self._n != NULL:
            freenect_nui_destroy(self._n)

    def reset(self):
        freenect_nui_reset(self._n)

    def process(self, depth_mm, int timestamp=0):
        """Tracks one 480x640 uint16 depth frame in mm (freenect.DEPTH_MM,
        0 = no data). Returns a Frame."""
        cdef const uint16_t[:, ::1] d = np.ascontiguousarray(depth_mm, dtype=np.uint16)
        cdef Frame fr = Frame.__new__(Frame)
        cdef int r
        if d.shape[0] != 480 or d.shape[1] != 640:
            raise ValueError("depth must be 480 x 640 (rows x columns)")
        with nogil:
            r = freenect_nui_process(self._n, &d[0, 0], <uint32_t> timestamp, &fr.f)
        if r < 0:
            raise RuntimeError("freenect_nui_process failed (%d)" % r)
        self._last = fr
        return fr

    def player_index(self):
        """uint8 (480, 640): the user (1..6) of every pixel of the last frame, 0 for none."""
        out = np.empty((480, 640), dtype=np.uint8)
        cdef uint8_t[:, ::1] o = out
        freenect_nui_get_player_index(self._n, &o[0, 0])
        return out

    def set_max_tracked(self, int n):
        """How many users get a full skeleton: the nearest n (default 2, at most 6)."""
        if freenect_nui_set_max_tracked(self._n, n) < 0:
            raise ValueError("0 to %d" % FREENECT_NUI_MAX_USERS)

    def set_tracked_users(self, ids=None):
        """Which users (tracking ids) get the skeletons; None: the nearest ones."""
        cdef uint32_t buf[6]
        cdef int i
        if ids is None:
            freenect_nui_set_tracked_users(self._n, NULL, 0)
            return
        ids = list(ids)[:FREENECT_NUI_MAX_USERS]
        for i in range(len(ids)):
            buf[i] = ids[i]
        freenect_nui_set_tracked_users(self._n, buf, len(ids))

    def set_grip(self, int grip):
        """Which grips start a two-hand manipulation: GRIP_* flags or'ed (default GRIP_ANY)."""
        if freenect_nui_set_grip(self._n, <freenect_nui_grip> grip) < 0:
            raise ValueError("GRIP_CLOSED, GRIP_HOLD, GRIP_TRIANGLE or a combination")

    def hands(self, int user):
        """(left, right) hand details of a tracked user of the last frame, as
        dicts, or None if the user has no skeleton."""
        cdef freenect_nui_hand h[2]
        if freenect_nui_get_hands(self._n, user, h) < 0:
            return None
        return (_hand(&h[0]), _hand(&h[1]))

    def clasp(self, int user):
        """The two hands together: a dict (clasped, state, center, tip,
        direction, towards_kinect), or None if the user has no skeleton."""
        cdef freenect_nui_clasp c
        if freenect_nui_get_clasp(self._n, user, &c) < 0:
            return None
        return dict(clasped=bool(c.clasped), state=int(c.state), center=_pt(c.center), tip=_pt(c.tip),
                    direction=_pt(c.direction), towards_kinect=bool(c.towards_kinect))

    def manipulation(self, int user):
        """Two hands holding a virtual object: a dict with active, began,
        ended, grip, center, distance, and the change since the last frame
        (scale, rotation as a quaternion (x, y, z, w), translation) and since
        the grip began (total_*). None if the user has no skeleton."""
        cdef freenect_nui_manipulation m
        if freenect_nui_get_manipulation(self._n, user, &m) < 0:
            return None
        return dict(active=bool(m.active), began=bool(m.began), ended=bool(m.ended), grip=int(m.grip),
                    center=_pt(m.center), distance=m.distance, scale=m.scale,
                    rotation=(m.rotation[0], m.rotation[1], m.rotation[2], m.rotation[3]),
                    translation=_pt(m.translation), total_scale=m.total_scale,
                    total_rotation=(m.total_rotation[0], m.total_rotation[1], m.total_rotation[2], m.total_rotation[3]),
                    total_translation=_pt(m.total_translation))

    def heads(self):
        """The head detector's tracked heads of the last frame (seated mode,
        or heads=True): a list of dicts (id, observed, rank, score, position
        of the head's front surface in metres, bbox left, top, right, bottom
        in 640x480 depth pixels), most likely first. None if the detector is
        not running (not seated, or HEADPOSITION.MSHDF not installed)."""
        cdef freenect_nui_head h[16]
        cdef int i
        if freenect_nui_get_heads(self._n, h) < 0:
            return None
        out = []
        for i in range(FREENECT_NUI_MAX_HEADS):
            if h[i].id:
                out.append(dict(id=h[i].id, observed=bool(h[i].observed), rank=h[i].rank, score=h[i].score,
                                position=_pt(h[i].position), bbox=(h[i].bbox[0], h[i].bbox[1], h[i].bbox[2], h[i].bbox[3])))
        out.sort(key=lambda d: d["rank"])
        return out

    def pointers(self, int user):
        """(left, right) hands as pointers: dicts with active, primary, x, y
        (0..1 across the hand's zone, (0, 0) top left), press, pressed,
        gripped, and events (a list of names: swipe_left, press, grip, ...).
        None if the user has no skeleton."""
        cdef freenect_nui_hand_pointer p[2]
        if freenect_nui_get_hand_pointers(self._n, user, p) < 0:
            return None
        out = []
        for i in range(2):
            out.append(dict(active=bool(p[i].active), primary=bool(p[i].primary), x=p[i].x, y=p[i].y,
                            press=p[i].press, pressed=bool(p[i].pressed), gripped=bool(p[i].gripped),
                            events=[n for f, n in EVENT_NAMES.items() if p[i].events & f]))
        return tuple(out)
