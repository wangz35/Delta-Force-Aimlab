# Aim // Range

A first-person aim-training prototype built in C++ with Unreal Engine 5.8. It uses no assets, maps, or characters from third-party games; its training cadence and feedback are inspired by modern tactical shooters.

## Features

- WASD movement, mouse aiming, automatic fire at 1x, and scoped single-fire behavior at 3.5x and 7.25x.
- Independent sensitivity settings for 1x, 3.5x, and 7.25x scopes.
- Smooth Q/E leaning with camera roll, lateral movement, and aim offset; sprinting, jumping, sliding, and slide-jumping momentum.
- Configurable recoil pattern, scoped recoil recovery, and a dynamic green crosshair.
- A persistent close-range tracking sphere with hover color feedback, plus 10m, 20m, 50m, and 100m headshot-only humanoid targets.
- A 60-second drill with hit count, shot count, accuracy, and reaction-time feedback.
- Procedural 100m range, blue lighting, and rear cover for peek-shot practice.

## Launch

1. Open `AimTracker.uproject` with Unreal Engine 5.8 and generate project files if prompted.
2. Build the `AimTrackerEditor` target on the first launch.
3. Click Play in the editor to start the drill.

Tested engine path: `D:\UE_5.8`.