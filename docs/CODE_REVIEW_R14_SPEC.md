# Product Specification: R14a Code Review Report

## 1. Overview
**Directive:** R14a
**Objective:** A complete, line-by-line audit of the codebase (Base: `main` @ `5763f4a`, R12) to ensure physics fidelity, stability, and architectural integrity.
**Deliverable:** `docs/CODE_REVIEW_R14.md`
**Constraint:** READ-ONLY. Zero code changes are permitted during this phase.

## 2. Acceptance Criteria for "Complete" Review
A report is considered "complete" only if it satisfies the following:
- **Total Line Coverage:** Every single line of code in `src/` and `tests/` has been ingested and analyzed.
- **Non-Truncation Proof:** Verification that files were read in sequential chunks of $\le 250$ lines to prevent LLM context truncation.
- **Cross-Module Analysis:** Findings must account for interactions between `physics`, `game`, and `render` modules, not just isolated files.
- **Traceability:** Every finding must reference a specific file and line number (e.g., `src/physics/engine.c:142`).

## 3. Output Format & Schema

### 3.1 Coverage Table
A markdown table tracking the audit progress.
| Module | File | Total Lines | Lines Reviewed | Status |
| :--- | :--- | :--- | :--- | :--- |
| `physics` | `engine.c` | 1200 | 1200 | ✅ Complete |
| `game` | `rules.c` | 500 | 250 | 🚧 In Progress |

### 3.2 Findings List
Findings must be categorized by severity and listed as follows:
**[ID] [Severity] [Module] - Short Title**
- **Location:** `path/to/file:line`
- **Observation:** What the code currently does.
- **Risk:** The potential failure mode (e.g., "Non-deterministic collision response").
- **Requirement:** What the code *should* do according to R13 specs.

### 3.3 Root-Cause Map
A mapping of clustered findings to systemic architectural failures.
- **Root Cause A (e.g., Floating Point Drift):** $\rightarrow$ Findings #12, #15, #22.
- **Root Cause B (e.g., Improper State Locking):** $\rightarrow$ Findings #04, #09.

### 3.4 Fix Plan
A prioritized sequence of remediation steps (to be executed in R14b).
1. **Priority 1 (Critical):** Fix [ID] $\rightarrow$ Action: [Specific technical change].
2. **Priority 2 (High):** Fix [ID] $\rightarrow$ Action: [Specific technical change].

## 4. Severity Definitions (Zero-Defect Target)

| Severity | Definition | Carrom Engine Example |
| :--- | :--- | :--- |
| **Critical** | Crash, memory corruption, or total physics collapse. | Buffer overflow in collision detection; `NaN` propagation in velocity vectors. |
| **High** | Determinism failure or blatant rule violation. | Different results on different runs with same seed; striker moving through walls. |
| **Medium** | Subtle physics inaccuracy or performance bottleneck. | Jittery movement at low velocities; $O(N^2)$ check where $O(N \log N)$ is possible. |
| **Low** | Code smell, missing docs, or stylistic inconsistency. | Unused variables; inconsistent naming in `common/` headers. |

## 5. Non-Truncation Protocol (Mandatory)
To guarantee "correctness," the auditor must:
1. Determine the total length of the file.
2. Read the file in offsets of exactly 250 lines (or fewer).
3. Explicitly log the chunk range (e.g., "Reviewing `physics.c` lines 251-500") before analysis.
4. Any skip or "summarization" of a block of code is a violation of the R14a directive.
