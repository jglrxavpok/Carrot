# Actions: Input & Output system

[Heavily inspired by OpenXR Action system](https://www.khronos.org/registry/OpenXR/specs/1.0/html/xrspec.html#input)

Actions are meant to be declared in advance. An ActionSet is not modifiable once its first poll happened (next frame after its creation).

## ActionSet
An `ActionSet` contains `Action`s which can be enabled/disabled as a group.

This allows the system to easily switch by a menu and a gameplay control system.

Active `ActionSet`s are polled each frame to query the state of each action. 

## Action
An `Action` represents a capability of the input/output system. For example, it can be a controller button, mouse moves, or haptic feedback.

Each `Action` has a name to represent its usage.

To query the state of an action, one can use `Action::getState` when available. The result will depend on the action type.

To set the state of an action, one can use `Action::setState` when available.

## Binding
A `Binding` is a way to link an action with the physical device. Because the user (and the OpenXR runtime if VR is enabled) can remap
the controls, bindings provided to the engine as only suggestions.

An action can be bound to multiple physical inputs, the engine will resolve the final value as such (same as OpenXR in the time of writing this):
* Bool: `OR` result of all inputs
* Float: Result with the largest absolute value
* Vec2: Result with the longest length

## Resolving type mismatches
Depending on the physical device, the OpenXR runtime (when running in VR) and suggested bindings, an `Action` of type `FloatInput` could be bound to a physical input which is only a boolean input (a button), and other mismatches can arise.

In the case of a mismatch, the following rules apply:
* If the action is a boolean input, but the physical input is scalar, the action is considered pressed if the scalar value is > `threshold`. `threshold` is hard-coded to be 0.5 at the moment.
* If the action is a float input, but the physical input is a button, the action will get the value 1.0 if the button is pressed, 0.0 otherwise.
* Vec2 cannot be applied to something else than the mouse, a d-pad or a joystick.