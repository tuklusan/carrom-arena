# Sound plan (draft for operator review)

Status: PLAN ONLY. Nothing is implemented yet. The game canvas is locked (no layout changes).

## The seven sounds

| # | Sound | Trigger | Loudness driver |
|---|-------|---------|-----------------|
| 1 | Striker flick | the moment the striker is launched (`physics_apply_shot`) | launch power |
| 2 | Striker hits coin | contact between striker and a coin | approach speed |
| 3 | Coin hits coin | contact between two coins | approach speed |
| 4 | Striker hits side | striker against a cushion | approach speed |
| 5 | Coin hits side | coin against a cushion | approach speed |
| 6 | Striker falls into pocket | striker captured by a pocket sensor | speed at capture |
| 7 | Coin falls into pocket | coin captured by a pocket sensor | speed at capture (queen may get a distinct variant later) |

## How the game will know (no guessing from positions)

Box2D already reports what we need:
- **Hit events** (`enableHitEvents` on the piece, striker and cushion shapes, plus `hitEventThreshold` set low, about 0.05 u/s) give the two shapes, the contact point and the **approach speed** for every impact.
- Each shape carries a small tag (coin id, striker, cushion) so an impact is classified as striker-coin, coin-coin, striker-wall or coin-wall with no lookups.
- Pocket captures are already detected (`physics_check_pocket_events`); they add sounds 6 and 7. The flick (1) is emitted where the launch velocity is applied.
- Physics appends `SoundEvent { kind, speed, x, y, sim_time }` records to a small queue while stepping; the app drains the queue once per frame and hands each event to the audio module. Physics itself never touches audio, so the simulation stays deterministic and headless/CI runs are unaffected.
- Every sound event is also written to the flight recorder, so a missing or wrong sound can be investigated afterwards.

## The audio module (`src/audio/`)

- raylib's audio (miniaudio inside) is already linked: `InitAudioDevice`, `LoadSoundFromWave`, `PlaySound`. No new dependency, works on Windows.
- If no audio device exists (Xvfb, CI, diagnostic/soak modes) the module silently does nothing.
- Volume = clamp(approach speed / reference speed) with a floor so soft touches still tick; a tiny deterministic pitch variation (a counter, not the game RNG) avoids the machine-gun effect.
- A per-kind minimum gap (about 30-50 ms) and a polyphony cap (about 16 voices) stop a rack break from turning into noise.
- Pockets: the sound starts at capture, in step with the fall animation.

## Game speed

Sounds are triggered by simulation events, so they follow the game speed automatically. Proposed rule: play at natural pitch at 0.5x and above; below 0.5x lower the pitch gently (never below about 0.6) or mute the impact ticks, because a 0.1x thud is just a rumble. At 4x, cap the rate so the ticks stay clean.

## Sound assets: needs your decision

- **A. Synthesised in code (recommended to start).** Each sound is generated at startup from a few decaying sine partials and filtered noise (coin tick: about 2.5 kHz + 4 kHz, 40 ms; striker tick: about 1.2 kHz + 2.4 kHz, 60 ms; wall thud: about 150 Hz + noise, 90 ms; pocket: thud, falling pitch, net rattle; flick: click + low thump). Zero files, zero licensing questions (fits the SANYALnet non-commercial license), easy to tune by ear, and it lets me build and test everything now.
- **B. Recorded samples** (real carrom recordings, CC0 or your own). More realistic, but each file needs a license check and it adds assets to the repo and the Windows build. The audio module is written so samples can replace any synthesised sound later without other changes.

## Build order

1. Sound events in physics (hit tags, queue, launch and pocket events) with unit tests (striker-coin, coin-coin, striker-wall, coin-wall, both pockets, flick) and flight-recorder events.
2. Audio module with the synthesised set, volume/pitch/rate-limit logic (pure functions, unit-tested), silent fallback.
3. Wire into the app, verify on a real run (event log against the recorder), deliver a Windows build for you to listen to.
4. Tune by your ear; optionally swap in recorded samples.

## Open questions for the operator

1. Synthesised first (A), or recorded samples (B)?
2. Master volume and a mute key (suggest `M`)?
3. Slow-motion rule above acceptable, or should sounds always play at natural pitch?
4. Anything extra: queen pocket flourish, foul buzzer, board-won chime, UI ticks?
