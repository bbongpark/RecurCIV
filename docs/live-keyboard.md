# RecurCIV live keyboard

The keyboard page offers Compose & Send, Live Keyboard and the `.txt` browser.
All outgoing text uses US QWERTY; configure the target device accordingly.
Saved French-layout and theme settings are ignored. The Switch system keyboard's
own language is not changed. US text preflight accepts printable ASCII, line
breaks and tabs; other characters, including accented letters, are rejected.

## Layout

Live Keyboard is a five-row, centered US QWERTY layout at 1280x720. It keeps all
47 printable keys and their shifted symbols, plus dedicated Shift, a wide Space,
Backspace and Enter keys. Row starts are staggered like a physical keyboard, and
every key has a visible label and a non-overlapping touch rectangle.

The selected key keeps its dark navy/slate fill and white label and is marked by
a Civ-gold border only. A pressed key briefly receives a subtle gold-tinted fill
with a gold border; the label stays white. There is no solid gold selection fill
and no cyan selection outline.

## Controls

| Control | Action |
| --- | --- |
| Touch | Activate the key under the finger |
| D-pad / left stick | Move the gold selection border |
| A | Activate the selected key |
| B | Backspace |
| X | One-shot Shift; double-tap for CAPS |
| Y | Space |
| + | Done; return to the USB Keyboard menu |
| ZL | Unassigned |

Horizontal movement stays within the current row. Vertical movement chooses the
nearest key in the next staggered row. The left stick has a 12000 deadzone and
uses only its dominant axis. Held directions move once immediately, pause for
350 ms, then repeat every 100 ms. Action buttons are edge-triggered, so holding
A does not repeat characters. Touch and controller input share the same
selection and stroke queue.

## Shift and CAPS

A single Shift tap activates one-shot Shift immediately: letter labels and the
next typed letter become uppercase, and shifted symbols are used for the next
printable character. Enter and Backspace do not consume Shift.

A second Shift tap within 350 ms activates CAPS. The Shift key label changes to
`CAPS`, receives a stronger gold border and a dark-gold tint, and letters remain
uppercase until Shift is tapped again. CAPS affects letters only; digits and
punctuation remain unshifted. Touching Shift and pressing X use the same state
machine, so mixed touch/controller double-taps work. An intervening character
cancels a pending double-tap. Each new Live session starts in normal case.

Live Keyboard applies the existing HID Shift modifier for uppercase letters; it
never sends a physical Caps Lock toggle. Displayed labels and transmitted
characters are produced by the same mapping.

## Queue and exit

The queue holds 32 strokes; if it fills, additional taps are dropped and a
warning appears. The application waits for each transfer to finish before
submitting the next press or release, with a minimum 80 ms key-down and 80 ms
all-keys-up interval.

`+` is handled inside Live Keyboard before the global exit shortcut. Done stops
new input, waits for an in-flight press minimum, requests a neutral report when
possible, and returns to the USB Keyboard menu without exiting RecurCIV or
resetting USB. The same `+` press cannot also trigger the global application
exit. Outside Live Keyboard, `+` still exits normally.

On disconnect RecurCIV discards pending taps and synchronizes with a neutral
report before accepting fresh input after reconnect. A transfer failure stops
live input until the keyboard is closed and reopened. USB disconnection or a
failed transfer can prevent neutral delivery, and USB transfer completion does
not prove the target application processed a key. Check the target after use.
