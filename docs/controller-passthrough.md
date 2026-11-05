# Controller monitor and passthrough

The USB Controller screen reads local Switch 1 inputs through libnx PadState.
It forwards A, B, X, Y, L, R, ZL, ZR, Plus, Minus and stick clicks as the
PicoSwitch button bits; the D-pad uses the HID eight-direction hat. Horizontal
directions and diagonals use the opposite numbering from PicoSwitch's local
constants because the previous numbering reversed left and right on Switch 2.
The left and right sticks map to the existing 8-bit X/Y/Z/Rz fields, with Y
inverted, saturation, and a small center deadzone. Analog trigger pressure,
motion, rumble, NFC, Home and Capture are not forwarded. Axis orientation,
calibration, and mappings other than A require physical verification.

On entry or after disconnect, release all buttons while USB is ready to arm
passthrough. History shows local button-state changes, not USB delivery. Plus
and Minus are forwarded during a combined hold; after two seconds the screen
exits and requests neutral. Touch Back and Diagnostics also request neutral.
The existing gamepad API does not confirm delivery of the neutral report; an
in-flight transfer, disconnection, or a later output report can prevent or
supersede it. Avoid relying on this monitor for safety-critical control.
