"use client";

import { Canvas, useFrame } from "@react-three/fiber";
import { Environment, OrbitControls, useGLTF } from "@react-three/drei";
import { useEffect, useRef } from "react";
import * as THREE from "three";
import { OrbitControls as OrbitControlsImpl } from "three-stdlib";
import type { DriveMode } from "../types/telemetry";

const MODEL_PATH = "/mitsubishi_lancer_evolution.glb";

interface VehicleSceneProps {
  speed: number;
  driveMode: DriveMode;
}

function VehicleModel({ speed, driveMode }: VehicleSceneProps) {
  const { scene } = useGLTF(MODEL_PATH);
  const rearWheelsRef = useRef<THREE.Object3D[]>([]);
  const frontWheelsRef = useRef<THREE.Object3D[]>([]);
  const steeringPivotsRef = useRef<THREE.Object3D[]>([]);
  const smoothSpeed = useRef(0);

  useEffect(() => {
    rearWheelsRef.current = [];
    frontWheelsRef.current = [];
    steeringPivotsRef.current = [];

    scene.traverse((child: any) => {
      if (child.isMesh) {
        child.castShadow = true;
        child.receiveShadow = true;
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

    scene.rotation.y = THREE.MathUtils.degToRad((smoothSpeed.current / 40) * 6);
    scene.rotation.z = THREE.MathUtils.degToRad(driveMode === "R" ? 1.2 : 0);
    scene.position.y = -0.35;
  });

  return <primitive object={scene} dispose={null} />;
}

export function VehicleScene({ speed, driveMode }: VehicleSceneProps) {
  const controlsRef = useRef<OrbitControlsImpl | null>(null);

  return (
    <div className="vehicle-scene">
      <Canvas
        shadows
        camera={{ position: [6, 2.8, 9.6], fov: 45 }}
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
          <ringGeometry args={[0.2, 18, 64]} />
          <meshStandardMaterial color="#1a1a1a" metalness={0.3} roughness={0.4} />
        </mesh>

        <OrbitControls
          ref={controlsRef}
          enablePan={false}
          enableZoom={false}
          enableDamping
          dampingFactor={0.08}
          maxPolarAngle={(65 * Math.PI) / 180}
          minPolarAngle={(55 * Math.PI) / 180}
          target={[0, 0.4, 0]}
        />
      </Canvas>
    </div>
  );
}

useGLTF.preload(MODEL_PATH);
