# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

This is the last version of my Masters Thesis software, called "Borg." It's a simulation platform that implements a Learning Classifier System (LCS) and a Distributed LCS (DLCS), plus an animat (simulated two-wheeled robot) task environment and a Windows GUI for running, stepping, and batching simulations.

The code is 16-bit Windows 3.x C++ built with **Borland C++ 4.02** against **BWCC** (Borland Windows Custom Controls). It does not compile with any modern toolchain: it relies on `huge` pointers, `PASCAL`/`FAR`/`_export` calling conventions, `<except.h>`'s `xalloc`, pre-standard scoping (`for (int i=...)` variables reused after the loop, see `TASK.CPP`), and 8.3 filenames. Treat it as a preserved artifact — read and reason about it, but don't assume you can build or run it here.

## Build

No makefile. The project is driven by the Borland IDE project file `BORG.IDE` (targets 16-bit
Windows, include path `C:\BC4\INCLUDE`, libs `C:\BC4\LIB`, links `bwcc`). `BORG.DEF` is the
module definition; `BORG.RC` compiles to `BORG.RES`. `BORG.EXE` (NE format), `BWCC.DLL`, and
`BC402RTL.DLL` are the checked-in build output and runtime DLLs.

Note `BORG.IDE` still references two source files that no longer exist under those names
(`GLOBAL.CPP`, `ENVIRON.CPP` — the latter is now `EI.CPP`). The project file is stale relative
to the sources.

## Architecture

Three layers, deliberately separable. The classifier system proper knows nothing about robots;
the robot task knows nothing about Windows.

**Layer 1 — the classifier system (`CFS`, `CLSSLIST`, `MSGBOARD`, `MESSAGE`, `QUEUE`, `NI`)**

`classifierSystem` (`CFS.H`) is one complete LCS agent, owning a `messageBoard`, a
`classifierList`, an `environmentInterface`, and (under `NETWORK`) a `networkInterface`.
Its `clockTick()` in [CFS.CPP:141](CFS.CPP#L141) is the canonical execution cycle and the best
single place to start reading:

genetics (every `geneticInterval` ticks) → read network → `environ.readInput()` → match
classifiers against the message board → calc bids → select and post to message board →
`environ.postOutput()` (may alter the board; returns the "done" flag) → post to network →
bucket-brigade payoff → `environ.adjustStrengths()` (environment payoff) → taxes → clamp
strengths.

**Layer 2 — the task environment (`TASK`, `EI`, `COORD`)**

`taskEnvironment` (`TASK.H`, one global `task`) owns the array of `classifierSystem` agents, the
playing field, obstacle/goal geometry, and per-agent position/direction history. Its
`clockTick()` ([TASK.CPP:219](TASK.CPP#L219)) ticks every unfinished agent and then *simulates
the network* by draining each agent's Tx queue and routing messages to matching or broadcast
destinations' Rx queues.

`environmentInterface` (`EI.H`) is the user-pluggable seam — `readInput`, `postOutput`,
`adjustStrengths`, plus reset/save/load stubs. The long comment block at the top of `EI.H` is
the contract for anyone retargeting the LCS to a different problem. The current implementation
is animat-specific and reaches back into the global `task` object.

**Layer 3 — the GUI (`UI`, `GRAPH`, `BORG`)**

`userInterface` (`UI.H`, one global `UI`) and `graphics` (`GRAPH.H`, one global `G`). Windows
callbacks are inline `_export` hook functions in `BORG.H` that forward into the `UI` object.
`UI.CPP` is ~100KB and is explicitly excluded from the thesis code listing (see the header
comment in `TASK.H`); many `taskEnvironment` methods exist only as hook functions for it.

Globals are constructed in `BORG.CPP` in a required order: `G`, then `task`, then `UI` last.

### The `NETWORK` define

`#define NETWORK` at the top of `CFS.H` switches DLCS code in and out across *every* module.
Function signatures differ between the two builds (agent IDs are threaded through message
posting, classifier access, etc.), so the `#ifdef NETWORK` / `#else` branches are parallel
overloads, not additions. **Saved `.sim` files are not interchangeable between a NETWORK build
and a non-NETWORK build.** To disable DLCS at runtime instead, leave the define and set
`networkEnabled` to 0.

Three message kinds cross the simulated network (`NI.H`, `QUEUE.H`): classifier messages (share
high-strength rules), action messages (let one agent's action fire another's rule, enabling
cross-agent chaining), and payoff messages (bucket-brigade payment across agents).

### Conventions used throughout

- **Reset triad.** Nearly every class implements `reset(int firstReset=0)` (allocates/frees
  memory and initializes), `softReset()` (restore initial conditions for another run *without*
  discarding learned rules; performs **no** allocation, so parameters must not change between
  soft resets), and `static restoreDefaults()` (parameter values only, never allocation).
- **Static = shared setting, instance = per-agent state.** Parameters that must agree across all
  agents (message length, list lengths, probabilities, thresholds) are static members with
  `static getSettings()`/`setSettings()` fan-out pairs. `CFS.H`'s giant `get/setSettings` overloads
  are the aggregation point for the whole system; adding a tunable means threading it through
  there, through the matching dialog in `UI.CPP`, and through the batch-file `sscanf`.
- **Save/load triad.** `saveStatic`/`loadStatic` for shared statics, `save`/`load` for instance
  state, cascading down the ownership tree from `taskEnvironment`. Raw `fwrite` of binary
  structs — field order is the format.
- **Allocation.** `new` with `catch(xalloc)` calling the global `error(int, char*)`
  (`EI.CPP`), which forwards to `UI.displayError`.
- **Supplier identity.** Rules are paid by *unique rule number*, not list index, because rule
  discovery operators can delete a supplier mid-tick (`supplierSet` / `whatIsIndex` in
  `CLSSLIST`).

### Message and mask encoding

Messages are fixed-length allele strings (default 9, `DEF_MSG_LEN`) over `0`/`1`/`#`. Conditions
and actions are both `message` objects. Masks constrain what may appear at each position:
`0`/`1` force that value, `#` forces don't-care, `X` allows 0/1/#, `B` allows 0/1 only
(documented at `CLSSLIST.CPP` above `setMasks`). `TASK.H` sets the masks by BBA state — e.g.
`ACTION_MASK_NO_BBA "00BB00000"` pins all but the two actuator bits.

Animat encoding (`EI.CPP`): the sensor message is `GL GR OL OR` in alleles 0–3 (goal-left,
goal-right, obstacle-left, obstacle-right). The actuator command is alleles **2 and 3** — left
and right wheel on/off — which is what the active `ACTION_MASK_*` defines leave free. The
comment above `moveAnt()` saying the command is in the *last two* alleles is stale; it describes
the commented-out reversible-motor model (`00` stop, `01` left turn, `10` right turn, `11`
forward) that read `msg.size()-2`/`-1`. Trust the code and the mask, not that comment.

`postOutput()` sorts the message board by bid and drives the animat from the single
highest-bidding message; if that winner's rule strength is 0 and CEO is on, a CEO-generated
action replaces it first. Payoff (`adjustStrengths`) is shaped: reward for closing on the goal
and for improving heading, half-magnitude penalty otherwise, plus a crash penalty — paid only
to the supplier of message board slot 0.

Pass-through (`#` in an action meaning "copy from condition") is **not** supported; `#` in an
action means don't-care. An environment interface must implement pass-through itself if wanted.

## File formats

- `.sim` — binary simulation snapshot. 4-byte version string (currently `"2.1"`), then
  `task.saveStatic()`, then `task.save()` (`userInterface::save` in `UI.CPP`).
- `.rul` — rule list only (`saveRules`/`loadRules`).
- `.p` — binary agent position dumps.
- `.b` — batch script, plain text. Lines beginning with `;` are comments; the first data line is
  the starting simulation number; a line beginning with `*` ends the batch. Each remaining line
  is one whitespace-separated simulation spec parsed by a single `sscanf` at
  [UI.CPP:2367](UI.CPP#L2367) in three groups — system, agent, network. Boolean fields are the
  literal characters `Y`/`N`. **The field order in that `sscanf` is the format**; there is no
  other spec.
- `.O` — batch output, a fixed-column text table (sim #, agent, seed, iterations, sexual and
  asexual crossovers, mutations, CDOs, CEOs, crashes), written per completed run.
- `BORG.INI` — display preferences only (`readINI`/`writeINI`).

## Things to know before changing code

- Several parameters are declared but explicitly **NOT USED** and must stay at their documented
  values: multiple conditions (`numConditions` = 1), `BBAstyle`, `TCOenabled`, bid tax, producer
  tax, `eliteThresh`. They exist as scaffolding for unimplemented features.
- Probabilities have 2-decimal resolution; don't set one below 0.01 (`CLSSLIST.H`).
- `getRandom()` in `CFS.H` counts calls in the global `numRandCalls` — reproducibility of a
  seeded run depends on the exact sequence of random draws, so reordering calls changes results
  even when the logic is equivalent.
- Resource IDs live in `BORG.H` and must stay in sync with `BORG.RC`.
- The non-`NETWORK` branch of `environmentInterface::postOutput` has a typo (`&suppport`,
  [EI.CPP:391](EI.CPP#L391)) and will not compile. Only the DLCS build is currently buildable.
