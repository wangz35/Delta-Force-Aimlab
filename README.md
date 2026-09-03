# Aim // Range

A first-person aim-training prototype built in C++ with Unreal Engine 5.8. It uses no assets, maps, or characters from third-party games; its training cadence and feedback are inspired by modern tactical shooters.

## Features

- WASD movement, mouse aiming, automatic fire at 1x, and scoped single-fire behavior at 3.5x and 7.25x.
- Independent sensitivity settings for 1x, 3.5x, and 7.25x scopes.
- Smooth Q/E leaning with camera roll, lateral movement, and aim offset; Caps Lock slow-walk toggle, sprinting, jumping, sliding, and slide-jumping momentum.
- Configurable recoil pattern, scoped recoil recovery, and a minimal red-dot crosshair.
- Static and moving 10m, 20m, 50m, and 100m targets, jump bots, mixed peeks, cover peeks, target switching, and reactive tracking drills.
- 60-second scoring for hits, accuracy, reaction time, bot eliminations, or continuous time on target, depending on the drill.
- Procedural 100m range, blue lighting, and rear cover for peek-shot practice.

## Training modes

Press `1` through `7` on the number row or numpad to switch drills.

1. **Standard Range** — warm up on the persistent close target, then confirm precision on static targets at 10m, 20m, 50m, and 100m.
2. **Moving Distance Targets** — track full-body targets moving at the same world speed across four distances; hits respawn the target.
3. **Jump Bots** — acquire and hold jumping targets to practice jump-pull tracking.
4. **Mixed Peek Bots + Ball** — combine a horizontal reference target with bots that jump or strafe into view.
5. **Random Cover Peeks** — practice left/right cover acquisition against mixed jump and strafe peeks.
6. **Target Switching** — eliminate four small targets distributed across different distances and screen positions; each hit relocates a target.
7. **Reactive Tracking** — keep the crosshair on one fast horizontal target through unpredictable direction changes; score is continuous time on target.

## Suggested 15-minute routine

- 2 minutes in Mode 1 for calibration and relaxed precision.
- 3 minutes in Mode 7 for smooth continuous tracking.
- 3 minutes in Mode 2 for distance-dependent target reading.
- 2 minutes in Mode 6 for target acquisition and micro-corrections.
- 2 minutes in Mode 3 for jump tracking.
- 3 minutes split between Modes 4 and 5 for practical peek tracking.

Prioritize a stable crosshair path over raw speed. Increase speed or difficulty only after accuracy and time-on-target scores remain consistent across several sessions.

## Launch

1. Open `AimTracker.uproject` with Unreal Engine 5.8 and generate project files if prompted.
2. Build the `AimTrackerEditor` target on the first launch.
3. Click Play in the editor to start the drill.

Tested engine path: `D:\UE_5.8`.
