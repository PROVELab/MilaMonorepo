
"""Evaluate YOLO 2D detections against Isaac Sim ground truth.

Supported modes:
1. Isaac Sim Replicator dataset + YOLO model (.pt or .engine)
2. Original ground-truth JSON + predictions JSON

Replicator files:
    rgb_0000.png
    bounding_box_2d_tight_0000.npy
    bounding_box_2d_tight_labels_0000.json

Bounding boxes use pixel coordinates [xmin, ymin, xmax, ymax].

Note:
    Replicator 2D boxes do not provide object distances.
    Distance filtering is disabled unless --max-distance is supplied.
"""

import argparse
import json
from pathlib import Path


def iou(a, b):
    x1, y1 = max(a[0], b[0]), max(a[1], b[1])
    x2, y2 = min(a[2], b[2]), min(a[3], b[3])

    intersection = max(0, x2 - x1) * max(0, y2 - y1)

    area_a = max(0, a[2] - a[0]) * max(0, a[3] - a[1])
    area_b = max(0, b[2] - b[0]) * max(0, b[3] - b[1])

    union = area_a + area_b - intersection
    return intersection / union if union else 0.0


def load_isaac_ground_truth(dataset):
    """Read native Isaac Sim Replicator annotations."""
    import numpy as np

    dataset = Path(dataset)
    truth = {}

    images = sorted(dataset.glob("rgb_*.png"))

    if not images:
        raise ValueError(f"No rgb_*.png images found in {dataset}")

    for image in images:
        frame_id = image.stem.removeprefix("rgb_")

        boxes_file = dataset / (
            f"bounding_box_2d_tight_{frame_id}.npy"
        )
        labels_file = dataset / (
            f"bounding_box_2d_tight_labels_{frame_id}.json"
        )

        if not boxes_file.exists() or not labels_file.exists():
            raise FileNotFoundError(
                f"Missing annotations for {image.name}"
            )

        boxes = np.load(boxes_file, allow_pickle=False)

        with open(labels_file) as f:
            labels = json.load(f)

        objects = []

        for row in boxes:
            semantic_id = str(int(row[0]))

            if semantic_id not in labels:
                raise ValueError(
                    f"Unknown semantic ID {semantic_id} "
                    f"in {boxes_file.name}"
                )

            label = labels[semantic_id]["class"]

            box = [float(row[i]) for i in range(1, 5)]

            if box[2] <= box[0] or box[3] <= box[1]:
                continue

            objects.append({
                "class": str(label),
                "box": box
            })

        truth[image.name] = objects

    return truth


def run_yolo(dataset, model_path, confidence=0.25, imgsz=640):
    """Run YOLO on every Isaac Sim RGB image."""
    from ultralytics import YOLO

    dataset = Path(dataset)
    images = sorted(dataset.glob("rgb_*.png"))

    model = YOLO(str(model_path), task="detect")

    predictions = {}

    for index, image in enumerate(images, start=1):
        result = model.predict(
            source=str(image),
            conf=confidence,
            imgsz=imgsz,
            verbose=False
        )[0]

        detections = []

        for box in result.boxes:
            class_id = int(box.cls.item())

            detections.append({
                "class": str(result.names[class_id]),
                "box": [
                    float(v) for v in box.xyxy[0].tolist()
                ],
                "confidence": float(box.conf.item())
            })

        predictions[image.name] = detections

        if index % 50 == 0 or index == len(images):
            print(f"Processed {index}/{len(images)} frames")

    return predictions


def evaluate(
    truth,
    predictions,
    threshold=0.75,
    max_distance=None
):
    tp = fp = fn = 0

    for frame in sorted(set(truth) | set(predictions)):
        gt = truth.get(frame, [])

        if max_distance is not None:
            gt = [
                x for x in gt
                if x.get("distance_m") is not None
                and x["distance_m"] <= max_distance
            ]

        pred = predictions.get(frame, [])

        # Greedy one-to-one matching, highest IoU first.
        # Class labels must match exactly.
        candidates = sorted(
            (
                (iou(g["box"], p["box"]), gi, pi)
                for gi, g in enumerate(gt)
                for pi, p in enumerate(pred)
                if str(g["class"]) == str(p["class"])
            ),
            reverse=True
        )

        used_g = set()
        used_p = set()

        for score, gi, pi in candidates:
            if score < threshold:
                break

            if gi not in used_g and pi not in used_p:
                used_g.add(gi)
                used_p.add(pi)

        tp += len(used_g)
        fn += len(gt) - len(used_g)
        fp += len(pred) - len(used_p)

    recall = tp / (tp + fn) if tp + fn else None
    fdr = fp / (tp + fp) if tp + fp else None

    return tp, fp, fn, recall, fdr


def main():
    parser = argparse.ArgumentParser()

    parser.add_argument(
        "--dataset",
        help="Isaac Sim Replicator dataset directory"
    )
    parser.add_argument(
        "--model",
        help="YOLO model (.pt or .engine)"
    )
    parser.add_argument(
        "--ground-truth",
        help="Original ground-truth JSON"
    )
    parser.add_argument(
        "--predictions",
        help="Original predictions JSON"
    )
    parser.add_argument(
        "--iou",
        type=float,
        default=0.75
    )
    parser.add_argument(
        "--conf",
        type=float,
        default=0.25
    )
    parser.add_argument(
        "--imgsz",
        type=int,
        default=640
    )
    parser.add_argument(
        "--max-distance",
        type=float,
        default=None,
        help="Filter ground truth by distance in meters"
    )
    parser.add_argument(
        "--output",
        help="Optional path to save predictions JSON"
    )

    args = parser.parse_args()

    if args.dataset:
        if not args.model:
            parser.error("--dataset requires --model")

        if args.max_distance is not None:
            parser.error(
                "Native Replicator 2D annotations do not "
                "contain distance_m; omit --max-distance"
            )

        print("Loading Isaac Sim ground truth...")
        truth = load_isaac_ground_truth(args.dataset)

        print(f"Loaded {len(truth)} frames")
        print(
            "Ground-truth objects:",
            sum(len(v) for v in truth.values())
        )

        print(f"Running YOLO model: {args.model}")
        predictions = run_yolo(
            args.dataset,
            args.model,
            args.conf,
            args.imgsz
        )

    else:
        if not args.ground_truth or not args.predictions:
            parser.error(
                "Provide --dataset and --model, or "
                "--ground-truth and --predictions"
            )

        with open(args.ground_truth) as f:
            truth = json.load(f)

        with open(args.predictions) as f:
            predictions = json.load(f)

    if args.output:
        output = Path(args.output)
        output.parent.mkdir(parents=True, exist_ok=True)

        with open(output, "w") as f:
            json.dump(predictions, f, indent=2)

        print(f"Saved predictions to {output}")

    tp, fp, fn, recall, fdr = evaluate(
        truth,
        predictions,
        args.iou,
        args.max_distance
    )

    print("\n===== ACCURACY BENCHMARK =====")
    print(f"IoU threshold: {args.iou}")
    print(f"TP={tp}, FP={fp}, FN={fn}")

    print(
        f"Recall: {recall:.2%}"
        if recall is not None else "Recall: N/A"
    )
    print(
        f"FDR: {fdr:.2%}"
        if fdr is not None else "FDR: N/A"
    )

    if recall is not None and fdr is not None:
        print(
            "PASS"
            if recall >= 0.95 and fdr <= 0.05
            else "FAIL"
        )

    print(
        "NOTE: Results use all annotated objects. "
        "No distance filtering is applied by default."
    )


if __name__ == "__main__":
    main()
