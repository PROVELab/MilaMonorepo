"use client";
import * as THREE from "three";
import { useLoader } from "@react-three/fiber";
import { useMemo } from "react";
import { PLYLoader } from "three-stdlib";

const CAR_BOUNDING_BOX = new THREE.Vector3(5.4, 2.1, 2.0);
const car_max_dimension = Math.max(CAR_BOUNDING_BOX.x, CAR_BOUNDING_BOX.y, CAR_BOUNDING_BOX.z);
export const PointCloud = ({ scale }: { scale: number }) => {
  const geometry = useLoader(PLYLoader, "model/sample_points.ply");
  // const hasVertexColor = geometry.hasAttribute("color");

  const scaledGeometry = useMemo(() => {
    const clone = geometry.clone();

    clone.translate(0, 0, 2.5);

    clone.computeBoundingBox();
    clone.rotateX(-Math.PI / 2);
    const bb = clone.boundingBox;
    if (!bb) {
      return geometry;
    }

    const width = bb.max.x - bb.min.x;
    const height = bb.max.y - bb.min.y;
    const depth = bb.max.z - bb.min.z;
    const maxDimension = Math.max(width, height, depth);

    if (maxDimension > 0) {
      const scaleFactor = (car_max_dimension * 4.5) / maxDimension;
      clone.scale(scaleFactor, scaleFactor, scaleFactor);
    }

    return clone;
  }, [geometry]);

  return (
    <points scale={scale}>
      <primitive object={scaledGeometry} attach="geometry" />
      <pointsMaterial size={0.1} vertexColors={false} color={"cyan"} />
    </points>
  );
};
