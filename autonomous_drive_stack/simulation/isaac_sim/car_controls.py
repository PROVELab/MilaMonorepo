import omni.usd
import omni.ui as ui
import builtins
import math
from pxr import UsdGeom, Gf

CAR_PATH = "/Root/Sensor_Placement_Model__2_"
MOVE_SPEED = 0.5
TURN_SPEED = 10.0

# Close previous control window
old_window = getattr(builtins, "car_control_window", None)
if old_window is not None:
    old_window.visible = False

stage = omni.usd.get_context().get_stage()
car = stage.GetPrimAtPath(CAR_PATH)

if not car.IsValid():
    print("ERROR: Car not found!")
else:
    xform = UsdGeom.Xformable(car)

    translate_op = None
    orient_op = None

    for op in xform.GetOrderedXformOps():
        if op.GetOpName() == "xformOp:translate":
            translate_op = op
        elif op.GetOpName() == "xformOp:orient":
            orient_op = op

    if translate_op is None or orient_op is None:
        print("ERROR: Car transform operations not found!")
    else:

        def move_car(direction):
            pos = translate_op.Get()
            rot = orient_op.Get()

            x, y, z = pos

            q = rot.GetImaginary()
            w = rot.GetReal()

            yaw = math.atan2(
                2 * (w * q[2] + q[0] * q[1]),
                1 - 2 * (q[1]**2 + q[2]**2)
            )

            # Forward and backward are now reversed
            if direction == "forward":
                x -= MOVE_SPEED * math.cos(yaw)
                y -= MOVE_SPEED * math.sin(yaw)

            elif direction == "backward":
                x += MOVE_SPEED * math.cos(yaw)
                y += MOVE_SPEED * math.sin(yaw)

            elif direction == "left":
                yaw += math.radians(TURN_SPEED)

            elif direction == "right":
                yaw -= math.radians(TURN_SPEED)

            # Update car position
            translate_op.Set(Gf.Vec3d(x, y, z))

            # Update car rotation
            half = yaw / 2
            new_rot = Gf.Quatf(
                math.cos(half),
                Gf.Vec3f(0, 0, math.sin(half))
            )
            orient_op.Set(new_rot)

            print("CAR:", direction, "POSITION:", x, y, z)

        # Create control window
        window = ui.Window(
            "Car Controls",
            width=280,
            height=220
        )

        with window.frame:
            with ui.VStack(spacing=10):
                ui.Label("CAR CONTROLS", height=30)

                ui.Button(
                    "FORWARD",
                    height=40,
                    clicked_fn=lambda: move_car("forward")
                )

                with ui.HStack(spacing=10):
                    ui.Button(
                        "LEFT",
                        height=40,
                        clicked_fn=lambda: move_car("left")
                    )

                    ui.Button(
                        "RIGHT",
                        height=40,
                        clicked_fn=lambda: move_car("right")
                    )

                ui.Button(
                    "BACKWARD",
                    height=40,
                    clicked_fn=lambda: move_car("backward")
                )

        # Keep window and callback alive
        builtins.car_control_window = window
        builtins.car_control_move = move_car

        print("CAR CONTROL WINDOW READY!")
        print("Forward / Backward corrected")
        print("Left / Right unchanged")

