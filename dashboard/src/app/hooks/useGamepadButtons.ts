"use client";

import { useEffect, useRef } from "react";

type GamepadButtonBinding = {
  button: number;
};

export function useGamepadButtons<T extends GamepadButtonBinding>(
  bindings: readonly T[],
  onPress: (binding: T) => void,
  enabled = true,
) {
  const onPressRef = useRef(onPress);

  useEffect(() => {
    onPressRef.current = onPress;
  }, [onPress]);

  useEffect(() => {
    if (!enabled) return;

    let animationFrameId = 0;
    const pressedButtons = new Set<number>();

    const poll = () => {
      const gamepad = Array.from(navigator.getGamepads()).find(
        (candidate): candidate is Gamepad => candidate !== null,
      );

      if (!gamepad) {
        pressedButtons.clear();
      } else {
        for (const binding of bindings) {
          const isPressed = gamepad.buttons[binding.button]?.pressed ?? false;
          const wasPressed = pressedButtons.has(binding.button);

          if (isPressed && !wasPressed) {
            onPressRef.current(binding);
          }

          if (isPressed) {
            pressedButtons.add(binding.button);
          } else {
            pressedButtons.delete(binding.button);
          }
        }
      }

      animationFrameId = window.requestAnimationFrame(poll);
    };

    animationFrameId = window.requestAnimationFrame(poll);
    return () => window.cancelAnimationFrame(animationFrameId);
  }, [bindings, enabled]);
}
