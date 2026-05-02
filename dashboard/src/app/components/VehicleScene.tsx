"use client";

import { Canvas, useFrame } from "@react-three/fiber";
import { Environment, OrbitControls, useGLTF } from "@react-three/drei";
import { useEffect, useMemo, useRef } from "react";
import * as THREE from "three";
import { OrbitControls as OrbitControlsImpl } from "three-stdlib";
import type { DriveMode } from "../types/telemetry";

const MODEL_PATH = "/octane.glb";
const TARGET_MODEL_BOUNDS = new THREE.Vector3(5.4, 2.1, 3.0);
const GROUND_Y = -0.36;

interface VehicleSceneProps {
  speed: number;
  driveMode: DriveMode;
}

function VehicleModel({ speed, driveMode }: VehicleSceneProps) {
  const gltf = useGLTF(MODEL_PATH);
  const scene = useMemo(() => gltf.scene.clone(true), [gltf.scene]);
  const rearWheelsRef = useRef<THREE.Object3D[]>([]);
  const frontWheelsRef = useRef<THREE.Object3D[]>([]);
  const steeringPivotsRef = useRef<THREE.Object3D[]>([]);
  const basePositionRef = useRef(new THREE.Vector3(0, GROUND_Y, 0));
  const smoothSpeed = useRef(0);

  useEffect(() => {
    rearWheelsRef.current = [];
    frontWheelsRef.current = [];
    steeringPivotsRef.current = [];

    // Reset root-level transforms so normalization always starts from source model space.
    scene.position.set(0, 0, 0);
    scene.rotation.set(0, 0, 0);
    scene.scale.set(1, 1, 1);
    scene.updateMatrixWorld(true);

    scene.traverse((child: any) => {
      if (child.isMesh) {
        child.castShadow = true;
        child.receiveShadow = true;
        child.frustumCulled = false;
      }

      if (child.name?.includes("SteeringPivot")) {
        steeringPivotsRef.current.push(child);
        if (child.children[0]) {
          frontWheelsRef.current.push(child.children[0]);
        }
      }

      if (/w[34]/i.test(child.name)) {
        rearWheelsRef.current.push(child);
      }
    });

    // Normalize unknown model origins/scales so different GLBs stay framed.
    const rawBounds = new THREE.Box3().setFromObject(scene);
    if (!rawBounds.isEmpty()) {
      const rawSize = rawBounds.getSize(new THREE.Vector3());
      const epsilon = 0.001;
      const scale = Math.min(
        TARGET_MODEL_BOUNDS.x / Math.max(rawSize.x, epsilon),
        TARGET_MODEL_BOUNDS.y / Math.max(rawSize.y, epsilon),
        TARGET_MODEL_BOUNDS.z / Math.max(rawSize.z, epsilon),
      );
      scene.scale.setScalar(scale);
      scene.updateMatrixWorld(true);

      const scaledBounds = new THREE.Box3().setFromObject(scene);
      const center = scaledBounds.getCenter(new THREE.Vector3());
      const minY = scaledBounds.min.y;
      scene.position.set(scene.position.x - center.x, scene.position.y + (GROUND_Y - minY), scene.position.z - center.z);
      basePositionRef.current.copy(scene.position);
    }
  }, [scene]);

  useFrame((state, delta) => {
    smoothSpeed.current += (speed - smoothSpeed.current) * Math.min(1, delta * 4);
    const speedMS = smoothSpeed.current * 0.44704;
    const wheelRadius = 0.3;
    const spinDirection = smoothSpeed.current >= 0 ? -1 : 1;
    const spinSpeed = (speedMS / wheelRadius) * delta * spinDirection;

    const steeringAngle = THREE.MathUtils.degToRad(Math.sin(state.clock.elapsedTime * 0.5) * 4);
    steeringPivotsRef.current.forEach(pivot => (pivot.rotation.y = steeringAngle));
    frontWheelsRef.current.forEach(wheel => (wheel.rotation.x += spinSpeed));
    rearWheelsRef.current.forEach(wheel => (wheel.rotation.x += spinSpeed));

    scene.position.copy(basePositionRef.current);
    scene.rotation.y = THREE.MathUtils.degToRad((smoothSpeed.current / 40) * 6);
    scene.rotation.z = THREE.MathUtils.degToRad(driveMode === "Reverse" ? 1.2 : 0);
  });

  return <primitive object={scene} dispose={null} />;
}

export function VehicleScene({ speed, driveMode }: VehicleSceneProps) {
  const controlsRef = useRef<OrbitControlsImpl | null>(null);

  return (
    <div className="vehicle-scene">
      <Canvas
        shadows
        camera={{ position: [8.2, 3.8, 12.2], fov: 45 }}
        gl={{ toneMapping: THREE.ACESFilmicToneMapping, outputColorSpace: THREE.SRGBColorSpace }}
      >
        <color attach="background" args={["#0b111c"]} />
        <fog attach="fog" args={["#0b111c", 18, 65]} />
        <ambientLight intensity={0.9} />
        <directionalLight
          position={[6, 10, 4]}
          castShadow
          intensity={2.4}
          shadow-mapSize-width={2048}
          shadow-mapSize-height={2048}
        />
        <spotLight position={[-8, 6, 2]} angle={0.55} penumbra={0.5} intensity={1.4} color="#9dc7ff" />
        <Environment preset="sunset" />

        <VehicleModel speed={speed} driveMode={driveMode} />

        <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, -0.36, 0]} receiveShadow>
          <ringGeometry args={[0.25, 9.5, 64]} />
          <meshStandardMaterial color="#1a1a1a" metalness={0.3} roughness={0.4} />
        </mesh>

        <OrbitControls
          ref={controlsRef}
          enablePan={false}
          enableZoom={false}
          enableDamping
          dampingFactor={0.08}
          maxPolarAngle={(72 * Math.PI) / 180}
          minPolarAngle={(48 * Math.PI) / 180}
          target={[0, 0.7, 0]}
        />
      </Canvas>
    </div>
  );
}

useGLTF.preload(MODEL_PATH);
