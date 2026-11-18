"""Offline YOLO model benchmark on the same recorded camera images.

Requires: pip install ultralytics opencv-python
Optional GPU telemetry: pip install nvidia-ml-py
"""
import argparse
import csv
import json
import time
from pathlib import Path

import cv2
from ultralytics import YOLO


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--images', required=True)
    parser.add_argument('--model', default='yolov8n.pt')
    parser.add_argument('--output', default='benchmark_results.json')
    parser.add_argument('--predictions', default='predictions.json')
    parser.add_argument('--imgsz', type=int, default=640)
    parser.add_argument('--conf', type=float, default=0.35)
    parser.add_argument('--device', default='0')
    args = parser.parse_args()
    paths = sorted(p for p in Path(args.images).rglob('*') if p.suffix.lower() in {'.jpg', '.jpeg', '.png'})
    if not paths:
        raise SystemExit('No images found')
    model = YOLO(args.model)
    import torch
    cuda = args.device != 'cpu' and torch.cuda.is_available()
    def sync():
        if cuda:
            torch.cuda.synchronize()
    # Warm up to exclude one-time model initialization/compilation.
    sample = cv2.imread(str(paths[0]))
    if sample is None:
        raise SystemExit('Cannot read first image')
    for _ in range(5):
        model.predict(sample, imgsz=args.imgsz, conf=args.conf, device=args.device, verbose=False)
    sync()
    telemetry = []
    nvml = None
    try:
        import pynvml
        pynvml.nvmlInit()
        nvml = pynvml
        handle = nvml.nvmlDeviceGetHandleByIndex(0)
    except Exception:
        handle = None
    predictions = {}
    timings_ms = []
    try:
        for idx, path in enumerate(paths):
            frame = cv2.imread(str(path))
            if frame is None:
                print(f'Skipping unreadable image: {path}')
                continue
            sync()
            start = time.perf_counter()
            results = model.predict(frame, imgsz=args.imgsz, conf=args.conf, device=args.device, verbose=False)
            sync()
            timings_ms.append((time.perf_counter() - start) * 1000)
            result = results[0]
            boxes = []
            for b in result.boxes:
                boxes.append({
                    'class': result.names[int(b.cls.item())],
                    'box': [float(x) for x in b.xyxy[0].tolist()],
                    'confidence': float(b.conf.item()),
                })
            predictions[path.name] = boxes
            if handle is not None and idx % 10 == 0:
                util = nvml.nvmlDeviceGetUtilizationRates(handle).gpu
                vram = nvml.nvmlDeviceGetMemoryInfo(handle).used / 1024**2
                telemetry.append({'gpu_util_percent': util, 'vram_used_mb': vram})
    finally:
        if nvml is not None:
            nvml.nvmlShutdown()
    if not timings_ms:
        raise SystemExit('No valid images processed')
    total_seconds = sum(timings_ms) / 1000
    report = {
        'model': args.model, 'frames': len(timings_ms),
        'avg_inference_call_ms': sum(timings_ms) / len(timings_ms),
        'throughput_fps': len(timings_ms) / total_seconds,
        'gpu_util_avg_percent': sum(x['gpu_util_percent'] for x in telemetry) / len(telemetry) if telemetry else None,
        'vram_used_avg_mb': sum(x['vram_used_mb'] for x in telemetry) / len(telemetry) if telemetry else None,
        'note': 'Time includes Ultralytics preprocessing and postprocessing, not ROS2 or image disk loading. GPU utilization samples are approximate.',
    }
    Path(args.output).write_text(json.dumps(report, indent=2))
    Path(args.predictions).write_text(json.dumps(predictions, indent=2))
    print(json.dumps(report, indent=2))
    print(f'Saved {args.output} and {args.predictions}')


if __name__ == '__main__':
    main()
