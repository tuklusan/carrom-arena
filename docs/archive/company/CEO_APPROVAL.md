# CEO APPROVAL — Carrom Arena Architectural Sign-Off
**SANYALnet Labs | CEO Authorization | 2026-08-28**

---

## DECISION: APPROVED WITH MODIFICATIONS

The CTO's Architectural Sign-Off Document (ARCHITECTURAL_SIGN_OFF.md) is **APPROVED** with the following resolutions to open decisions D1–D7:

---

### Resolved Open Decisions

| # | Decision | Resolution | Rationale |
|---|----------|------------|-----------|
| **D1** | Physics fixed timestep | **1/120 s (8.33 ms)** | CTO recommendation; balances CCD headroom vs CPU; finalize after soak validation |
| **D2** | Box2D fixed-point | **Enable `B2_USE_FIXED_POINT=1` if v3 supports; else document narrow claim** | Maximizes cross-arch determinism per Article 13; fallback is documented limitation |
| **D3** | Board-plane resistance parameters | **Lock in `config.h` before Phase 3**: `BOARD_COULOMB=0.12`, `BOARD_VISCOUS=0.85`, `SETTLE_SPEED_EPS=1e-4` | CTO-tuned values; immutable after Phase 3 start |
| **D4** | AI search budget | **320 candidates** (8 legal placements × 40 aim/power variants) | Fits 16 ms planning budget on CI runners; deterministic bound per A.6.11 |
| **D5** | 4 seat strategy profiles | **Four distinct StrategyProfile weight vectors** (defined below) | CPO-approved personalities for production arena |
| **D6** | CI platform matrix | **GitHub Actions matrix**: `ubuntu-latest`, `windows-latest`, `macos-latest` | Article 22 cross-platform mandate; full coverage |
| **D7** | Audio/SFX | **Omit** (not Mandatory per Article 12.7) | Reduces dependency surface; zero risk to core requirements |

---

### D5 Detail: Four Seat Strategy Profiles

| Seat | Archetype | Weight Vector (pocket, queen, cover, striker_risk, opponent_leave, positional) |
|------|-----------|---------------------------------------------------------------------------------|
| **NORTH** (White) | **Aggressive** | (1.0, 1.5, 1.2, -0.8, -0.6, 0.4) — favors direct pockets, queen attempts |
| **SOUTH** (White) | **Balanced** | (1.0, 1.0, 1.0, -1.0, -0.8, 0.6) — standard tournament play |
| **EAST** (Black) | **Defensive** | (0.8, 0.8, 1.5, -1.2, -1.0, 1.0) — prioritizes cover, safety, positional control |
| **WEST** (Black) | **Trickster** | (1.2, 1.3, 0.8, -0.6, -0.4, 0.3) — favors banks, cuts, unconventional shots |

Profiles stored in `src/common/strategy_profiles.h` — **immutable after Phase 3 start**.

---

## AUTHORIZATION

> As CEO of SANYALnet Labs, I authorize **Phase 3: Incremental Development & Coding** to commence immediately under the approved architecture with the above decisions locked.

**Phase 3 Preconditions Met:**
- ✅ CTO architectural sign-off completed
- ✅ CEO approval granted with decisions D1–D7 resolved
- ✅ No Genuine External Blockers (Article 9.2)

---

## PHASE 3 KICK-OFF DIRECTIVE

### Programmer Agent — Primary Implementation Lead
**Objective:** Implement the complete Carrom Arena per approved architecture and contract Mandatory Requirements (Articles 5–13, 16–17, Appendix A).

**Scope:** Full product delivery — all modules in Section 2.1 of sign-off, all four verification seams, trace/telemetry, CI pipeline.

**Deliverables:**
1. **Scaffold** `src/` per approved layout with compiling stubs
2. **Core loop** — fixed-timestep accumulator, mode dispatch (4 modes)
3. **Physics** — Box2D v3 integration, normalized units, board resistance, pocket sensors, CCD
4. **Rules engine** — `rules_resolve()` pure logic, all Article 16.1 cases
5. **AI** — Controller interface, baseline + arena controllers, scratch sim, 4 strategy profiles
5. **Render** — raylib renderer, board view, HUD, effects (read-only authoritative state)
6. **Telemetry** — JSONL trace writer, human-readable log mirror
7. **Build/CI** — CMake + FetchContent, 8-stage pipeline, clean-build script
8. **Tests** — Unit tests for rules, physics settling, AI isolation; integration tests

**Constraints:**
- **Incremental commits** — meaningful steps, clean history
- **Build must pass** at every commit (CI green)
- **Zero warnings** in Debug (`-Werror`)
- **Determinism** — same binary + seed = identical trace (Article 13)
- **Headless-testable** — rules, physics settling, AI planning run without raylib

**Handoff to Reviewer (Phase 4):** Programmer reports Phase 3 complete → Reviewer begins static analysis per Article 19.

---

## PHASE 4 & 5 PRE-AUTHORIZATION

Upon Programmer's Phase 3 completion report:
- **Phase 4 (Reviewer)** auto-authorized for deep static analysis, iterative feedback loop until zero critical defects
- **Phase 5 (Tester)** auto-authorized for dynamic QA: unit/integration/soak tests, Article 16/17 certification, QA Certificate of Compliance

---

## NEXT ACTION

**Launching Programmer Agent for Phase 3 Implementation.**

---

**Document Status:** `APPROVED` — Phase 3 **ACTIVE**