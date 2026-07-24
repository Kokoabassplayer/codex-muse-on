# Use manual pedal profile selection

The first release persists a manual Control Profile selection, defaulting to Controller Only with Pedal Enabled as an option. The pedal is only bit `0x10` in the Muse-On joystick report and has no independent HID service or presence signal, so idle and unplugged states are indistinguishable; automatic switching would be unreliable. Profile changes release holds, make Control Inactive, reset input state, and revalidate before dispatch resumes.
