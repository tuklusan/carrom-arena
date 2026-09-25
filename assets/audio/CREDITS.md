# Audio asset credits

All sound effects are from the free asset packs of **Kenney** (Kenney Vleugels, https://kenney.nl), released under
**Creative Commons CC0 1.0 Universal (public domain)**: https://creativecommons.org/publicdomain/zero/1.0/
Attribution is not required by CC0; it is given here as a courtesy. The licence text shipped with the packs is
`KENNEY_LICENSE_CC0.txt`. The same packs are used in the snakes-and-ladders-arena project.

Packs (downloaded 2026-09-25 from the URLs recorded in snakes-and-ladders-arena/assets/audio/CREDITS.md):
Casino Audio, Impact Sounds, Interface Sounds.

The files are embedded in the executable at build time (see `src/CMakeLists.txt`), so the game ships as one file.
Several numbered variants exist per sound and are rotated so repeated impacts do not sound identical.

| Game file | Source (Kenney pack: file) |
|-----------|----------------------------|
| `coin_coin_1.ogg` | `casino pack: chips-collide-1.ogg` |
| `coin_coin_2.ogg` | `casino pack: chips-collide-2.ogg` |
| `coin_coin_3.ogg` | `casino pack: chips-collide-3.ogg` |
| `coin_coin_4.ogg` | `casino pack: chips-collide-4.ogg` |
| `striker_coin_1.ogg` | `impact pack: impactPlate_light_000.ogg` |
| `striker_coin_2.ogg` | `impact pack: impactPlate_light_001.ogg` |
| `striker_coin_3.ogg` | `impact pack: impactPlate_light_002.ogg` |
| `striker_coin_4.ogg` | `impact pack: impactPlate_light_003.ogg` |
| `striker_wall_1.ogg` | `impact pack: impactWood_medium_000.ogg` |
| `striker_wall_2.ogg` | `impact pack: impactWood_medium_001.ogg` |
| `striker_wall_3.ogg` | `impact pack: impactWood_medium_002.ogg` |
| `coin_wall_1.ogg` | `impact pack: impactWood_light_000.ogg` |
| `coin_wall_2.ogg` | `impact pack: impactWood_light_001.ogg` |
| `coin_wall_3.ogg` | `impact pack: impactWood_light_002.ogg` |
| `flick_1.ogg` | `impact pack: impactGeneric_light_000.ogg` |
| `flick_2.ogg` | `impact pack: impactGeneric_light_001.ogg` |
| `flick_3.ogg` | `impact pack: impactGeneric_light_002.ogg` |
| `striker_pocket_1.ogg` | `impact pack: impactWood_heavy_000.ogg` |
| `striker_pocket_2.ogg` | `impact pack: impactWood_heavy_001.ogg` |
| `striker_pocket_3.ogg` | `impact pack: impactWood_heavy_002.ogg` |
| `striker_bounce_1.ogg` | `impact pack: impactWood_medium_003.ogg` |
| `striker_bounce_2.ogg` | `impact pack: impactWood_medium_004.ogg` |
| `coin_pocket_1.ogg` | `casino pack: chips-stack-1.ogg` |
| `coin_pocket_2.ogg` | `casino pack: chips-stack-2.ogg` |
| `coin_pocket_3.ogg` | `casino pack: chips-stack-3.ogg` |
| `coin_pocket_4.ogg` | `casino pack: chips-stack-4.ogg` |
| `queen_1.ogg` | `impact pack: impactBell_heavy_000.ogg` |
| `foul_1.ogg` | `interface pack: error_002.ogg` |
| `board_won_1.ogg` | `interface pack: confirmation_002.ogg` |

Mapping by game event: `flick_*` striker flick; `striker_coin_*` striker hits coin; `coin_coin_*` coin hits coin;
`striker_wall_*` striker hits side; `coin_wall_*` coin hits side; `striker_pocket_*` striker falls through the pocket to the floor (heavy wood thud) and `striker_bounce_*` its two floor bounces;
`coin_pocket_*` coin falls into pocket; `queen_1` queen pocketed; `foul_1` foul; `board_won_1` board won.
To swap a sound, replace the .ogg with the same name and rebuild.
