import cv2
import numpy as np
import yaml
import sys

VIDEO_PATH = '/home/nicolam/ros2_ws/checkerboard.mov'
BOARD_SIZE = (8, 5)
SQUARE_SIZE = 0.03       # meters
TARGET_SAMPLES = 20

objp = np.zeros((BOARD_SIZE[0] * BOARD_SIZE[1], 3), np.float32)
objp[:, :2] = np.mgrid[0:BOARD_SIZE[0], 0:BOARD_SIZE[1]].T.reshape(-1, 2)
objp *= SQUARE_SIZE

objpoints = []
imgpoints = []
image_size = None

flags = (cv2.CALIB_CB_ADAPTIVE_THRESH +
         cv2.CALIB_CB_NORMALIZE_IMAGE +
         cv2.CALIB_CB_FAST_CHECK)

cap = cv2.VideoCapture(VIDEO_PATH)
if not cap.isOpened():
    print(f'Could not open video: {VIDEO_PATH}')
    sys.exit(1)

total_frames = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
print(f'Video opened: {width}x{height}, {total_frames} total frames')

sample_frame_indices = set(
    int(i * total_frames / (TARGET_SAMPLES * 3)) for i in range(TARGET_SAMPLES * 3)
)

frame_idx = 0
found_count = 0
checked_count = 0

print('Scanning video for checkerboard (spread across full video)...')
while True:
    ret, frame = cap.read()
    if not ret:
        break
    frame_idx += 1
    if frame_idx not in sample_frame_indices:
        continue
    #if found_count >= TARGET_SAMPLES:
    #    break

    checked_count += 1
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    if image_size is None:
        image_size = gray.shape[::-1]

    found, corners = cv2.findChessboardCorners(gray, BOARD_SIZE, flags=flags)
    if found:
        criteria = (cv2.TERM_CRITERIA_EPS + cv2.TERM_CRITERIA_MAX_ITER, 30, 0.001)
        corners_refined = cv2.cornerSubPix(gray, corners, (11, 11), (-1, -1), criteria)
        objpoints.append(objp)
        imgpoints.append(corners_refined)
        found_count += 1
        print(f'  found checkerboard in frame {frame_idx} ({found_count}/{TARGET_SAMPLES})')

cap.release()
print(f'\nChecked {checked_count} candidate frames, found board in {found_count} of them.')

if found_count < 10:
    print('Not enough good views -- need at least 10.')
    sys.exit(1)

print(f'\nRunning calibration with {found_count} views...')
ret, camera_matrix, dist_coeffs, rvecs, tvecs = cv2.calibrateCamera(
    objpoints, imgpoints, image_size, None, None
)

print(f'Reprojection error: {ret:.4f} (lower is better; under ~0.5 is good)')
print(f'Camera matrix:\n{camera_matrix}')
print(f'Distortion coefficients:\n{dist_coeffs}')

calib_data = {
    'image_width': int(image_size[0]),
    'image_height': int(image_size[1]),
    'camera_name': 'webcam',
    'camera_matrix': {
        'rows': 3, 'cols': 3,
        'data': camera_matrix.flatten().tolist(),
    },
    'distortion_model': 'plumb_bob',
    'distortion_coefficients': {
        'rows': 1, 'cols': 5,
        'data': dist_coeffs.flatten().tolist(),
    },
    'rectification_matrix': {
        'rows': 3, 'cols': 3,
        'data': np.eye(3).flatten().tolist(),
    },
    'projection_matrix': {
        'rows': 3, 'cols': 4,
        'data': np.hstack([camera_matrix, np.zeros((3, 1))]).flatten().tolist(),
    },
}

out_path = '/home/nicolam/ros2_ws/camera_calibration.yaml'
with open(out_path, 'w') as f:
    yaml.dump(calib_data, f, default_flow_style=False)

print(f'\nSaved calibration to {out_path}')