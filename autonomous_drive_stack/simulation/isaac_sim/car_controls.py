
import omni.usd
import omni.ui as ui
import omni.appwindow
import carb.input
import builtins
import math
from pxr import UsdGeom, Gf

# =========================
# SETTINGS
# =========================

CAR_PATH = "/Root/Car"
MOVE_SPEED = 0.5
TURN_SPEED = 10.0

# =========================
# CLEAN UP OLD CONTROLS
# =========================

old_window = getattr(builtins, "car_control_window", None)

if old_window is not None:
    old_window.visible = False

input_iface = carb.input.acquire_input_interface()
keyboard = omni.appwindow.get_default_app_window().get_keyboard()

old_sub = getattr(builtins, "car_keyboard_sub", None)

if old_sub is not None:
    input_iface.unsubscribe_to_keyboard_events(keyboard, old_sub)
    builtins.car_keyboard_sub = None

# =========================
# FIND CAR
# =========================

stage = omni.usd.get_context().get_stage()
car = stage.GetPrimAtPath(CAR_PATH)

if not car.IsValid():
    print("ERROR: Car not found at", CAR_PATH)

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

        # =========================
        # ORIGINAL WORKING MOVEMENT
        # =========================

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

            translate_op.Set(Gf.Vec3d(x, y, z))

            half = yaw / 2

            new_rot = Gf.Quatf(
                math.cos(half),
                Gf.Vec3f(0, 0, math.sin(half))
            )

            orient_op.Set(new_rot)

            print("CAR:", direction, "POSITION:", x, y, z)

        # =========================
        # GUI WINDOW
        # =========================

        window = ui.Window(
            "Car Controls",
            width=300,
            height=260
        )

        with window.frame:
            with ui.VStack(spacing=10):

                ui.Label("CAR CONTROLS", height=30)

                ui.Label("I = Forward | K = Backward")
                ui.Label("J = Left | L = Right")

                ui.Button(
                    "FORWARD (I)",
                    height=40,
                    clicked_fn=lambda: move_car("forward")
                )

                with ui.HStack(spacing=10):

                    ui.Button(
                        "LEFT (J)",
                        height=40,
                        clicked_fn=lambda: move_car("left")
                    )

                    ui.Button(
                        "RIGHT (L)",
                        height=40,
                        clicked_fn=lambda: move_car("right")
                    )

                ui.Button(
                    "BACKWARD (K)",
                    height=40,
                    clicked_fn=lambda: move_car("backward")
                )

        # =========================
        # KEYBOARD CONTROLS
        # =========================

        key_actions = {
            carb.input.KeyboardInput.I: "forward",
            carb.input.KeyboardInput.K: "backward",
            carb.input.KeyboardInput.J: "left",
            carb.input.KeyboardInput.L: "right",
        }

        def on_keyboard(event, *args):
            if event.type in (
                carb.input.KeyboardEventType.KEY_PRESS,
                carb.input.KeyboardEventType.KEY_REPEAT
            ):
                direction = key_actions.get(event.input)

                if direction:
                    move_car(direction)

            return True

        sub = input_iface.subscribe_to_keyboard_events(
            keyboard,
            on_keyboard
        )

        # =========================
        # KEEP CONTROLS ALIVE
        # =========================

        builtins.car_control_window = window
        builtins.car_control_move = move_car
        builtins.car_keyboard_sub = sub
        builtins.car_keyboard_callback = on_keyboard

        print("CAR CONTROLS READY!")
        print("I = Forward")
        print("K = Backward")
        print("J = Left")
        print("L = Right")



