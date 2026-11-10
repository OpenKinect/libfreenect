/*
 * This file is part of the OpenKinect Project. http://www.openkinect.org
 *
 * Copyright (c) 2026 individual OpenKinect contributors. See the CONTRIB file
 * for details.
 *
 * This code is licensed to you under the terms of the Apache License, version
 * 2.0, or, at your option, the terms of the GNU General Public License,
 * version 2.0. See the APACHE20 and GPL2 files for the text of the licenses,
 * or the following URLs:
 * http://www.apache.org/licenses/LICENSE-2.0
 * http://www.gnu.org/licenses/gpl-2.0.txt
 *
 * If you redistribute this file in source form, modified or unmodified, you
 * may:
 *   1) Leave this header intact and distribute it under the same terms,
 *      accompanying it with the APACHE20 and GPL20 files, or
 *   2) Delete the Apache 2.0 clause and accompany it with the GPL2 file, or
 *   3) Delete the GPL v2 clause and accompany it with the APACHE20 file
 * In all cases you must keep the copyright notice intact and include a copy
 * of the CONTRIB file.
 *
 * Binary distributions must follow the binary distribution requirements of
 * either License.
 */
#pragma once

#include <stdint.h>

#ifdef _WIN32
#define FREENECT_NUI_API __declspec(dllexport)
#else
#define FREENECT_NUI_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * libfreenect_nui: skeleton tracking for the Kinect v1 (optional addon,
 * library freenect_nui, CMake BUILD_NUI).
 *
 * It needs Microsoft's body-part forest (DATABASE.WSDF), which libfreenect
 * does not contain: freenect-driver-install --fetch-nui-models (Windows) or
 * fwfetcher.py --nui extract it from Microsoft's Kinect Runtime download on
 * your machine, and Microsoft's licence terms for the Runtime apply to it.
 *
 * Feed it 640x480 millimetre depth frames as libfreenect delivers them
 * (FREENECT_DEPTH_MM, not mirrored); it returns up to 6 users with 20 joints
 * each, in the Kinect for Windows SDK's skeleton space: metres, centred on
 * the depth camera, x to the Kinect's left, y up, z away from it (the
 * coordinate mapper's camera space, libfreenect_coordinate_mapper.h).
 */

/// The SDK's 20 body joints come first, in its order; then each hand's
/// palm and fingertips.
#define FREENECT_NUI_BODY_JOINTS 20
#define FREENECT_NUI_JOINTS 32
#define FREENECT_NUI_MAX_USERS 6

typedef enum {
	FREENECT_NUI_HIP_CENTER = 0, FREENECT_NUI_SPINE, FREENECT_NUI_SHOULDER_CENTER, FREENECT_NUI_HEAD,
	FREENECT_NUI_SHOULDER_LEFT, FREENECT_NUI_ELBOW_LEFT, FREENECT_NUI_WRIST_LEFT, FREENECT_NUI_HAND_LEFT,
	FREENECT_NUI_SHOULDER_RIGHT, FREENECT_NUI_ELBOW_RIGHT, FREENECT_NUI_WRIST_RIGHT, FREENECT_NUI_HAND_RIGHT,
	FREENECT_NUI_HIP_LEFT, FREENECT_NUI_KNEE_LEFT, FREENECT_NUI_ANKLE_LEFT, FREENECT_NUI_FOOT_LEFT,
	FREENECT_NUI_HIP_RIGHT, FREENECT_NUI_KNEE_RIGHT, FREENECT_NUI_ANKLE_RIGHT, FREENECT_NUI_FOOT_RIGHT,
	/* hands: the palm centre and the fingertips. Fingertips are tracked
	 * when the hand is close enough to resolve them (about 1.6 m or
	 * nearer) and the finger is extended; otherwise NOT_TRACKED. */
	FREENECT_NUI_PALM_LEFT = 20, FREENECT_NUI_THUMB_LEFT, FREENECT_NUI_INDEX_LEFT, FREENECT_NUI_MIDDLE_LEFT,
	FREENECT_NUI_RING_LEFT, FREENECT_NUI_LITTLE_LEFT,
	FREENECT_NUI_PALM_RIGHT = 26, FREENECT_NUI_THUMB_RIGHT, FREENECT_NUI_INDEX_RIGHT, FREENECT_NUI_MIDDLE_RIGHT,
	FREENECT_NUI_RING_RIGHT, FREENECT_NUI_LITTLE_RIGHT,
} freenect_nui_joint;

typedef enum {
	FREENECT_NUI_HAND_UNKNOWN = 0,  ///< not seen well enough
	FREENECT_NUI_HAND_OPEN = 1,
	FREENECT_NUI_HAND_CLOSED = 2,   ///< fist / grip
	FREENECT_NUI_HAND_POINTING = 3, ///< one finger out
	FREENECT_NUI_HAND_GUN = 4,      ///< index and thumb out, the thumb at a wide angle
} freenect_nui_hand_state;

typedef enum {
	FREENECT_NUI_JOINT_NOT_TRACKED = 0,
	FREENECT_NUI_JOINT_INFERRED = 1,
	FREENECT_NUI_JOINT_TRACKED = 2,
} freenect_nui_joint_state;

typedef enum {
	FREENECT_NUI_USER_NOT_TRACKED = 0,
	FREENECT_NUI_USER_POSITION_ONLY = 1, ///< a user is there; only its centre is known
	FREENECT_NUI_USER_TRACKED = 2,       ///< with a full skeleton
} freenect_nui_user_state;

/// Quality flags (the SDK's NUI_SKELETON_QUALITY_CLIPPED_*): the user
/// extends past that edge of the depth camera's view.
#define FREENECT_NUI_CLIPPED_RIGHT 1
#define FREENECT_NUI_CLIPPED_LEFT 2
#define FREENECT_NUI_CLIPPED_TOP 4
#define FREENECT_NUI_CLIPPED_BOTTOM 8

typedef struct {
	float x, y, z;
} freenect_nui_point;

/// A rotation, as NUI_SKELETON_BONE_ORIENTATION: rows of `matrix` are the
/// object's X, Y and Z axes in skeleton space; quaternion (x, y, z, w).
typedef struct {
	float matrix[3][3];
	float quaternion[4];
} freenect_nui_rotation;

/// One user. users[i] of a frame is player index i + 1 in the player
/// index image, as in the SDK.
typedef struct {
	freenect_nui_user_state state;
	uint32_t tracking_id;                       ///< stable while the user stays; never 0 when present
	freenect_nui_point position;                ///< the user's centre
	freenect_nui_point joints[FREENECT_NUI_JOINTS];
	freenect_nui_joint_state joint_state[FREENECT_NUI_JOINTS];
	uint32_t quality;                           ///< FREENECT_NUI_CLIPPED_*
	freenect_nui_hand_state hand_state[2];      ///< left, right
	int hand_holding[2];                        ///< 1 if that hand holds something (a stick, a handle)
	/// Each hand's orientation (see freenect_nui_hand): Y from the wrist
	/// towards the fingers, Z the palm's normal, X = Y x Z. Identity when
	/// the hand is not seen.
	freenect_nui_rotation hand_orientation[2];
	/// 1 if the hands are clasped together (see freenect_nui_get_clasp);
	/// hand_state then describes both hands together.
	int hands_clasped;
} freenect_nui_skeleton;

typedef struct {
	uint32_t frame_number;
	uint32_t timestamp;                         ///< as passed in
	float floor_plane[4];                       ///< A x + B y + C z + D = 0 (metres); all 0 if not found
	freenect_nui_skeleton users[FREENECT_NUI_MAX_USERS];
} freenect_nui_frame;

typedef struct freenect_nui freenect_nui;

/// Track seated users: only the upper body (joints 2-11) is reported, the
/// others are 0 and NOT_TRACKED (the SDK's seated mode). The head detector
/// (freenect_nui_get_heads) runs too, if HEADPOSITION.MSHDF is installed.
#define FREENECT_NUI_SEATED 1
/// Run the head detector also when not seated.
#define FREENECT_NUI_HEADS 2

/// A tracker. models_dir: where DATABASE.WSDF is (NULL: where the fetchers
/// install it, or $LIBFREENECT_NUI_MODELS). NULL if the model is missing or
/// not valid, or out of memory.
FREENECT_NUI_API freenect_nui* freenect_nui_create(const char* models_dir, int flags);
FREENECT_NUI_API void freenect_nui_destroy(freenect_nui* nui);
FREENECT_NUI_API void freenect_nui_reset(freenect_nui* nui);

/// Tracks one 640x480 depth frame (mm, 0 = no data, as FREENECT_DEPTH_MM).
/// Returns 0, or < 0 on error.
FREENECT_NUI_API int freenect_nui_process(freenect_nui* nui, const uint16_t* depth_mm, uint32_t timestamp,
                                          freenect_nui_frame* out);

/// The player index (0: none, 1..6) of every pixel of the last frame, 640x480
/// in libfreenect's orientation (aligned with the depth frame passed in).
FREENECT_NUI_API int freenect_nui_get_player_index(freenect_nui* nui, uint8_t* out);

/// How many users get a full skeleton: the SDK tracks the 2 nearest
/// (default 2, at most 6).
FREENECT_NUI_API int freenect_nui_set_max_tracked(freenect_nui* nui, int n);
/// Which users get the full skeletons (tracking ids; 0 = free choice), as
/// NuiSkeletonSetTrackedSkeletons. NULL returns to the nearest ones.
FREENECT_NUI_API int freenect_nui_set_tracked_users(freenect_nui* nui, const uint32_t* ids, int n);

/* ------------------------------------------------------------ hands */

/// More about one hand than the skeleton's joints carry (the palm and
/// fingertip joints and the hand state are in freenect_nui_skeleton).
typedef struct {
	freenect_nui_hand_state state;
	float confidence;                     ///< 0..1, from how many pixels the hand has
	freenect_nui_point palm;              ///< palm centre (skeleton space)
	float palm_radius;                    ///< m
	float extent;                         ///< farthest point of the hand from the palm centre, m
	int fingers_valid;                    ///< 1 if the hand was resolved well enough for fingers
	int num_fingers;                      ///< extended fingers found, 0..5
	freenect_nui_point fingertips[5];     ///< thumb, index, middle, ring, little (as the joints)
	int finger_found[5];                  ///< which of those are extended and seen
	int pixels;                           ///< hand pixels in the 640x480 depth
	int holding;                          ///< 1 if the hand holds something that sticks out of it (a stick, a handle)
	/// Orientation, from the plane and the long axis of the hand's pixels:
	/// direction from the wrist towards the fingers (for a fist, along the
	/// forearm), and the normal of the hand's plane. Depth cannot tell the
	/// palm from the back of the hand, so the normal points to the Kinect's
	/// side; for a hand seen edge-on, away from the body's midline.
	freenect_nui_point direction, normal;
	freenect_nui_rotation orientation;    ///< Y = direction, Z = normal, X = Y x Z
	/// 1 if a finger points at the Kinect: it does not stick out of the
	/// hand's outline then, only towards the Kinect in depth. It is reported
	/// as the index finger (fingertips[1]).
	int finger_towards_kinect;
} freenect_nui_hand;

/// The two hands together (clasped): their pixels are then one region.
typedef struct {
	int clasped;                          ///< 1 if the hands are together
	/// CLOSED: clasped with nothing out; POINTING / GUN: fingers out
	/// together, along the arms (a two-hand gun; GUN if a thumb is also up)
	freenect_nui_hand_state state;
	freenect_nui_point center;            ///< of the clasped hands
	freenect_nui_point tip;               ///< the fingertips, if pointing
	freenect_nui_point direction;         ///< where they point (from the wrists to the tip), if pointing
	int towards_kinect;                   ///< 1 if they point at the Kinect
} freenect_nui_clasp;

/// The hands (0: left, 1: right) of users[user] of the last frame, which
/// must be TRACKED: the same analysis that filled the skeleton's hand
/// joints. Returns 0, or -1 if the user has no skeleton.
FREENECT_NUI_API int freenect_nui_get_hands(freenect_nui* nui, int user, freenect_nui_hand hands[2]);

/// Whether users[user]'s hands are clasped together, and what they form.
/// Returns 0, or -1 if the user has no skeleton.
FREENECT_NUI_API int freenect_nui_get_clasp(freenect_nui* nui, int user, freenect_nui_clasp* out);

/* ------------------------------------------------------------ two-hand manipulation */

/// How an object is held between the two hands. Flags, so that
/// freenect_nui_set_grip can allow several.
typedef enum {
	FREENECT_NUI_GRIP_NONE = 0,
	FREENECT_NUI_GRIP_CLOSED = 1, ///< both fists closed (grabbing)
	FREENECT_NUI_GRIP_HOLD = 2,   ///< open hands, palms facing each other (holding a ball)
	/// a triangle in front of the Kinect: thumb and index out on both hands
	/// (the gun state; fingers are seen within about 1.6 m), the thumbs
	/// pointing towards each other, the index fingers up. Apart zooms in,
	/// together zooms out; turning or tipping the triangle turns the object.
	FREENECT_NUI_GRIP_TRIANGLE = 4,
	FREENECT_NUI_GRIP_ANY = 7,    ///< for freenect_nui_set_grip: all of them
} freenect_nui_grip;

/// Two hands holding a virtual object in front of the user: turn it, zoom
/// it (hands apart / together), move it. Starts after 3 frames of a grip
/// with both hands at least 15 cm in front of the shoulders; ends after 4
/// frames without (or with another grip). The deltas are the change since
/// the last frame (only
/// reported once they pass about 0.5 degrees, 0.3 % or 3 mm; smaller ones
/// add up until they do), so applying each frame's delta gives the total.
typedef struct {
	int active;                         ///< the object is held this frame
	int began, ended;                   ///< the grip began / ended this frame
	freenect_nui_grip grip;
	freenect_nui_point center;          ///< between the palms (skeleton space, m)
	float distance;                     ///< between the palms, m
	float scale;                        ///< distance change since the last frame: > 1 apart (zoom in), < 1 together
	float rotation[4];                  ///< rotation since the last frame, quaternion (x, y, z, w), skeleton space
	freenect_nui_point translation;     ///< centre movement since the last frame, m
	float total_scale;                  ///< since the grip began
	float total_rotation[4];
	freenect_nui_point total_translation;
} freenect_nui_manipulation;

/// The two-hand manipulation of users[user] in the last frame. Returns 0
/// (out->active says whether something is held), or -1 if the user has no
/// skeleton.
FREENECT_NUI_API int freenect_nui_get_manipulation(freenect_nui* nui, int user, freenect_nui_manipulation* out);
/// Which grips start a manipulation: FREENECT_NUI_GRIP_* flags or'ed
/// together (default FREENECT_NUI_GRIP_ANY).
FREENECT_NUI_API int freenect_nui_set_grip(freenect_nui* nui, freenect_nui_grip grip);

/* ------------------------------------------------------------ one-hand gestures */

/// One-hand events (flags; several can happen in one frame).
#define FREENECT_NUI_EVENT_SWIPE_LEFT 0x01  ///< a fast move to the user's left
#define FREENECT_NUI_EVENT_SWIPE_RIGHT 0x02
#define FREENECT_NUI_EVENT_SWIPE_UP 0x04
#define FREENECT_NUI_EVENT_SWIPE_DOWN 0x08
#define FREENECT_NUI_EVENT_PRESS 0x10       ///< pushed towards the Kinect (a click)
#define FREENECT_NUI_EVENT_PRESS_END 0x20   ///< pulled back again
#define FREENECT_NUI_EVENT_GRIP 0x40        ///< the hand closed (grab; drag with the cursor)
#define FREENECT_NUI_EVENT_GRIP_END 0x80    ///< and opened again (drop)

/// One hand as a pointer on a screen the user faces (as the SDK's
/// interaction stream): raise the hand in front of you into its interaction
/// zone, a box about 60 cm wide and 45 cm high in front of that side's
/// shoulder. The frame counts assume 30 frames a second.
typedef struct {
	int active;                ///< the hand is in its zone, at least 20 cm in front of the shoulder
	int primary;               ///< the hand to follow: the active one, or of two the one further forward
	float x, y;                ///< cursor, 0..1 across the zone, as the user sees the screen: (0, 0) top left
	float press;               ///< how far pushed towards the Kinect: 0 resting .. 1 pressed (about 12 cm)
	int pressed;               ///< pushed in (from press 1 until it is back under 0.5)
	int gripped;               ///< closed while active (grabbing; move to drag)
	uint32_t events;           ///< FREENECT_NUI_EVENT_* of this frame
} freenect_nui_hand_pointer;

/// Both hands (0: left, 1: right) of users[user] as pointers. Swipes need a
/// move of 30 cm within half a second, mostly along one axis, and are then
/// paused for half a second. While two hands manipulate an object or are
/// clasped, there are no events. Returns 0, or -1 if the user has no
/// skeleton.
FREENECT_NUI_API int freenect_nui_get_hand_pointers(freenect_nui* nui, int user, freenect_nui_hand_pointer out[2]);

/* ------------------------------------------------------------ heads */

/// The seated-mode head detector, as the SDK's (Microsoft's head forest
/// HEADPOSITION.MSHDF on an 80x60 grid, with its head tracks): heads are
/// found by shape alone, before and independently of the skeletons.
#define FREENECT_NUI_MAX_HEADS 16

typedef struct {
	uint32_t id;                    ///< the head track's id; 0: an empty record
	int observed;                   ///< seen in this frame
	int rank;                       ///< 1 = the most likely head; confirmed heads before lost ones
	float score;
	freenect_nui_point position;    ///< the head's front surface, skeleton space (m)
	int bbox[4];                    ///< left, top, right, bottom in 640x480 depth pixels (libfreenect's orientation)
} freenect_nui_head;

/// The tracked heads of the last frame, by record (each head keeps its
/// record while tracked). Needs FREENECT_NUI_SEATED or FREENECT_NUI_HEADS
/// and the head model. Returns how many records are in use, or -1 if the
/// head detector is not running.
FREENECT_NUI_API int freenect_nui_get_heads(freenect_nui* nui, freenect_nui_head heads[FREENECT_NUI_MAX_HEADS]);

/* ------------------------------------------------------------ smoothing */

/// The SDK's NuiTransformSmooth parameters (Holt double exponential filter).
typedef struct {
	float smoothing, correction, prediction, jitter_radius, max_deviation_radius;
} freenect_nui_smoothing;

/// The SDK's defaults: 0.5, 0.5, 0.5, 0.05 m, 0.04 m.
#define FREENECT_NUI_SMOOTHING_DEFAULT { 0.5f, 0.5f, 0.5f, 0.05f, 0.04f }

typedef struct freenect_nui_smoother freenect_nui_smoother;
FREENECT_NUI_API freenect_nui_smoother* freenect_nui_smoother_create(void);
FREENECT_NUI_API void freenect_nui_smoother_destroy(freenect_nui_smoother* s);
/// Smooths the tracked users of a frame in place, as NuiTransformSmooth
/// (params NULL: the defaults). History is kept per user slot.
FREENECT_NUI_API int freenect_nui_smooth(freenect_nui_smoother* s, freenect_nui_frame* frame,
                                         const freenect_nui_smoothing* params);

/* ------------------------------------------------------------ orientations */

/// A bone's rotation (freenect_nui_rotation): Y along the bone.
typedef struct {
	int end_joint, start_joint;
	freenect_nui_rotation hierarchical; ///< relative to the parent bone
	freenect_nui_rotation absolute;     ///< relative to the camera
} freenect_nui_bone;

/// The 20 body bone orientations of a tracked user, as
/// NuiSkeletonCalculateBoneOrientations (the hand joints have none).
/// Returns 0, or -1 if the user is not tracked (bones are then identity).
FREENECT_NUI_API int freenect_nui_bone_orientations(const freenect_nui_skeleton* user,
                                                    freenect_nui_bone bones[FREENECT_NUI_BODY_JOINTS]);

#ifdef __cplusplus
}
#endif
