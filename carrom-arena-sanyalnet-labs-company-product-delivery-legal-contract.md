# STATEMENT OF WORK AND AUTONOMOUS SOFTWARE DELIVERY AGREEMENT
## CARROM ARENA

This Statement of Work and Autonomous Software Delivery Agreement (the "Agreement") is issued by the Overseer to SANYALnet Labs (the "Company") for the design, implementation, verification, and delivery of the software product identified herein as "Carrom Arena" (the "Product").

The Overseer has retained and hired the Company to deliver the Product as a correct, complete, production-quality, bug-free software application meeting every Mandatory Requirement stated in this Agreement. The Company, acting through its Chief Executive Officer ("CEO"), Chief Product Officer ("CPO"), Chief Technology Officer ("CTO"), programmer, reviewer, tester, QA function, and any other internal agent or division used by the Company's SDLC, shall perform the work in accordance with this Agreement.

This Agreement is intended to operate as the controlling project directive supplied to the Company's autonomous SDLC. It is not an invitation to provide estimates, prototypes, partial work, reduced scope, or best-effort results. Acceptance is conditioned solely upon satisfaction of the requirements and evidence gates set forth herein.

The Recitals, Articles, technical requirements, testing requirements, acceptance conditions, and appendices of this Agreement are incorporated into and form part of the same project mandate.

## RECITALS

WHEREAS, the Overseer requires a graphical, cross-platform, four-player, autonomous Carrom Arena capable of playing correct carrom doubles without human gameplay intervention;

WHEREAS, the Company represents an internal multi-role software-development organization capable of product definition, architecture, implementation, static review, dynamic QA, repair, and final executive acceptance;

WHEREAS, the Overseer intends to supervise the engagement only through the CEO and does not intend to act as programmer, tester, architect, product manager, or routine approval authority;

WHEREAS, correctness and completeness are the canonical objectives of this engagement, and engineering efficiency or speed shall not justify reducing the required standard;

WHEREAS, the Company's ordinary SDLC contains human approval and HALT points that are intentionally superseded for this engagement by the autonomous-execution provisions below while all internal quality gates remain in force;

NOW, THEREFORE, the Company is directed to perform and complete the Work subject to the following terms.

## ARTICLE 1 - DEFINITIONS AND RULES OF INTERPRETATION

For purposes of this Agreement:

1. "Acceptance Conditions" means every condition in Article 22 together with every prerequisite gate incorporated into that Article.
2. "Company" means SANYALnet Labs and every internal role, subagent, division, or process acting on its behalf for this project.
3. "Final Release Candidate" means the exact build, source tree, configuration, dependencies, assets, and executable state proposed for final delivery after all Production-Affecting Changes have ceased.
4. "FULL GAME" means one complete rendered Game under the selected authoritative carrom rules profile, beginning before or at its first break and continuing through every constituent Board and every shot until the engine lawfully reaches GAME_COMPLETE. A single Board, shortened demo, replay fragment, headless-only run, or partial observation is not a FULL GAME.
5. "Genuine External Blocker" means an external condition outside the Company's reasonable design or engineering control that makes performance impossible, such as an unavoidable missing credential, unavailable required external system, or prohibited host capability. Ordinary defects, failed tests, difficult engineering, technology choices, UI choices, and internal implementation problems are not Genuine External Blockers.
6. "Mandatory Requirement" means every requirement expressed in this Agreement by "shall", "shall not", "must" if any remains, an unconditional acceptance bullet, an explicit gate, or language declaring a condition required, mandatory, prohibited, or necessary for delivery.
7. "Overseer" means the human client and hiring authority for this project.
8. "Production-Affecting Change" means any change to production source, game rules, physics behavior, AI behavior, dependencies, build configuration, runtime configuration, shipped assets, or any other element capable of changing delivered behavior or invalidating prior evidence.
9. "Verified Delivery" means delivery of the Final Release Candidate only after all Mandatory Requirements and Acceptance Conditions have been satisfied and all required evidence and certifications exist on disk.
10. "Work" means all research, design, architecture, coding, review, testing, repair, documentation, evidence generation, certification, and delivery activity required to achieve Verified Delivery.
11. "Circular Trace File" means the single persistent local diagnostic trace file required by Section 14.2. For this Agreement, its hard maximum size of 8 MB means exactly 8,000,000 bytes.
12. "Project Material" means every project-related source file, header, test, document, script, configuration, workflow, manifest, asset, filename, path, generated file, build artifact, archive member, binary or embedded string, diagnostic trace or log, CI/job/build log retained for the project, screenshot, video, evidence file, version-control object or administrative metadata, repository configuration or hook, branch, tag, reflog where accessible, commit message, pull-request content, issue content, release content or release asset, submodule reference/content, large-file object referenced by the repository, and every file committed or pushed in connection with the Product.
13. "Prohibited AI Identifier" means any proper name, brand, model designation, provider name, assistant name, agent name, model-family identifier, vendor-specific model token, or recognizable alias that identifies a development-system or external artificial-intelligence agent, assistant, model, model family, provider, or orchestration service. Generic functional terms such as "AI", "agent", "model", "controller", and "LLM", and neutral game-player or Company-role labels, are not names and are not prohibited unless they are used as or form part of a specific Prohibited AI Identifier.

Rules of interpretation:

- "Including" means "including without limitation."
- A permission granted to the CEO does not waive a Mandatory Requirement.
- Advisory architecture language identified expressly as a "hint", "helper", "recommended", "desirable", or "strong implementation pattern" may be adapted by the CTO only where the adaptation does not weaken any Mandatory Requirement.
- A requirement to prove or verify behavior is separate from, and additional to, the requirement to implement that behavior.
- Silence, absence of a test failure, or successful compilation shall not by itself constitute proof of correctness.
- No ambiguity shall be interpreted to lower the acceptance standard.
- If two provisions can reasonably be read together, both shall apply.
- If any internal Company directive conflicts with this Agreement on operator pauses, approval pauses, scope, quality, acceptance, or delivery, this Agreement controls to the extent of the conflict.

## ARTICLE 2 - INCORPORATION, AUTHORITY, ORDER OF PRECEDENCE, AND AUTONOMOUS EXECUTION

2.1 This Agreement constitutes an explicit Overseer directive for the current project.

2.2 This Agreement supplements the preloaded SDLC-Multi-Agent-Project-Directive.md and overrides that directive only to the extent that the preloaded directive would otherwise require Overseer approval, a clarifying question to the Overseer, or a HALT between phases.

2.3 The Overseer hereby pre-authorizes the CEO to cause the Company to execute Phases 1, 2, 3, 4, and 5 continuously, including every necessary repair, re-review, re-test, and re-certification loop, without pausing for Overseer approval.

2.4 Every instruction equivalent to "HALT and wait for the Overseer" shall, for this engagement, be construed as follows:

"Record the result internally, allow the CEO to determine whether the applicable internal phase gate is satisfied, and continue autonomously if it is."

2.5 The waiver of human approval pauses does not waive, reduce, or bypass any internal architecture, implementation, review, QA, evidence, or delivery gate. All such gates remain mandatory.

2.6 Performance shall commence immediately. The Company shall not ask the Overseer routine clarifying questions. Ordinary product, UX, architecture, implementation, testing, and documentation ambiguities shall be resolved internally by the CEO, CPO, CTO, programmer, reviewer, tester, and other appropriate Company personnel.

2.7 The Company shall continue performance until either:

1. Verified Delivery has been achieved; or
2. a Genuine External Blocker has been established by evidence.

2.8 Preference questions, technology-stack choices, UI details, ordinary defects, failed tests, architecture flaws, implementation difficulty, performance problems, code-review findings, and QA failures shall not constitute Genuine External Blockers.

2.9 The Product shall be designed so that no external credential or paid runtime service is required for normal delivered operation.

## ARTICLE 3 - ENGAGEMENT, STANDARD OF PERFORMANCE, MATERIAL BREACH, AND TERMINATION

3.1 The Overseer hereby retains, engages, and hires the Company for the sole purpose of achieving Verified Delivery of a correct, complete, production-quality, bug-free Carrom Arena satisfying every Mandatory Requirement in this Agreement.

3.2 The engagement is a product-delivery engagement. It is not a research engagement, prototype engagement, proof-of-concept exercise, demonstration, MVP exercise, best-effort undertaking, or invitation to renegotiate a smaller scope after performance begins.

3.3 The Company is accountable both for producing the complete Product and for proving, through the evidence required by this Agreement, that the complete Product operates correctly.

3.4 Failure to satisfy any Mandatory Requirement at delivery constitutes a material failure of the engagement. If the Company tenders, represents, or declares a final delivery while even one Mandatory Requirement is unmet, unverified, knowingly defective, silently omitted, weakened, waived, mocked, stubbed, deferred, or reclassified as nonblocking without authorization, the engagement shall be deemed failed and the Overseer will terminate the engagement and fire the Company for nonperformance. Ordinary intermediate test or review failures encountered and honestly repaired before delivery do not themselves trigger termination; they trigger the mandatory repair and re-certification loops in this Agreement.

3.5 The termination consequence in Section 3.4 shall not be construed as authorization to conceal defects, falsify evidence, weaken or remove tests, suppress errors, reinterpret requirements to manufacture a pass, or issue an unsupported certification. Any discovered defect shall instead be repaired, and every review, QA, visual-observation, and certification gate invalidated by that repair shall be rerun honestly.

3.6 No Company role or division may treat a Mandatory Requirement as "good enough", "close enough", "mostly complete", "acceptable for an MVP", "probably fine", or any equivalent lesser standard.

3.7 No Mandatory Requirement may be dropped, relaxed, deferred, or avoided merely because compliance is difficult, time-consuming, inconvenient, or expensive in engineering effort. The Company shall not ask the Overseer to waive, downgrade, defer, reclassify, or remove a Mandatory Requirement in order to obtain acceptance or avoid termination.

3.8 The only acceptable project outcomes are:

1. VERIFIED DELIVERY - every Mandatory Requirement and Acceptance Condition is satisfied and proven by the required evidence; or
2. GENUINE EXTERNAL BLOCKER - the CEO proves that delivery is impossible because of an external condition outside the Company's control and reports that blocker in the manner permitted by this Agreement.

3.9 Any purported final project outcome other than those stated in Section 3.8 constitutes Delivery Failure. A Delivery Failure under this Section terminates the engagement and the Company will be fired for nonperformance. The only way to alter a Mandatory Requirement is an explicit written amendment issued by the Overseer before the Company purports to make final delivery; silence, informal discussion, Company recommendation, or a request for leniency is not an amendment and creates no waiver.

3.10 The CEO personally owns the final delivery decision and the truth, completeness, and evidentiary support of every certification presented to the Overseer.

3.11 No waiver shall arise from silence, delay, prior tolerance of a defect, successful compilation, passing tests that do not cover a requirement, or partial acceptance of intermediate work. Only Verified Delivery satisfies the engagement.

## ARTICLE 4 - GOVERNANCE, REPORTING, AND OVERSEER INTERFACE

The Overseer is the client, hiring authority, and oversight authority, and is not a member of the implementation team.

Only the CEO shall address the Overseer.

All CPO, CTO, programmer, reviewer, and tester discussions, disagreements, handoffs, defect reports, and repair instructions are internal company business. Subagents report to the CEO, not to the Overseer.

The Company shall not request phase-by-phase approvals from the Overseer.

The Company shall not expose raw subagent chatter to the Overseer unless it is needed to explain a genuine blocker or the final Verified Delivery.

The CEO has authority to:

- refine the PRD while preserving the mission and Mandatory Requirements below;
- approve the CPO product specification;
- approve or reject the CTO architecture;
- send implementation back to the programmer;
- send review findings back to the programmer;
- send QA failures back through the programmer and reviewer;
- change technical choices when evidence shows the current choice is defective;
- require additional tests, instrumentation, or documentation;
- continue repair loops as many times as required.

If a Genuine External Blocker occurs, the CEO may send one concise Overseer-facing blocker report stating the evidence, attempted alternatives, and the single action required from the Overseer. Otherwise remain autonomous through delivery.

## ARTICLE 5 - SCOPE OF WORK AND REQUIRED DELIVERABLE

Design, implement, test, and deliver a polished graphical application named "Carrom Arena".

Carrom Arena shall show exactly four autonomous players playing the carrom board game on one graphical carrom board.

The human user is a spectator and arena Overseer, not a player.

The application shall be genuinely self-playing:

- no human aim selection;
- no human striker placement;
- no human power selection;
- no human move confirmation;
- no scripted or pre-recorded shot sequence;
- no teleporting pieces to predetermined outcomes.

Every visible shot shall be selected by an autonomous player controller, executed through the live physics simulation, resolved by the rules engine, and reflected in the displayed game state.

For standard four-player carrom, implement doubles: two teams of two players, with partners seated opposite each other and all four sides occupied.

The Product shall automatically play from initial setup through completed Boards, Games, and a Match without Overseer intervention.

After a match ends, present the result clearly. Provide an arena option to start another match. Automatic next-match play after a short visible intermission is desirable if it remains easy to pause.

## ARTICLE 6 - PRIORITY OF OBLIGATIONS

Priority order is:

1. Rules correctness.
2. State-machine correctness.
3. Physics correctness and stability.
4. Autonomous player legality and competence.
5. Runtime reliability and deterministic reproducibility.
6. Clear graphical presentation.
7. Spectator enjoyment.
8. Performance optimization.

The Company shall not trade correctness for speed of development.

The Company shall not declare an MVP complete while mandatory behavior remains stubbed, mocked, faked, disabled, or unverified.

## ARTICLE 7 - GOVERNING CARROM RULES, RESEARCH DUTY, AND DIGITAL RULES PROFILE

Before implementation, the CPO and CTO shall establish a written digital rules profile based primarily on authoritative or federation-grade carrom rules.

Use the International Carrom Federation laws, or the best currently accessible primary federation source, as the authority. A useful published reference is:

https://www.carrom.co.uk/laws-of-carrom/

That reference states that the laws were adopted by the International Carrom Federation and includes equipment, doubles, break, turn, scoring, foul, due, and queen rules.

At minimum, verify and document the following baseline facts before coding:

- The playing surface is approximately 73.5 to 74.0 cm square.
- Four corner pockets are approximately 4.45 cm in diameter.
- A standard set has 9 white carrom men, 9 black carrom men, and 1 red queen.
- Carrom men are approximately 3.02 to 3.18 cm in diameter.
- The striker is circular and no more than approximately 4.13 cm in diameter.
- In doubles, partners sit opposite each other and the four players occupy all four sides.
- The queen is common to both teams and has cover requirements.
- A legal turn may continue when the player legally pockets the appropriate own carrom man or queen, subject to the queen and foul rules.
- Pocketing the striker creates a due/penalty situation.
- Board scoring, queen scoring, Game scoring, break rotation, and doubles side/turn behavior shall be explicitly defined.
- Under the cited ICF baseline, the breaker has white carrom men for that Board and the opponent has black; the Queen is common.
- A Game is won at 25 points or at the rules-defined conclusion of eight Boards, with the rules-defined extra-Board procedure used when required for a tie.
- A Match is decided by the best of three Games.
- Queen value is 3 points only under the rules-defined eligibility conditions; once the scoring side has reached 22 points, the additional Queen value is no longer available.
- The maximum ordinary Board score under the cited baseline is 12 points, subject to the authoritative treatment of dues/penalties and any later primary-rule revision adopted in RULES.md.
- Doubles break/turn rotation and change-of-sides behavior, including the right-hand progression required by the cited laws, shall be represented explicitly in the state machine.
- The break-validity rules, including missed-contact retry/loss-of-break behavior and the special striker/no-contact case, shall be represented if they remain part of the selected authoritative profile.
- State-changing Due, foul, Queen, last-carrom-man, and simultaneous-pocket combinations shall be implemented from the authoritative rule table rather than reduced to one generic penalty rule.

If a newer primary federation rule source changes any numerical value or procedure above, RULES.md may adopt the newer authoritative rule only if it identifies the exact source, revision/date where available, changed clause, and corresponding test updates. The Company shall not silently substitute house rules.

The Company shall not blindly copy a simplified commercial rule page if it conflicts with the primary rules source.

Create RULES.md before or during architecture work. It shall contain:

- the chosen rules profile name, such as ICF_Doubles_Digital_v1;
- source links and access notes;
- the implemented rule behavior;
- the digital adaptations;
- the physical/tournament-only rules intentionally omitted because they cannot occur in a computer simulation;
- any ambiguous rule interpretation and the reason for the chosen interpretation;
- a rule-to-test mapping for every state-changing digital gameplay rule implemented from the authoritative profile, with explicit coverage or a documented reason a rule is physically inapplicable to the digital simulation.

Physical administrative behavior such as chair height, elbow position, application of powder, player speech, and referee etiquette does not need to be simulated. The Company shall not invent gameplay penalties for events the software cannot physically perform.

Rules that affect coin ownership, turns, queen status, striker dues, fouls, penalty-piece replacement, board completion, scoring, or match completion are not cosmetic and shall be implemented and tested.

## ARTICLE 8 - FOUR-PLAYER DOUBLES MATCH MODEL

Model four distinct player seats: North, East, South, and West, or equivalent unambiguous names.

Partners shall be opposite.

Each player shall have:

- a stable identity and visible label;
- an independent controller instance;
- an independent seeded random stream if randomness is used;
- individual statistics such as shots taken, successful pockets, and fouls;
- a visible active-turn state.

Team scoring is authoritative. Individual statistics are informational only.

The Company shall not confuse a player's UI accent color with the team's white/black carrom-men assignment for the current board.

Research and implement the correct break, color assignment, turn progression, board scoring, game scoring, match scoring, and side changes for the selected doubles rules profile.

Where the source laws use the terms Board, Game, and Match, preserve those meanings in the engine and explain them in the UI or documentation.

The default experience shall be a complete self-playing Match. A faster single-Board demonstration mode may also exist, but it shall not replace the full Match implementation.

## ARTICLE 9 - ARCHITECTURAL COVENANTS AND PLATFORM DIRECTION

The CTO shall choose the lowest-risk maintainable architecture within the Overseer's intended platform direction.

Strong CTO architecture hint: this project is intended to be a cross-platform native application written in portable C17, with raylib providing the graphics/window/input platform layer. Treat portable C17 plus raylib as the presumptive architecture for Phase 2. Keep platform-specific code isolated or absent, and keep the core rules, physics, AI, and match logic portable and independent of the renderer. The delivered source tree and build configuration shall support Windows, Linux, and macOS desktop targets through raylib-compatible toolchains unless the evidence-backed deviation rule below is invoked.

The CTO may deviate from portable C17 plus raylib only if a concrete technical blocker makes that architecture materially incapable of satisfying the Mandatory correctness or verification requirements. Any deviation shall be evidence-backed, documented in ARCHITECTURE.md, and explicitly approved internally by the CEO before implementation. Convenience, developer familiarity, or faster prototyping are not sufficient reasons to deviate. A deviation does not waive the cross-platform desktop requirement unless the same documented blocker specifically makes one or more target platforms impossible.

The Company shall not move authoritative game logic into raylib callbacks. raylib is the presentation/platform layer, not the rules engine.

Regardless of stack, preserve these boundaries:

### 9.1 Rules engine

The rules engine is the authoritative source for:

- legal phase transitions;
- player/team ownership;
- turn continuation and turn changes;
- queen state and cover state;
- striker dues and penalties;
- penalty-piece replacement;
- board completion;
- scoring;
- game completion;
- match completion.

Rules logic shall not live in the renderer.

### 9.2 Physics engine

The physics layer is the authoritative source for physical motion and contact outcomes.

It shall handle at minimum:

- circular striker and carrom men;
- disc-to-disc collisions;
- disc-to-cushion collisions;
- four pockets;
- rolling/sliding slowdown using a documented digital friction model;
- pocket capture;
- a reliable rest/settled condition;
- finite coordinates and velocities at all times;
- collision stability under strong break shots.

Use a fixed simulation time step or an equivalently deterministic technique.

The Company shall not tie physical outcomes to display frame rate.

Changing arena playback speed shall change wall-clock presentation speed, not the underlying physics parameters or the logical outcome of a seeded shot.

Prefer a mature, permissively licensed 2D physics library over writing a collision solver from scratch unless the CTO can justify the latter with evidence.

Use official board and piece dimensions as proportions or normalized world units. The exact physical friction coefficient is not standardized by the basic equipment laws; choose and document a plausible tuned value rather than pretending an invented number is official.

### 9.3 Autonomous player controller

Player controllers decide shots. They do not directly mutate game state.

Define a clean controller contract equivalent in spirit to:

observe immutable game state -> return a legal shot plan

A shot plan shall contain enough information to execute the stroke, such as:

- legal striker placement on the active player's baseline;
- aim direction or target point;
- shot power;
- optional metadata describing the selected tactic.

Before a live shot, validate the controller result against current game state. Reject or repair illegal placements and parameters through deterministic internal logic. A controller shall never be able to bypass the rules engine.

### 9.4 Match controller and state machine

Use an explicit state machine. An appropriate shape is:

SETUP
-> PLANNING
-> STRIKER_PLACEMENT
-> SHOT_IN_MOTION
-> SHOT_RESOLUTION
-> TURN_CONTINUE or TURN_ADVANCE
-> BOARD_COMPLETE
-> GAME_COMPLETE
-> MATCH_COMPLETE

Adjust names if desired, but do not implement the match as a loose collection of UI callbacks.

A shot is not resolved until all moving objects have settled or a clearly documented safety timeout has detected a physics fault.

### 9.5 Renderer

The renderer consumes authoritative state and draws it.

It shall not secretly correct the rules or physics state to make the display look right.

Visible piece positions shall correspond to the actual simulation state.

Resizing the window or changing visual scale shall never alter world coordinates, scores, turns, AI decisions, or physics.

## ARTICLE 10 - PHYSICS, BOARD GEOMETRY, AND SIMULATION REQUIREMENTS

Render a recognizable top-down carrom board with:

- square wooden playing surface and frame;
- four corner pockets;
- four player baselines and base circles;
- center circle and outer circle;
- standard decorative arrows or a faithful equivalent;
- 9 white carrom men;
- 9 black carrom men;
- 1 red queen;
- 1 striker when a shot is being prepared or executed.

The initial carrom-men rack shall follow the selected authoritative setup rather than a random pile.

Striker placement shall be legal for the active player's side.

A desired striker location that is geometrically blocked shall not produce overlap or teleportation. The planner shall find another legal location.

Tune rail restitution, disc restitution, damping/friction, pocket geometry, sleep thresholds, and solver iteration/substep settings so that:

- break shots disperse the rack naturally;
- ordinary shots can pocket pieces;
- pieces do not visibly tunnel through one another or through rails;
- pieces do not remain in endless micro-motion;
- pieces do not explode numerically;
- pocketed pieces stop participating in live collisions;
- the striker is correctly removed/reset between shots.

If the selected 2D engine cannot faithfully model 3D cases such as a carrom man standing on edge, document that as a digital adaptation. The Company shall not fake unsupported 3D behavior.

## ARTICLE 11 - AUTONOMOUS PLAYER INTELLIGENCE AND SHOT PLANNING

The arena shall contain real autonomous shot selection, not random angle-and-power spam.

Build at least two controller implementations:

1. A simple RandomLegal or Baseline controller useful for tests.
2. The normal Arena controller used by the four visible players.

The Arena controller shall use physics-aware candidate generation and forward evaluation.

At minimum consider:

- direct own-coin-to-pocket shots;
- cut shots;
- legal queen attempts;
- queen-cover opportunities;
- break shots;
- rebound/bank candidates when useful;
- defensive or low-risk legal shots when no strong pocket is available;
- striker-pocket risk;
- accidental opponent-coin risk;
- foul risk;
- leave quality for the opposing side.

A strong implementation pattern is:

1. Enumerate legal striker placements.
2. Generate candidate targets/pockets and shot lines.
3. Generate plausible power levels.
4. Clone/snapshot the physics state.
5. Simulate candidates without mutating the live board.
6. Score resulting states.
7. Choose the best legal candidate within a bounded search budget.
8. Execute only that chosen shot in the live world.

For gameplay decisions, the bounded search budget shall be defined in deterministic work units such as candidate count, simulation-step count, search depth, or another seed-stable bound. The Arena controller shall not choose a different shot merely because a wall-clock timeout expired sooner on one run. If a host safety watchdog is also used, watchdog expiry shall be treated as a diagnosable planning fault with a deterministic legal fallback and shall be recorded in the trace.

The live physics execution is authoritative. The candidate simulation is prediction, not permission to force the predicted outcome.

The four visible players may share planner code, but they shall be separate controller instances. Give them distinct strategy profiles or skill parameters so that they do not behave as four identical copies. Differences shall affect real decision weights or search behavior, not just names.

Any intentional aim or power imperfection shall be seeded and reproducible.

Partners may pursue the same team objective, but each player chooses only when that player's turn arrives. The Company shall not add hidden inter-player communication that violates the chosen doubles rules profile.

The shipped application shall not require an external artificial-intelligence service, cloud API, external model service, login, API key, or internet connection at runtime.

Keep the player-controller interface extensible so a future LLM or human controller could be added without rewriting the rules or physics engine.

## ARTICLE 12 - GRAPHICAL ARENA AND SPECTATOR EXPERIENCE

The CPO owns the spectator experience.

The graphical arena shall make it immediately obvious:

- whose turn it is;
- which players are partners;
- which team currently owns white and black;
- the board/game/match score;
- queen status;
- whether a cover is required;
- when a foul or striker due occurs;
- who won the board, game, and match.

Provide four visible player panels or equally clear player indicators.

Before each shot, briefly show the selected striker position, aim direction, and power in a readable way. The Company shall not reveal thousands of internal candidate simulations.

During the shot, animate the actual physics smoothly.

Provide a compact event log or commentary strip for important events such as:

- break;
- own carrom man pocketed;
- opponent carrom man pocketed;
- queen pocketed;
- queen covered;
- queen returned;
- striker pocketed;
- due/penalty applied;
- turn changed;
- board/game/match won.

Provide at minimum these spectator controls:

- Pause/Resume.
- Normal speed.
- At least one slower speed.
- At least one faster speed.
- New Match or Restart Match.
- Sound toggle only if sound is implemented.

Playback-speed changes shall not change the deterministic outcome for a given seed.

At normal speed, target smooth desktop animation, ideally around 60 rendered frames per second where the host allows it. The Company shall not make CI fail merely because a headless runner cannot measure a desktop frame rate.

The board shall remain square, centered, and fully visible at common desktop window sizes. Player panels and controls shall not cover pockets or active play.

Use local/procedural/vector graphics where practical. The Company shall not make the product depend on hot-linked art, a CDN, or copyrighted assets of uncertain license.

No human shot controls are required in this release.

## ARTICLE 13 - RUNTIME RELIABILITY, DETERMINISM, AND REPRODUCIBILITY

Provide a seed mechanism.

A seed shall control all intentional gameplay randomness, including tosses, player imperfections, and strategy randomness.

Within the same supported runtime/platform, the same build, seed, and configuration shall produce the same logical shot choices and Match result unless the CTO documents and the reviewer verifies a specific unavoidable physics-library limitation. Any such limitation shall be narrowly stated and shall not excuse unseeded application-level randomness, timing-dependent AI choices, or playback-speed-dependent outcomes.

Provide a visible or documented way to obtain the current seed.

The application shall guard against:

- NaN or infinite physics values;
- illegal overlapping striker placement;
- a missing active player;
- a turn with no legal shot;
- an AI planning loop with no bound;
- a shot that never settles;
- duplicated or lost carrom men;
- a pocketed piece remaining collidable;
- score changes without a corresponding rules event;
- queen state becoming impossible;
- repeated no-progress play that can never terminate;
- unhandled runtime failures or process-terminating faults.

The Company shall not "solve" a deadlock by silently awarding coins, altering the score, or moving pieces to convenient locations.

If a genuine rules-defined replay condition applies, use it. Otherwise improve planning or state handling.

## ARTICLE 14 - LOCAL EXECUTION, DIAGNOSTIC TRACE, IDENTIFIER HYGIENE, DEPENDENCIES, AND EXTERNAL-SERVICE RESTRICTIONS

### 14.1 Local execution and dependency controls

This is a local application.

No backend server is required unless the CTO demonstrates a real need.

No account system, telemetry service, ads, analytics, database, or remote multiplayer is in scope.

After dependencies are installed, normal play shall not require network access.

Pin, vendor, or otherwise reproducibly control dependency versions using the selected C/CMake toolchain's appropriate mechanism. Use a lockfile where the chosen dependency manager supports one; absence of a lockfile shall not excuse unpinned or irreproducible dependencies.

Use only dependencies with acceptable redistribution licenses. Record significant third-party dependencies and licenses in the README or a license notice.

The Company shall not commit credentials, tokens, machine-specific absolute paths, or generated secret material.

### 14.2 Mandatory circular diagnostic trace file

The Final Release Candidate shall include and enable a local Circular Trace File suitable for troubleshooting rules, physics, controller, state-machine, rendering-synchronization, and runtime failures.

The Circular Trace File shall satisfy all of the following Mandatory Requirements:

1. It shall be a single persistent logical trace file. The Company shall not substitute an unbounded log, an ever-growing append-only file, or an accumulating family of rotated persistent trace files.
2. The file shall have a hard maximum on-disk size of exactly 8,000,000 bytes. The file shall never exceed that size. No implementation convention that interprets "8 MB" as a larger binary quantity is permitted.
3. When additional trace data would exceed available capacity, the implementation shall discard or overwrite the oldest complete trace records first while retaining the newest complete records. Record boundaries shall remain recoverable; a wrapped or partially written record shall never be interpreted as a valid complete record.
4. The implementation may use in-place circular storage or bounded compaction, but any temporary replacement file used during compaction shall itself be bounded to 8,000,000 bytes, shall be removed or recovered safely after interruption, and shall not become a persistent rotated history.
5. The trace facility shall survive application restart. If an existing trace is full or has wrapped, the next run shall continue bounded circular operation without truncating all useful recent history merely because the application restarted.
6. The file path shall be resolved through a cross-platform platform/filesystem abstraction to a writable per-user application-data or log location appropriate to the host operating system. Portable gameplay code shall not contain hard-coded user names, drive letters, home directories, or operating-system-specific absolute paths. Automated tests may override the trace path with an isolated temporary directory.
7. The trace format shall be versioned and documented. It shall be readable by a documented local diagnostic procedure or tool without requiring network access or a proprietary service.
8. The trace shall include enough structured information to diagnose a complete shot and state transition, including build or commit identity, seed, Game and Board identity, shot number, active player/team, selected shot plan, resulting pocket/foul/queen events, rule-resolution decision, score/turn changes, relevant invariant failures, and runtime errors. The CTO may add further useful fields while preserving the size bound.
9. The trace facility shall avoid unbounded frame-by-frame noise. Individual records shall be bounded by design. If diagnostic content is too large for a record, the implementation shall emit a bounded summary/truncation indicator rather than violating the file-size limit.
10. Trace output shall not contain credentials, secrets, environment dumps, unrestricted process command lines, or Prohibited AI Identifiers.
11. Trace I/O shall not participate in gameplay decisions. Enabling, disabling for a controlled verification comparison, wrapping, flushing, or encountering an I/O error shall not alter seeded logical shot selection, rules outcomes, scores, or Match results. A trace I/O failure shall be surfaced locally as a diagnosable condition and shall not silently corrupt authoritative game state.
12. The implementation shall use portable C and the project's platform abstraction for file operations needed by the circular trace. Any unavoidable platform-specific code shall be isolated as required by Article 9.
13. The trace shall be flushed at deterministic, diagnostically meaningful boundaries sufficient to preserve recent troubleshooting information, including at least settled-shot resolution and orderly shutdown. Fatal-error paths shall make a best effort to preserve a final bounded diagnostic record without creating recursive failure.
14. If the trace is found to contain a torn or corrupt final record after abnormal termination, startup recovery shall preserve the most recent valid records where reasonably possible or safely reinitialize the trace if recovery is impossible. Recovery behavior shall be tested and documented.
15. No other Product-generated diagnostic log may be allowed to grow without a documented bound. The Circular Trace File is the canonical persistent runtime diagnostic history for this release.
16. Any in-memory queue, staging buffer, or asynchronous trace pipeline shall also be explicitly bounded so a stalled filesystem cannot create unbounded memory growth. Trace writes shall be serialized or otherwise made race-safe.
17. The Circular Trace File is a troubleshooting history, not a substitute for a complete finite certification trace required by Article 21. If a FULL GAME can produce more diagnostic data than the circular file can retain without wrapping away its beginning, the verification harness shall produce a separate finite per-Game certification trace with a documented safety bound. That certification trace is an evidence artifact, not the persistent runtime diagnostic history, and remains subject to the identifier prohibition and evidence-retention requirements.

### 14.3 Absolute prohibition on AI agent and model names in project material and commands

Prohibited AI Identifiers are forbidden from Project Material.

The prohibition applies regardless of capitalization, filename case, quoting, encoding intended merely to evade detection, or whether the identifier appears in prose, comments, source, generated text, metadata, diagnostic output, build output, evidence, or version-control history.

Without limiting the general rule above:

1. No Prohibited AI Identifier may appear in any source, header, test, documentation file, script, configuration, workflow, manifest, generated file, build artifact, archive, binary embedded string, trace/log file, evidence artifact, screenshot or video content/metadata, filename, directory name, branch, tag, commit message, author/committer attribution added for an automated development identity, pull-request text, issue text, release text, or other Project Material.
2. No project-related command line, script invocation, CI command, build command, test command, shell command, process argument, or version-control command issued by the Company may contain a Prohibited AI Identifier.
3. In particular, no GitHub CLI command, GitHub API command, Git command, remote-repository command, or arguments/payloads supplied to such commands may contain a Prohibited AI Identifier. Remote verification shall fetch, list, or inspect repository content generically and perform prohibited-identifier comparison locally; the Company shall not use a prohibited name as a remote search query.
4. A Prohibited AI Identifier shall not be introduced into Project Material as attribution, authorship, co-authorship, generated-by text, comments, acknowledgments, badges, labels, branch names, commit metadata, release metadata, or troubleshooting notes.
5. If a Prohibited AI Identifier is discovered in a file or metadata that has already been committed or pushed, deleting it in a later commit is insufficient. Before Verified Delivery, the Company shall rewrite or recreate affected repository history and refs, remove contaminated unreachable/dangling local objects, and clean repository administrative metadata so the prohibited identifier is absent from the repository object store and retained metadata. If contaminated content was pushed to a remote repository, the Company shall use a provider-supported permanent purge where available or replace/recreate the project remote with a demonstrably clean repository and retire/delete the contaminated project remote from use. A later clean commit or force-push by itself is not proof that previously pushed contamination has been eliminated. No further commit, push, pull-request creation/update, release publication, packaging, or delivery operation may occur while known persistent contamination remains.
6. The Company shall maintain any verification denylist containing actual prohibited names outside the project workspace and outside all project repositories. The denylist itself shall never be committed, pushed, copied into Project Material, embedded in scripts, or passed as command-line arguments. A local audit tool may read that external denylist through standard input or another mechanism that does not place the names in project files or process command arguments.
7. The denylist is a verification aid, not the definition of the prohibition. The semantic prohibition in the definition of Prohibited AI Identifier controls even if a particular prohibited name was accidentally omitted from the audit list.
8. At minimum, the external audit list shall cover every development-system agent, assistant, model, model family, provider, or orchestration identity actually used, invoked, configured, mentioned, or encountered by the Company during this engagement, together with any additional identities the CEO or reviewer reasonably determines could have contaminated Project Material.
9. The Company shall use neutral Company-role terms, neutral technical descriptions, and neutral game-player/controller labels in Project Material. It shall not preserve development-tool provenance by substituting one prohibited proper name for another.
10. The Circular Trace File and every QA/CEO evidence artifact are expressly subject to this prohibition.
11. The command-line prohibition is absolute for every project-related command issued under this Agreement. A Prohibited AI Identifier shall not be supplied directly, indirectly through variable expansion, through standard input used as a command payload, through a generated request body, or through a referenced project file to any Git, GitHub CLI, GitHub API, repository-hosting, build, test, packaging, or release command. The sole permitted handling of actual prohibited names during verification is the dedicated local audit scanner described in Paragraph 6, reading the external denylist without placing those names in process arguments and without forwarding them to another project command or writing them into Project Material.
12. Remote repository metadata needed for auditing shall be fetched or streamed generically into memory or neutral temporary storage outside the project workspace and repositories, scanned locally against the external denylist, and discarded or retained only outside Project Material. The audit process shall not contaminate the project merely in order to prove that the project is clean.

The Company shall produce a final artifact named PROHIBITED_IDENTIFIER_AUDIT.md. That artifact shall not list any Prohibited AI Identifier. It shall record the audit method, audit scope, the cryptographic hash of the external denylist used, the repository refs/history scope checked, binary/archive/metadata checks performed, visual-artifact checks performed, command-hygiene certification, audit timestamp, and a zero-occurrence PASS/FAIL result.

To avoid a self-referential audit or post-commit gap, the final audit shall use this closure procedure:

1. Scan the complete candidate Project Material other than the not-yet-written final audit report.
2. Write PROHIBITED_IDENTIFIER_AUDIT.md using only neutral text, the external denylist hash, the audit scope/method, and a candidate PASS statement that is expressly contingent on successful closure.
3. If the final audit report or any other final evidence is intended to be committed or pushed, commit and push it using neutral metadata and commands that themselves satisfy this Article. Fetch the resulting final remote refs generically.
4. Perform a final closure scan over the complete actual delivery state: the finished audit report, filenames/paths, all workspace/delivery artifacts, the Git index, every local object including unreachable/dangling objects, final local and fetched remote refs, commit/tag metadata, repository administrative metadata, release assets, and all other Project Material required by Section 16.7.
5. Record the closure-scan result only in the CEO's delivery communication or neutral evidence stored outside Project Material. Do not modify Project Material merely to record the closure result.

The candidate PASS in PROHIBITED_IDENTIFIER_AUDIT.md becomes valid only if Step 4 returns zero occurrences and the command-hygiene certification remains true. If Step 4 fails, the candidate PASS is invalid; remediate the contamination and restart the entire closure procedure. Any Project Material change after a successful Step 4 invalidates the clean state and requires the closure procedure to be repeated.

## ARTICLE 15 - REQUIRED PROJECT RECORDS AND DOCUMENTATION

Deliver at minimum:

- README.md
- RULES.md
- ARCHITECTURE.md
- TESTING.md
- TRACE.md
- PROHIBITED_IDENTIFIER_AUDIT.md before Verified Delivery
- source code
- dependency manifest and lockfile where applicable
- automated tests
- a sane .gitignore
- any scripts needed for build, test, verification, and launch
- final verification traces and visual-evidence manifests required by Articles 17, 20, and 21
- the bounded Circular Trace File or a final verification copy of it where appropriate for evidence
- FULL_GAME_QA_CERTIFICATION.md and FULL_GAME_CEO_CERTIFICATION.md before Verified Delivery

README.md shall contain:

- what Carrom Arena is;
- prerequisites;
- exact clean-install command;
- exact development/run command;
- exact production build command;
- exact full-test command;
- exact verification/soak command;
- controls;
- rules-profile summary;
- Circular Trace File location, hard 8,000,000-byte limit, wrap behavior, and diagnostic-reading procedure;
- known limitations, if any.

ARCHITECTURE.md shall explain at least:

- rules engine;
- physics layer;
- player-controller interface;
- Arena AI;
- match state machine;
- renderer/UI;
- determinism/seed design;
- test boundaries;
- Circular Trace File architecture, cross-platform path resolution, record format/versioning, wrap/recovery behavior, and its separation from authoritative gameplay decisions;
- Prohibited AI Identifier hygiene architecture for generated files, evidence, and repository operations;
- cross-platform build strategy for Windows, Linux, and macOS;
- any platform-specific code and why it is necessary;
- the evidence used to support the portability claim.

TRACE.md shall document the trace format/version, path-resolution behavior, 8,000,000-byte hard limit, circular wrap semantics, corruption/recovery behavior, diagnostic-reading procedure, and tests that prove the limit.

TESTING.md shall map important acceptance criteria to actual automated or manual verification, including trace-boundary tests and the prohibited-identifier audit gate.

If the current workspace is not already a git repository, initialize one unless the environment makes that inappropriate. The Company shall not commit dependency caches, build output, screenshots that are purely temporary, secrets, or Prohibited AI Identifiers.

## ARTICLE 16 - MANDATORY AUTOMATED VERIFICATION

The tester shall build a serious verification suite. A handful of happy-path unit tests is not sufficient.

### 16.1 Rules tests

Create deterministic tests for at least:

- initial rack inventory;
- team/seat relationship;
- toss/break assignment and white/black ownership for the Board;
- valid break contact;
- failed-break retry and loss-of-break behavior where required by the selected profile;
- striker/no-contact break edge case where required by the selected profile;
- legal turn continuation;
- legal turn advance, including correct right-hand doubles progression;
- own carrom-man pocket;
- opponent carrom-man pocket;
- simultaneous own/opponent carrom-man pocket;
- queen eligibility, including the own-carrom-man prerequisite and outstanding-Due restrictions;
- queen pocket and same-shot cover where applicable;
- queen pocket followed by successful cover;
- queen pocket followed by failed cover and queen return;
- Queen scoring eligibility below/at the rules-defined 22-point cutoff;
- striker pocket alone and outstanding Due;
- striker plus own carrom-man combination;
- striker plus opponent carrom-man combination;
- striker plus own and opponent carrom-men combination;
- Queen plus striker combination;
- Queen plus own carrom man plus striker combination;
- covering-Queen plus striker edge cases;
- foul/improper-stroke penalty behavior retained by the digital profile;
- penalty/Due availability, restoration, and outstanding-Due behavior;
- board completion;
- board scoring, including the rules-defined maximum ordinary Board score;
- Game scoring to 25 points;
- eight-Board Game completion and tie/extra-Board procedure where applicable;
- Match completion as best of three Games;
- break changes and doubles side changes required by the selected rules;
- last-carrom-man/Queen/opponent-last-carrom-man edge cases relevant to the implemented profile.

### 16.2 Physics tests

Create deterministic tests or controlled simulations for:

- disc-to-disc collision;
- disc-to-rail collision;
- pocket capture;
- strong break shot stability;
- no NaN/infinite state;
- settling to rest;
- no permanent pocketed-body collision;
- legal striker reset;
- no gross tunneling at supported shot powers;
- playback-speed independence from physical outcome.

The Company shall not demand physically impossible conservation of kinetic energy when the model intentionally includes friction and inelasticity.

### 16.3 AI and controller tests

Verify that:

- every returned Arena shot plan is legal;
- striker placement is on the correct active baseline;
- blocked placements are handled;
- the planner terminates within a bounded deterministic work budget;
- the same seed is reproducible;
- live shot choice is not changed by playback speed or an ordinary wall-clock timing difference;
- candidate simulation does not mutate the live board, authoritative RNG state, or future live decision sequence except through the single chosen live shot;
- the controller can identify obvious direct pocket shots in constructed states;
- the controller can produce a legal fallback when no easy pocket exists;
- all four controller instances can take turns without state leakage.

### 16.4 State invariants

After every settled shot in test simulations, assert invariants such as:

- every in-play object has finite position and velocity;
- total carrom-man identity/count is conserved across in-play, pocketed, and penalty-restored states;
- the queen has exactly one valid state;
- exactly one player is active when a turn is required;
- team ownership is valid;
- score is internally consistent;
- pocketed pieces are not active physics bodies;
- striker lifecycle is valid;
- no impossible state transition occurred.

### 16.5 Headless soak test

Provide a fast headless mode that runs the same authoritative rules, physics, and Arena controller logic without rendering delays.

Run at least 100 complete boards across at least 100 deterministic seeds.

The soak test shall fail on:

- crash;
- unhandled runtime failure or process-terminating fault;
- invalid invariant;
- illegal AI shot accepted by the engine;
- non-finite physics state;
- rules deadlock;
- board that exceeds a documented generous safety bound without a valid rules-defined resolution.

Also run at least 10 complete matches across distinct deterministic seeds to prove the scoring and match state machine reaches terminal states.

Report actual counts and results. The Company shall not merely state that a soak test exists.

### 16.6 Circular trace verification

Create automated tests and controlled integration runs proving at minimum:

- the Circular Trace File never exceeds 8,000,000 bytes after creation, write, flush, wrap, restart, recovery, or orderly shutdown;
- enough records are generated to force at least three complete wrap/eviction cycles;
- the oldest complete records are evicted first and the newest complete records remain readable after wrap;
- record boundaries and format/version metadata remain valid across wrap;
- startup with a nearly full, exactly full, and previously wrapped trace continues bounded operation;
- an oversized diagnostic payload is bounded or summarized without breaching the file limit;
- a deliberately torn/corrupt final record is recovered or safely reinitialized according to TRACE.md;
- the trace path can be redirected to an isolated temporary location for tests without changing portable gameplay code;
- trace enabled versus a controlled trace-disabled comparison produces the same seeded logical choices and terminal Match result;
- trace I/O failure is surfaced and does not corrupt authoritative game state;
- no companion persistent rotated logs accumulate as an alternative unbounded history;
- the actual headless soak and at least one graphical verification run operate with tracing enabled and leave the persistent trace at or below the hard limit.

After every write in the boundary/wrap stress test, assert the on-disk trace size. Checking only the final size is insufficient.

### 16.7 Prohibited-identifier audit verification

Before the release candidate may enter final QA, and again after the last change to any Project Material, run a zero-occurrence prohibited-identifier audit.

The audit shall use an external denylist maintained outside the workspace and repositories as required by Section 14.3. The scanner and audit procedure shall not place prohibited names in project files or command-line arguments.

The audit shall cover at minimum:

- every tracked and untracked file in the project workspace;
- generated files and final build artifacts intended for delivery;
- filenames and directory names;
- case-insensitive and common-separator-normalized identifier matching across text, filenames, and metadata, plus text extracted recursively from binaries, nested archives, packaged deliverables, and common text encodings, including UTF-8 and UTF-16 where applicable;
- the Circular Trace File, finite certification traces, and every other retained log or CI/job/build log;
- documentation, test evidence, screenshots, video frames/captions/metadata, and certification artifacts; screenshots and video frames containing readable text shall receive manual visual inspection and, where practical, an additional local OCR/text-extraction check;
- the Git index;
- repository administrative metadata under the Company's control, including repository configuration, hooks, refs, packed refs, and accessible reflogs;
- every object in the local repository object database that can be enumerated, including reachable and unreachable/dangling commits, trees, tags, and blobs, plus commit messages, relevant author/committer metadata, tree paths, and blob contents;
- all local branches/tags/refs and all fetched remote refs that are part of the project history or repository state;
- submodule repositories/content, large-file objects referenced by retained project history, and release assets;
- pull-request, issue, release, workflow, retained CI/job-log, and other repository metadata under the Company's control where such metadata is used for this project;
- scripts and documented command examples;
- evidence that project-related command invocations, including Git and GitHub operations, complied with Section 14.3.

The audit passes only at zero occurrences.

If any prohibited occurrence is found, remove it everywhere required by Section 14.3, rewrite/recreate repository history or remote refs where necessary, permanently prune contaminated local objects, purge or replace a contaminated remote as required by Section 14.3, regenerate contaminated artifacts, rerun affected tests/evidence, reset any invalidated review or QA gate, and restart this audit from the beginning.

PROHIBITED_IDENTIFIER_AUDIT.md shall contain only neutral audit results and the external denylist hash; it shall not contain the prohibited names themselves.

The final zero-occurrence result shall use the closure procedure in Section 14.3 so the completed audit artifact, any final commit/push containing it, unreachable local objects, and the final fetched remote state are all included without an infinite self-modification loop.

## ARTICLE 17 - MANDATORY GRAPHICAL END-TO-END VERIFICATION

A graphical product is not verified by reading its source code.

The tester shall launch the actual built application and verify the rendered product.

For a browser implementation, use a real browser or headless browser automation such as Playwright when practical.

For a native GUI, use the best available launch and screen-capture method.

The tester and reviewer shall also verify the cross-platform delivery claim. At minimum, the current host shall be configured, built, launched, and graphically tested from the documented build system. For the other required desktop targets, obtain actual CI/build evidence when compatible target runners or toolchains are available. If such runners/toolchains are unavailable in the execution environment, perform and record a dedicated portability audit of source, CMake/build definitions, compiler assumptions, path handling, platform APIs, asset paths, and dependency configuration. Lack of a local target OS is not permission to ship known single-platform code or an unsupported build definition.

At minimum verify from a running build:

- the board renders;
- all four pockets render;
- the full starting set renders;
- four players are visible;
- partner/team information is visible;
- active player indication changes;
- the game self-starts after application launch without a human gameplay action;
- a real AI-selected shot visibly executes;
- pieces move according to the live physics state;
- turns can progress between multiple different players;
- scoreboard/event UI updates;
- pause/resume works;
- speed control works without changing rules state;
- the board remains correctly laid out after a window-size change;
- no uncaught console/runtime errors appear.

Capture at least one actual screenshot of the running arena as QA evidence. Prefer several meaningful states, such as initial board, shot in motion, and completed board/match.

If practical, capture a short demo recording, but a recording is not a substitute for tests.

The Company shall not claim visual verification from static code inspection alone.

## ARTICLE 18 - CLEAN-BUILD AND RELEASE-CANDIDATE ACCEPTANCE

Before final delivery, the Company shall perform clean-build acceptance against the Final Release Candidate from a newly cleared build/output state and a reproducibly controlled dependency state:

1. remove disposable build output, generated intermediates, and project-local caches that could mask a build or dependency defect;
2. obtain/install the pinned, vendored, or otherwise reproducibly specified dependencies using the documented command or documented offline dependency procedure;
3. configure and build using the documented commands;
4. run the complete automated test suite;
5. run the headless soak verification with the Circular Trace File enabled and verify its hard size bound;
6. run the prohibited-identifier audit against the resulting release artifacts and repository state;
7. launch the production or release build;
8. perform graphical end-to-end verification with tracing enabled;
9. rerun the prohibited-identifier audit after all final evidence artifacts have been generated; before Verified Delivery, complete the Section 14.3 closure procedure after any intended final commit/push.

If the execution environment contains unavoidable system-level compiler, SDK, or package-manager state that cannot safely be removed, the Company shall identify that retained state explicitly. Such retained host state does not waive the requirement to clear project build artifacts and prove a reproducible project dependency configuration.

The exact commands depend on the selected stack and shall be recorded.

Tests shall execute against the final on-disk code.

The Company shall not weaken tests, skip required cases, reduce assertions, disable failing checks, or change acceptance criteria merely to obtain a green result.

If a test itself is wrong, the tester shall explain the defect, the programmer shall correct it where appropriate, and the reviewer shall verify that the correction did not hide a product defect.

## ARTICLE 19 - STATIC REVIEW, REPAIR, AND RE-CERTIFICATION GATES

Phase 4 is a real gate.

The reviewer shall audit the current on-disk implementation, not only the programmer's summary.

Review at minimum:

- rules correctness;
- state transitions;
- queen/due/foul edge cases;
- coordinate transforms for four sides;
- physics/rules synchronization;
- planner/live-world separation;
- asynchronous or timing hazards;
- cleanup of physics bodies and event listeners;
- deterministic seed handling;
- renderer/state separation;
- error handling;
- Circular Trace File size-bound, wrap, recovery, portability, and determinism isolation;
- prohibited-identifier hygiene across project files, artifacts, repository history/metadata, and commands;
- tests for false positives;
- security/dependency issues;
- dead code, stubs, TODOs, and disabled functionality affecting scope.

The reviewer does not directly modify production files. Findings go to the CEO, then to the programmer for repair.

Any Production-Affecting Change after reviewer sign-off invalidates the prior review sign-off.

After the final Production-Affecting Change and after mandatory project documentation has been brought into agreement with that candidate, require two consecutive full review passes from the current disk state with zero newly identified defects or requirement gaps requiring any Production-Affecting Change or any mandatory documentation, test, verification, or evidence correction.

If either pass identifies such a defect or gap, correct it and restart the consecutive-clean-review count at zero. A documentation-only correction that changes no Product behavior still resets the review count when the correction was necessary to satisfy a Mandatory Requirement or to make certification evidence accurate.

Phase 5 begins only after the review gate is clean.

Any Production-Affecting Change made because of QA failure invalidates both the previous review and QA sign-offs. Route the fix through programmer -> reviewer -> tester again.

## ARTICLE 20 - QA RELEASE GATE

The tester is not permitted to issue a Certificate of Compliance until all mandatory tests have actually run and passed.

After the last Production-Affecting Change:

- have the CPO inspect the actual running UI and QA screenshots against the spectator requirements, and route any product/UX defect back through the CEO;
- run the full deterministic test suite;
- run the headless soak suite;
- run the graphical end-to-end checks;
- inspect runtime/console errors and the Circular Trace File;
- verify the Circular Trace File remains at or below 8,000,000 bytes and has demonstrably wrapped while preserving valid recent records;
- run and inspect the zero-occurrence prohibited-identifier audit, including final evidence artifacts, the complete enumerable local repository object database including unreachable/dangling objects, repository administrative metadata, and fetched remote repository state;
- repeat the normal full automated suite once more to catch order/state leakage.

Zero failing mandatory tests are allowed.

Zero known reproducible defects affecting required rules, physics, AI, startup, controls, scoring, match completion, graphical rendering, Circular Trace File behavior, or prohibited-identifier hygiene are allowed.

Minor cosmetic observations may be documented only if they do not contradict a Mandatory Requirement.

The tester shall produce a QA Certificate of Compliance containing actual evidence, not predictions.

## ARTICLE 21 - HARD PROJECT DELIVERY GATE: TWO FULL GAMES BY QA, FOLLOWED BY TWO FULL GAMES BY THE CEO

21.0 Condition Precedent to Acceptance

Completion of this Article is an express condition precedent to Verified Delivery and final acceptance. Neither automated tests, headless soak results, static review, screenshots, partial visual inspection, nor ordinary QA may substitute for the full-game observations required below.

The sequence is mandatory: QA Division certification first; CEO personal certification second. The CEO may not certify before QA has certified the same unchanged Final Release Candidate.


This gate is mandatory and non-waivable. It is a project delivery gate, not a documentation preference.

The Product cannot be declared DONE, RELEASED, VERIFIED, or suitable for final delivery unless this entire gate passes against the final on-disk Final Release Candidate after the last Production-Affecting Change and after all mandatory project documentation and verification material required for the gate have been brought into agreement with that candidate.

For this gate, a FULL GAME means one complete Game under the selected rules profile, beginning at the Game's true initial state and continuing without skipped gameplay through its rules-defined GAME_COMPLETE terminal state. A single Board is not a Game. A shortened demonstration, accelerated headless simulation, partial recording, selected highlights, screenshots, or a manually forced terminal state does not count as a FULL GAME. Playback may be accelerated only if every rendered shot and state transition remains visually observable and the underlying simulation is unchanged.

### 21.1 Stage A - QA Division full-game certification

After all other mandatory QA checks are green, the QA division shall launch the actual final graphical release candidate and visually monitor at least TWO FULL GAMES from start to finish. Use two distinct deterministic seeds.

QA shall monitor the rendered arena continuously for the complete gameplay lifecycle of each Game and shall analyze the corresponding execution logic for the entire Game. The observation shall not be replaced by source inspection, headless-only output, isolated screenshots, or automated pass/fail status. Automation and traces may assist the analysis, but QA itself shall evaluate the evidence and certify the result. The mandatory Circular Trace File shall remain enabled during both QA-observed FULL GAMES and shall be retained or copied as bounded evidence without violating the 8,000,000-byte limit.

For each of the two Games, QA shall retain or generate an authoritative complete finite per-shot certification trace sufficient to reconstruct the Game from its first shot through GAME_COMPLETE. This certification trace shall not depend on the Circular Trace File retaining the Game's earliest records after circular wrap. It shall have a documented finite safety bound based on the Game/shot safety limits and shall be retained as evidence.

The complete finite per-shot certification trace shall reconstruct and audit at least:

- Game seed and build/commit identity;
- player and team assignments;
- active player before every shot;
- striker placement, aim, and power selected by the controller;
- relevant collision/pocket outcomes;
- pocketed carrom men and striker events;
- queen state and cover state;
- fouls, dues, penalties, and restorations;
- turn-continuation or turn-advance decision;
- Board score changes;
- Game score/state changes;
- Board and Game terminal transitions;
- final Game winner and score.

QA shall reconcile the execution trace with the visible rendered behavior for the complete two-Game run and certify all five dimensions below:

1. Accuracy - rules, scoring, turn order, queen behavior, dues, penalties, restorations, and state transitions are correct.
2. Visual correctness - the board, pieces, motion, pockets, active-player state, scores, events, and transitions shown on screen agree with authoritative state and contain no material rendering defects.
3. Behavioral correctness - all four autonomous players behave legally and coherently; shots physically execute; turns progress; the game does not deadlock, cheat, teleport state, or require human rescue.
4. Execution-logic correctness - every observed shot and resulting transition is reconciled against the authoritative per-shot trace; controller choice, physics outcome, pocket/foul facts, rule resolution, turn decision, score mutation, queen/cover state, Board/Game transition, and displayed result agree without unexplained divergence.
5. Overall code quality - the observed behavior is supported by the final reviewed architecture and code, with no evidence of hidden scripting, renderer-side rule fixes, brittle state coupling, suppressed errors, test-only behavior, or unexplained trace/display disagreement.

QA shall produce a signed-in-role artifact named FULL_GAME_QA_CERTIFICATION.md containing, for each Game, the seed, start/end evidence, Board count, shot count, winner, score, any observations, trace/log paths, visual evidence paths, and explicit PASS/FAIL findings for all five dimensions.

QA may issue PASS only if both FULL GAMES pass all five dimensions with no unresolved defect.

If QA finds any defect requiring a Production-Affecting Change, this hard gate immediately fails, all prior final review/QA sign-offs invalidated by that change shall be rerun as specified elsewhere in this directive, and Stage A shall restart from Game 1 on the new release candidate.

### 21.2 Stage B - CEO personal full-game certification

Only after Stage A has passed may the CEO perform Stage B. The CEO may not satisfy this requirement merely by reading the QA certificate or accepting the tester's summary.

The CEO himself shall independently launch or observe the actual final graphical release candidate and visually monitor at least TWO FULL GAMES from start to finish, then independently analyze the complete execution logic and final code-quality evidence.

The CEO's two Games shall use two distinct deterministic seeds recorded in the certification. At least one of the CEO-observed Games shall use a seed not used in QA Stage A. Replaying one QA seed is permitted for comparison, but the second CEO Game shall use a different seed and at least one CEO Game shall therefore be a fresh full Game not observed by QA.

For both CEO-observed Games, the CEO shall independently evaluate and certify the same five dimensions:

1. Accuracy.
2. Visual correctness.
3. Behavioral correctness.
4. Execution-logic correctness.
5. Overall code quality.

The CEO shall inspect the complete finite per-shot certification trace for both Games and reconcile it against the rendered behavior. The mandatory Circular Trace File shall remain enabled throughout both CEO-observed FULL GAMES and shall be included in the CEO's troubleshooting/evidence review. The CEO shall specifically challenge any suspicious event, including apparently impossible pockets, illegal striker placement, unexplained turn changes, incorrect queen handling, score jumps, visual/log disagreement, physics instability, repeated AI pathologies, or runtime errors.

The CEO shall produce a signed-in-role artifact named FULL_GAME_CEO_CERTIFICATION.md containing the same evidence fields required from QA, plus an explicit statement that the CEO personally monitored two FULL GAMES after QA certification and independently found the release candidate correct.

The CEO may issue PASS only if both CEO-observed FULL GAMES pass all five dimensions with no unresolved defect.

If the CEO finds any defect requiring a Production-Affecting Change, the hard gate immediately fails. Route the defect through programmer -> reviewer -> tester, rerun every invalidated gate, then restart QA Stage A from Game 1 followed by CEO Stage B from Game 1. Prior full-game observations do not carry forward across a Production-Affecting Change.

### 21.3 Hard-gate release condition

This hard project delivery gate passes only when ALL of the following are true simultaneously for the same unchanged Final Release Candidate:

- QA has visually monitored and analyzed at least two FULL GAMES and issued FULL_GAME_QA_CERTIFICATION.md with PASS.
- The CEO, after QA PASS, has personally and independently visually monitored and analyzed at least two FULL GAMES and issued FULL_GAME_CEO_CERTIFICATION.md with PASS.
- Both certifications cover accuracy, visual correctness, behavioral correctness, execution-logic correctness, and overall code quality.
- The two CEO Games use distinct seeds, and at least one CEO Game uses a seed not used in QA Stage A.
- The complete finite per-shot certification traces and visual evidence referenced by both certificates exist on disk.
- The Circular Trace File was enabled throughout all four certified FULL GAMES, remained within the 8,000,000-byte hard limit, and its referenced evidence exists on disk.
- PROHIBITED_IDENTIFIER_AUDIT.md contains its contingent PASS, and the Section 14.3 final closure scan has returned zero occurrences after all final certification/evidence artifacts and any intended final commit/push were completed.
- No Production-Affecting Change or other Project Material change occurred after the final prohibited-identifier audit and certifications.

If any one of these conditions is false, final delivery is forbidden.

The CEO is expressly prohibited from issuing the line `FINAL DELIVERY STATUS: VERIFIED PASS` unless this hard gate has passed in full.

## ARTICLE 22 - DEFINITION OF ACCEPTED AND COMPLETE DELIVERY

22.0 Exclusive Acceptance Standard

The Product shall be deemed complete, accepted, and eligible for final delivery only when every condition below is simultaneously true for the applicable Final Release Candidate. Substantial performance, partial performance, an MVP, a mostly working build, or a release with known Mandatory defects does not satisfy this Article.


Carrom Arena is DONE only when all of the following are true:

- Four autonomous players are implemented.
- They play standard doubles with partners opposite.
- The game is visibly graphical.
- The Final Release Candidate follows the portable C17 plus raylib platform direction unless Article 9's evidence-backed deviation rule was validly invoked.
- The delivered source/build configuration supports Windows, Linux, and macOS desktop targets, or any platform exception is covered by the narrow evidence-backed blocker rule in Article 9.
- No human action is needed to choose or execute shots.
- Rules, physics, AI, and rendering are separate components.
- The required carrom rules profile is documented.
- The Queen, striker Due/penalty, foul, simultaneous-pocket, and last-carrom-man behavior required by the selected authoritative profile is implemented and tested.
- Game scoring, the eight-Board/25-point completion rules, applicable tie procedure, doubles break/side progression, and best-of-three Match completion match the selected authoritative profile and are tested.
- Shots use live physics rather than scripted outcomes.
- The Arena AI makes legal, physics-aware shot choices.
- A full board can complete autonomously.
- Full game/match scoring can reach a valid terminal result.
- The app can run without runtime cloud services or API keys.
- A cross-platform local Circular Trace File is implemented, enabled, documented, and proven never to exceed 8,000,000 bytes while wrapping oldest complete records and retaining useful recent diagnostics.
- No Prohibited AI Identifier exists anywhere in Project Material, the complete enumerable local repository object database including unreachable/dangling objects, repository administrative metadata, final fetched remote repository state, final artifacts, trace/log content, or project command material, and the final prohibited-identifier audit passes at zero occurrences.
- Seeded behavior is available and tested.
- The clean build succeeds.
- All unit/integration/rules/physics/controller tests pass.
- The 100-seed headless soak passes.
- The graphical E2E pass is complete.
- Actual screenshot evidence exists.
- Two consecutive final reviewer audits are clean.
- Final QA is green.
- QA has passed the hard two-FULL-Games visual and execution-logic certification gate on the Final Release Candidate.
- The CEO has subsequently and independently passed the hard two-FULL-Games visual and execution-logic certification gate on that same unchanged release candidate.
- FULL_GAME_QA_CERTIFICATION.md and FULL_GAME_CEO_CERTIFICATION.md exist and contain PASS with their referenced evidence present on disk.
- PROHIBITED_IDENTIFIER_AUDIT.md exists, contains a closure-validated PASS, contains no prohibited names, and its Section 14.3 closure procedure covers the final unchanged Project Material, complete local object database, and final fetched remote repository state.
- TRACE.md exists and matches the final implemented trace facility.
- Documentation matches the actual final commands and behavior.
- No required behavior is a stub, TODO, mock, disabled branch, or future promise.

If any item is false, the CEO shall not present final delivery. Continue working.

Any final delivery presented with one or more false items constitutes failure of the engagement and grounds for the Overseer to fire the Company. The CEO shall therefore prefer an honest NOT READY status and continued repair over an unsupported PASS.

## ARTICLE 23 - FINAL CEO CERTIFICATION AND DELIVERY PROTOCOL

When and only when the Definition of Done is satisfied, the CEO shall address the Overseer with one consolidated final delivery report.

The final report shall include:

1. Product summary.
2. Final architecture and technology choices, including C standard, raylib version, physics-library version, and significant dependency versions.
3. Rules profile implemented and important digital adaptations.
4. Exact commands to install, run, build, test, and run soak verification.
5. Final file/repository location.
6. Final git commit hash if git is used.
7. Review evidence, including the two consecutive clean final review passes.
8. Test evidence with actual pass counts.
9. Soak evidence with actual seeds/boards/games/matches completed.
10. Graphical E2E evidence and screenshot path(s), plus cross-platform build/portability evidence for Windows, Linux, and macOS and any narrowly documented platform exception allowed by Article 9.
11. QA full-game certification evidence: both FULL Game seeds, shot/Board counts, winners/scores, and FULL_GAME_QA_CERTIFICATION.md path.
12. CEO personal full-game certification evidence: both FULL Game seeds, shot/Board counts, winners/scores, and FULL_GAME_CEO_CERTIFICATION.md path.
13. Explicit confirmation that the hard QA-then-CEO two-FULL-Games delivery gate passed on the same unchanged Final Release Candidate.
14. Circular Trace File evidence: implemented path-resolution design, format/version, actual final size, 8,000,000-byte hard-cap result, number of forced wrap cycles tested, recovery-test result, deterministic-isolation result, and TRACE.md path.
15. Prohibited-identifier hygiene evidence: PROHIBITED_IDENTIFIER_AUDIT.md path, external denylist hash, workspace/build/binary/archive/visual scope checked, complete local object-database scope checked, final fetched remote/release/metadata scope checked, command-hygiene certification, Section 14.3 final closure result, and zero-occurrence PASS. The final report shall not reproduce the prohibited names.
16. Any genuinely nonblocking known limitations that do not violate a Mandatory Requirement. A known reproducible defect affecting required behavior shall not be relabeled as a "limitation" for purposes of final delivery, and the Company shall not request an acceptance waiver for it.
17. A concise verification matrix mapping the major requirements to PASS evidence.

Before issuing the final report, the CEO shall explicitly certify: "SANYALnet Labs has satisfied every Mandatory Requirement of the engagement; no known Mandatory defect, omission, unverified requirement, disabled behavior, or deferred requirement remains."

If the CEO cannot truthfully make that statement from evidence on the unchanged Final Release Candidate, delivery is forbidden and the Company shall continue working.

End the report with exactly:

FINAL DELIVERY STATUS: VERIFIED PASS

The CEO shall not print that line before all Acceptance Conditions have been satisfied.

If the Product is not verified, the CEO shall not issue a ceremonial, provisional, or aspirational final report. The Company shall continue repair and shall repeat every gate invalidated by the repair.

## ARTICLE 24 - COMMENCEMENT ORDER

24.1 This Agreement is effective as an immediate work order to the CEO.

24.2 The CEO shall take ownership of the engagement now and shall brief every Company division that the Overseer hired the Company to deliver the complete, correct, bug-free Product described herein. If the Company fails at delivery to meet even one Mandatory Requirement, or falsely declares final delivery before doing so, the engagement fails and the Company will be fired under Article 3.

24.3 The Company shall not respond to that consequence by lowering standards. It shall respond by identifying defects, repairing them, reviewing the repairs, testing the resulting Product, gathering evidence, and continuing performance until the applicable gate is genuinely satisfied.

24.4 The CEO shall cause Phase 1 to run against the current workspace. If the workspace is greenfield, that fact shall be recorded internally and performance shall continue without an Overseer pause.

24.5 The CPO shall lock the product, rules, and UX requirements; the CTO shall produce and internally sign off the architecture; the programmer shall implement the Product; the reviewer shall drive defects to zero in accordance with Article 19; and the tester/QA function shall dynamically prove the final Product.

24.6 The CTO shall begin from the strong presumption that the target architecture is portable C17 plus raylib for a cross-platform native graphical application, subject only to the evidence-backed exception stated in Article 9 and further assisted by Appendix A.

24.7 The Company shall implement the Circular Trace File and prohibited-identifier hygiene requirements early enough that ordinary development, review, QA, evidence generation, and repository operations exercise the same mechanisms that will be delivered. These requirements shall not be deferred until final packaging.

24.8 Following ordinary QA, the Company shall enforce Article 21. QA shall first certify at least two FULL visually monitored Games with complete execution-logic analysis. Only thereafter shall the CEO personally and independently certify at least two FULL Games. Final delivery is prohibited unless both stages pass on the same unchanged Final Release Candidate.

24.9 The Company shall run Phases 1 through 5 back-to-back without Overseer approval pauses and shall execute all necessary repair/review/test loops.

24.10 The Company shall not ask the Overseer to choose the stack, physics library, visual style, AI algorithm, file structure, or test framework. Those matters are internal Company decisions, subject to this Agreement.

24.11 The Company shall not stop performance at a plan, prototype, scaffolding, partial implementation, MVP, demonstration, or unverified build.

24.12 The Company shall proceed autonomously until Verified Delivery is achieved or a Genuine External Blocker is proven in accordance with this Agreement.


# APPENDIX A - CTO IMPLEMENTATION-DESIGN HELPER
## Advisory implementation pattern; subordinate to all Mandatory Requirements

A.1 Purpose

This Appendix is provided as a concise engineering helper to reduce unnecessary architectural exploration. It does not replace the CTO's Phase 2 architecture work and does not authorize deviation from any Mandatory Requirement.

The strongly preferred implementation direction is a native cross-platform application built in portable C17, using raylib for windowing, graphics, input, timing presentation, and optional audio, while keeping rules, match logic, AI, physics orchestration, deterministic state, and verification logic independent of the renderer. Primary raylib project reference: https://www.raylib.com/

A current Box2D C implementation is a strong candidate for the 2D rigid-body layer because the current official Box2D documentation describes the engine as portable C17 and provides appropriate rigid-body/collision facilities. Primary Box2D documentation reference: https://box2d.org/documentation/ The CTO shall verify the exact selected version, license, integration method, and deterministic limitations at architecture time against primary project documentation. The Company remains responsible for proving the selected physics implementation fit for this Product.

A.2 Suggested source boundaries

A concise project structure may resemble:

src/
  app/
    main.c
    app.c
  game/
    rules.c
    match.c
    board.c
    scoring.c
    events.c
  physics/
    physics.c
    physics_snapshot.c
  ai/
    controller.c
    baseline_controller.c
    arena_controller.c
    shot_candidates.c
    shot_evaluator.c
  render/
    renderer.c
    board_view.c
    hud.c
    effects.c
  telemetry/
    trace.c
    replay.c
  platform/
    platform.c

include/
tests/
tools/
docs/

The precise layout may differ, but dependencies should point inward toward stable game concepts rather than outward from the renderer into business logic.

A.3 Suggested authoritative data model

The CTO should consider explicit types equivalent to:

- MatchState
- GameState
- BoardState
- TeamState
- PlayerState
- PieceState
- QueenState
- TurnState
- ShotPlan
- ShotResult
- GameEvent
- DeterministicRng
- PhysicsWorld
- PhysicsSnapshot

The renderer should consume immutable or read-only views of authoritative state. Player controllers should receive an immutable decision snapshot and return a ShotPlan. The rules engine should consume settled-shot facts and produce authoritative state transitions plus GameEvents.

A.4 Suggested frame and simulation flow

A suitable top-level execution flow is:

1. Poll raylib window/input events.
2. Advance presentation controls such as pause or playback multiplier.
3. Advance the authoritative simulation through a fixed-time-step accumulator when not paused.
4. During PLANNING, request a ShotPlan from the active player's controller.
5. Validate striker placement, aim, and power.
6. Execute exactly one selected plan in the live physics world.
7. Step physics until the shot reaches a proven settled condition.
8. Convert pocket/contact results into a deterministic ShotResult.
9. Resolve the ShotResult through the rules engine.
10. Emit an ordered game event and a bounded trace record through the circular trace subsystem.
11. Advance the explicit match state machine.
12. Render the current authoritative state with raylib.

Render frame rate shall not control simulation correctness.

A.5 Suggested physics integration

Use normalized physical world units based on documented carrom geometry. Convert world coordinates to screen pixels only in the render layer.

Represent carrom men and striker as dynamic circular bodies. Represent cushions with explicit rail geometry and corner openings. Model each pocket with deterministic capture geometry, commonly a sensor/capture region coordinated with the rail opening, so a genuinely entering disc is pocketed once and removed from further collision without visual teleportation.

Do not assume a 2D engine's fixture-friction coefficient will by itself model carrom-board resistance: in a top-down gravity-free simulation, free discs may have no surface contact that produces such drag. Implement and document deterministic board-plane resistance using an appropriate translational damping, drag, or deceleration model, tuned against plausible carrom motion and the settling tests. Keep cushion/disc restitution separate from board-plane slowdown.

Remove or disable pocketed pieces from physical interaction in a deterministic manner.

Use a fixed physics step selected and justified by testing. A value such as 1/120 second may be evaluated, but it is not contractual; the CTO shall select the value that passes collision, tunneling, settling, reproducibility, and soak requirements. Evaluate Box2D continuous-collision facilities and/or sufficient substepping for the supported maximum striker speed; the no-tunneling acceptance tests, not a chosen setting by itself, determine adequacy.

Where the physics library exposes nondeterminism across architectures or builds, isolate that fact, eliminate avoidable nondeterminism, and make the supported reproducibility claim no broader than the verified evidence permits.

A.6 Suggested autonomous-player design

For each live decision:

1. Snapshot authoritative logical state and the physical world.
2. Enumerate legal striker placements for the current seat.
3. Generate tactical candidates: break, direct pocket, cut, queen, cover, bank/rebound, defensive, and fallback.
4. Generate bounded aim/power variants.
5. Run candidate simulations only in scratch or cloned physics state.
6. Score candidate terminal states using strategy weights.
7. Apply seeded imperfection only after or as part of deterministic candidate evaluation.
8. Return the best legal ShotPlan within a deterministic bounded work budget; do not use elapsed wall-clock time as the normal tie-breaker or search cutoff that determines the live shot.
9. Destroy or reset scratch state without advancing or corrupting the live authoritative RNG stream.
10. Execute only the chosen ShotPlan in the live authoritative physics world.

The four arena players may share algorithms but should hold separate controller state, RNG streams, and meaningful strategy profiles.

A.7 Suggested rules and event architecture

Prefer a rules layer that can be exercised without raylib and without a live display.

A settled shot should be reducible to facts such as:

- pieces pocketed by identity and color;
- queen pocketed or not;
- striker pocketed or not;
- foul-relevant contact or placement facts if applicable;
- active player and team;
- current queen/cover state;
- existing dues or penalties.

The rules engine should transform those facts and the prior state into:

- authoritative next state;
- score changes;
- due or restoration actions;
- turn continuation or advancement;
- Board/Game/Match completion;
- ordered GameEvents.

This enables rules tests to prove the difficult carrom logic independently from graphics.

A.8 Suggested circular trace and certification support

Build observability into the architecture instead of adding it at the end.

A strong portable-C design is a small trace subsystem behind a narrow interface, for example:

- trace_open();
- trace_write_record();
- trace_flush();
- trace_close();
- trace_recover();

The public game/rules/controller layers should submit bounded structured records without knowing operating-system paths or circular-file mechanics.

The implementation may use an in-place ring or bounded compaction. A practical in-place design may use:

- a small versioned file header containing magic/version, capacity, oldest-record/read offset, write offset, wrap generation, and recovery/check information;
- length-prefixed records with type, sequence number, payload length, and an integrity check;
- a commit marker or equivalent rule preventing a torn record from being treated as complete;
- bounded UTF-8 or compact binary payloads;
- oldest-record overwrite/eviction when space is needed.

The exact format is the CTO's choice, but the persistent file shall never exceed 8,000,000 bytes and shall retain the most recent complete diagnostic history.

For every settled shot, record at minimum:

- build or commit identifier;
- seed;
- Game and Board identifiers;
- shot number;
- active player/team;
- pre-shot state hash or equivalent canonical digest;
- selected ShotPlan;
- planner tactic metadata;
- resulting pockets/fouls/queen events;
- score and turn decision;
- post-shot state hash;
- runtime errors or invariant failures, which should normally be none.

The trace should use the platform layer to resolve a per-user writable location. Tests should inject a temporary path rather than changing portable gameplay code.

If writes are queued or asynchronous, bound the in-memory queue and serialize access so a stalled disk cannot create unbounded memory growth or record-order races.

The circular trace may overwrite old runtime records by design. The separate finite certification trace required by Article 21 should be produced by the verification harness when a complete FULL GAME record would not fit in the circular history. Do not enlarge or disable the 8,000,000-byte cap to satisfy certification.

Avoid putting unrestricted source text, environment dumps, shell history, process command lines, or tool provenance into the trace. The trace is Product diagnostic evidence, not a development-session transcript, and it is subject to the Prohibited AI Identifier ban in Section 14.3.

Provide a local diagnostic reader or documented procedure capable of reconstructing record order across wrap generations. If a separate reader utility is shipped, it is itself Project Material and shall satisfy the same portability and identifier-hygiene requirements.

The QA and CEO FULL GAME certifications should be able to cite the bounded trace directly while comparing it with the rendered game, together with any more detailed per-shot evidence required by Article 21.

A.9 Suggested verification seams

The architecture should support at least four execution modes sharing the same authoritative core:

1. Normal rendered arena mode.
2. Deterministic single-seed diagnostic mode.
3. Headless accelerated soak mode.
4. Graphical verification/capture mode.

Do not create separate fake gameplay implementations for tests. Test modes should exercise the same rules, AI, scoring, and physics code used by the delivered arena, except that rendering delays may be removed in headless mode.

A.10 Suggested build discipline

CMake is a reasonable cross-platform build-system candidate. Pin or otherwise control dependency versions. Provide clean Debug and Release builds where practical. Enable aggressive compiler warnings during development and resolve meaningful warnings rather than suppressing them indiscriminately.

The release pipeline should make it simple to execute, from a clean tree:

- configure;
- build;
- unit/integration tests;
- deterministic verification;
- soak tests with tracing enabled;
- circular-trace wrap/limit/recovery tests;
- prohibited-identifier audit;
- release build;
- graphical E2E with tracing enabled;
- evidence generation;
- final prohibited-identifier audit after evidence generation and post-commit/post-push closure scan.

This Appendix is guidance only. If the CTO chooses a materially different internal design, the CTO shall document why the alternative lowers technical risk while still satisfying every Mandatory Requirement in the Agreement. Portable C17 plus raylib remains the presumptive platform direction and may be abandoned only under the evidence-backed blocker rule in Article 9.


