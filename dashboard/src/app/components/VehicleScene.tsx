"use client";

import { Canvas, useFrame, useThree } from "@react-three/fiber";
import { Environment, OrbitControls, useGLTF } from "@react-three/drei";
import { useEffect, useMemo, useRef } from "react";
import * as THREE from "three";
import { OrbitControls as OrbitControlsImpl } from "three-stdlib";
import type { DriveMode } from "../types/telemetry";

const MODEL_PATH = "/octane.glb";
const TARGET_MODEL_BOUNDS = new THREE.Vector3(5.4, 2.1, 3.0);
const GROUND_Y = -0.36;

const CAMERA_POS_DRIVE = new THREE.Vector3(-12.2, 3.8, 0);
const CAMERA_POS_REVERSE = new THREE.Vector3(12.2, 3.8, 0);

interface VehicleSceneProps {
  rpm: number | null;
  driveMode: DriveMode;
}

function CameraController({ controlsRef, driveMode }: { controlsRef: React.MutableRefObject<OrbitControlsImpl | null>; driveMode: DriveMode }) {
  const { camera, gl } = useThree();
  const interacting = useRef(false);
  const lastInteractionTime = useRef(Date.now());
  const shouldAutoRecenter =
    driveMode === "Drive" || driveMode === "Cruise Control" || driveMode === "Reverse";
  const targetPos = driveMode === "Reverse" ? CAMERA_POS_REVERSE : CAMERA_POS_DRIVE;

  useEffect(() => {
    const controls = controlsRef.current;
    if (!controls) return;

    const onStart = () => { interacting.current = true; };
    const onEnd = () => { interacting.current = false; lastInteractionTime.current = Date.now(); };
    
    controls.addEventListener('start', onStart);
    controls.addEventListener('end', onEnd);

    // Fail-safe: immediately halt camera lerp the moment a pointer hits the canvas
    const onPointerDown = () => { interacting.current = true; };
    const onPointerUp = () => { interacting.current = false; lastInteractionTime.current = Date.now(); };

    const canvas = gl.domElement;
    canvas.addEventListener('pointerdown', onPointerDown);
    window.addEventListener('pointerup', onPointerUp);

    return () => {
      controls.removeEventListener('start', onStart);
      controls.removeEventListener('end', onEnd);
      canvas.removeEventListener('pointerdown', onPointerDown);
      window.removeEventListener('pointerup', onPointerUp);
    };
  }, [controlsRef, gl]);

  useEffect(() => {
    if (!shouldAutoRecenter) return;
    camera.position.copy(targetPos);
    if (controlsRef.current) controlsRef.current.update();
    interacting.current = false;
    lastInteractionTime.current = Date.now();
  }, [camera, controlsRef, shouldAutoRecenter, targetPos]);

  useFrame((_, delta) => {
    if (interacting.current) return;
    if (Date.now() - lastInteractionTime.current < 5000) return;
    if (!shouldAutoRecenter) return;

    camera.position.lerp(targetPos, delta * 2.5);
    if (controlsRef.current) controlsRef.current.update();
  });

  return null;
}

function VehicleModel({ rpm, driveMode }: VehicleSceneProps) {
  const gltf = useGLTF(MODEL_PATH);
  const scene = useMemo(() => gltf.scene.clone(true), [gltf.scene]);
  const rearWheelsRef = useRef<THREE.Object3D[]>([]);
  const frontWheelsRef = useRef<THREE.Object3D[]>([]);
  const steeringPivotsRef = useRef<THREE.Object3D[]>([]);
  const basePositionRef = useRef(new THREE.Vector3(0, GROUND_Y, 0));
  const smoothRpm = useRef(0);

  useEffect(() => {
    rearWheelsRef.current = [];
    frontWheelsRef.current = [];
    steeringPivotsRef.current = [];

    // Reset root-level transforms so normalization always starts from source model space.
    scene.position.set(0, 0, 0);
    scene.rotation.set(0, 0, 0);
    scene.scale.set(1, 1, 1);
    scene.updateMatrixWorld(true);

    scene.traverse((child: THREE.Object3D) => {
      if (child instanceof THREE.Mesh) {
        child.castShadow = true;
        child.receiveShadow = true;
        child.frustumCulled = false;
      }

      if (child.name.includes("SteeringPivot")) {
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
    const targetRpm = rpm ?? 0;
    smoothRpm.current += (targetRpm - smoothRpm.current) * Math.min(1, delta * 4);
    const wheelRadius = 0.3;
    const wheelCircumference = 2 * Math.PI * wheelRadius;
    const wheelSurfaceMetersPerSecond = (smoothRpm.current / 60) * wheelCircumference;
    const spinDirection = smoothRpm.current >= 0 ? -1 : 1;
    const wheelSpinRadians = (wheelSurfaceMetersPerSecond / wheelRadius) * delta * spinDirection;

    const steeringAngle = THREE.MathUtils.degToRad(Math.sin(state.clock.elapsedTime * 0.5) * 4);
    steeringPivotsRef.current.forEach(pivot => (pivot.rotation.y = steeringAngle));
    frontWheelsRef.current.forEach(wheel => (wheel.rotation.x += wheelSpinRadians));
    rearWheelsRef.current.forEach(wheel => (wheel.rotation.x += wheelSpinRadians));

    scene.position.copy(basePositionRef.current);
    scene.rotation.y = THREE.MathUtils.degToRad((smoothRpm.current / 3000) * 6);
    scene.rotation.z = THREE.MathUtils.degToRad(driveMode === "Reverse" ? 1.2 : 0);
  });

  return <primitive object={scene} dispose={null} />;
}

export function VehicleScene({ rpm, driveMode }: VehicleSceneProps) {
  const controlsRef = useRef<OrbitControlsImpl | null>(null);

  return (
    <div className="vehicle-scene" style={{ touchAction: 'none', userSelect: 'none', WebkitTouchCallout: 'none' }}>
      <Canvas
        onContextMenu={(e) => e.preventDefault()}
        style={{ touchAction: 'none', userSelect: 'none', WebkitTouchCallout: 'none' }}
        shadows
        camera={{ position: [-12.2, 3.8, 0], fov: 45 }}
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

        <VehicleModel rpm={rpm} driveMode={driveMode} />
        <CameraController controlsRef={controlsRef} driveMode={driveMode} />

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
