# Game Logic Review - Programmer 2

## Coverage Table
| File | Total Lines | Lines Reviewed | Chunk Ranges | Status |
| :--- | :--- | :--- | :--- | :--- |
| `src/game/board.c` | 252 | 252 | 1-252 | Done |
| `src/game/board.h` | 43 | 43 | 1-43 | Done |
| `src/game/events.c` | 78 | 78 | 1-78 | Done |
| `src/game/events.h` | 28 | 28 | 1-28 | Done |
| `src/game/match.c` | 41 | 41 | 1-41 | Done |
| `src/game/match.h` | 22 | 22 | 1-22 | Done |
| `src/game/rules.c` | 458 | 458 | 1-458 | Done |
| `src/game/rules.h` | 35 | 35 | 1-35 | Done |
| `src/game/scoring.c` | 62 | 62 | 1-62 | Done |
| `src/game/scoring.h` | 27 | 27 | 1-27 | Done |
| `src/common/types.c` | 189 | 189 | 1-189 | Done |
| `src/common/types.h` | 259 | 259 | 1-259 | Done |
| `src/common/rng.h` | 147 | 147 | 1-147 | Done |
| `src/common/pcg32.h.in` | 76 | 76 | 1-76 | Done |
| `src/common/strategy_profiles.h` | 123 | 123 | 1-123 | Done |

## Findings
| ID | Severity | Location | Defect Class | Evidence | Recommended Fix |
| :--- | :--- | :--- | :--- | :--- | :--- |
| GL-001 | High | `src/common/types.h:38-40` | Spec Violation | `POCKET_RADIUS_NORM 0.030f`, `PIECE_RADIUS_NORM 0.021f`, `STRIKER_RADIUS_NORM 0.028f` | The normalized values do not match ICF specs. Board = 74cm. Pocket = 4.45cm / 74 $\approx$ 0.060. Piece = 3.2cm / 74 $\approx$ 0.043. Striker = 4.1cm / 74 $\approx$ 0.055. |
| GL-002 | Medium | `src/game/board.c:106` | Logic Error | `board->pieces[0].position = (Vec2){ INITIAL_RADIUS * 0.5f, INITIAL_RADIUS * 0.5f };` | Manual offset for white piece 0 is arbitrary and may overlap others or be non-symmetric. Use a proper formation generator. |
