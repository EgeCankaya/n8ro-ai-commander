<!--
Arkheon Technologies
Proprietary and Confidential.
Unauthorized copying of this file, via any medium, is strictly prohibited.
© Arkheon Technologies. All rights reserved.
-->

# `n8ro-sim-local.exe` process-teardown access violation — a host defect, not a plugin defect

**Status:** the fault is **reproduced, isolated, and attributed to the host**. It occurs with
`ai-commander` disabled, and it occurs with `ai-commander` not loaded at all. No plugin change is
proposed and none was made.
**For:** Arkheon platform engineering. Release `n8ro@2.1.144`
(`n8ro-core@0.1.90`, `n8ro-schema@1.0.43`, `n8ro-data@2.0.80`, `n8ro-sim@2.0.140`), tree `C:\N8RO`,
Windows 11 Pro 10.0.26200.
**Date:** 2026-08-11.
**Cost:** one 60 s hosted run (six Haiku 4.5 orders) for the as-configured baseline. **Every other
run in this document was made with the commander asserted off or the plugin absent — no inference,
no network, no egress.**

> **Why this document exists.** The defect arrived with a written fix plan that attributed it to a
> non-drained background worker in `AiCommanderPlugin::shutdown()`. That attribution is wrong, and
> it is wrong in a way that would have cost a demo: acting on it means replacing a soaked plugin
> binary with an unsoaked one, on the eve of a recording, against a symptom the change cannot
> remove. §4 records the three independent refutations. §8 records the defects the same reading
> *did* find in the plugin, which are real, are not this crash, and remain unfixed on purpose.

---

## 1. The defect in one paragraph

`bin\n8ro-sim-local.exe --scenario "Mariana Shield" --run-ms 60000` runs to completion — all 1,201
frames, scenario reset, plugin `onStop()`, `Simulation engine runLoop exiting` — and then the
process exits `-1073741819` (`0xC0000005`, access violation) during final teardown. **The fault
tracks which plugin is loaded, and neither the directory it was loaded from nor what it does while
running.** A plugin with a network backend and a thread pool (`ai-commander`) and a plugin with
neither (`sim-scripting`, the shipped SDK sample) each reproduce it alone; `ai-commander` reproduces
it identically when relocated to `bin/plugins/sim`, the directory `n8ro-skyfeed.dll` runs clean from.
With no such plugin loaded the same command exits `0`, three consecutive times, while
`n8ro-skyfeed.dll` stays loaded throughout. The one property that separates the two faulting plugins
from the clean one is that both register mission-scripting namespaces and skyfeed registers none —
see §5, which states plainly that this is a correlation over three plugins and not a proven
mechanism.

---

## 2. Provenance — what was read, and how

| Source | What it gave |
|---|---|
| Eight runs of the repro command, 2026-08-11 08:47Z–09:17Z | the exit codes in §3, each captured as `%errorlevel%` immediately after the process returned |
| `C:\N8RO\logs\n8ro-logger-n8ro-sim-local.log` and its `.crash-2026081*` rotations | which plugins loaded per run, the frame count, and the run-end counters |
| `C:\N8RO\logs\ai-commander\orders.jsonl` | the request/accept timeline of the failing 08:04Z baseline run — decisive for §4.3 |
| `C:\src\n8ro-ai-commander\src\AiCommanderPlugin.cpp`, `CommanderRuntime.{h,cpp}`, `OrderSlot.h` | the dispatch and teardown paths the original hypothesis named |
| `C:\N8RO\include\n8ro-core\{plugin/IPlugin.h,core/threading/IThreadRunner.h}` | the host's lifecycle and thread-runner contracts |

The release tree was returned to its exact prior state after the last run: `ai-commander.cfg` is
byte-identical to its backup, `bin/plugins/sim` holds `n8ro-skyfeed.dll` and nothing else, and the
deployed `userPlugins/sim/ai-commander.dll` still hashes equal to
`demo-backup\ai-commander-KNOWN-GOOD.dll`. Run 9 temporarily placed a copy of the known-good DLL in
`bin/plugins/sim`; it was deleted immediately afterwards. Plugins were disabled by renaming rather
than deleting throughout.

---

## 3. The isolation matrix

One variable changed per run — which plugins were loaded, and from where — with an identical command
line each time. `bin/plugins/sim/n8ro-skyfeed.dll` was loaded in **every** run, including the clean
ones.

| # | plugins loaded (besides skyfeed) | commander | exit code | crash log written |
|---|---|---|---|---|
| 1 | `ai-commander` + `sim-scripting`, from `userPlugins/sim` | enabled, `requested=6 accepted=6` | `-1073741819` | `…crash-20260811T080518Z.log` |
| 2 | `ai-commander` + `sim-scripting`, from `userPlugins/sim` | enabled, `requested=6 accepted=6` | `-1073741819` | `…crash-20260811T084851Z.log` |
| 3 | `ai-commander` + `sim-scripting`, from `userPlugins/sim` | **`commander.enabled=false`**, `requested=0` | `-1073741819` | `…crash-20260811T085030Z.log` |
| 4 | `sim-scripting` only, from `userPlugins/sim` | n/a — plugin absent | `-1073741819` | `…crash-20260811T085216Z.log` |
| 5 | *(none)* | n/a | **`0`** | none |
| 6 | `ai-commander` only, from `userPlugins/sim` | enabled, `requested=6 accepted=6` | `-1073741819` | `…crash-20260811T085522Z.log` |
| 7, 8 | *(none)* | n/a | **`0`**, **`0`** | none |
| 9 | `ai-commander` only, **from `bin/plugins/sim`** | enabled, `requested=6 accepted=6` | `-1073741819` | pending — see below |

Run 1 is the operator's own baseline; runs 2–9 are this investigation. The crash-log column is an
independent check rather than a restatement: the host rotates the run log to a `.crash-*` copy after
a faulting exit, and it did so for the faulting runs and for none of the three clean ones.

**Run 9 exists to kill a hypothesis this document previously carried.** Runs 1–8 are equally
consistent with "the fault is in the `userPlugins/sim` load path" — every faulting run had loaded a
plugin from there, and skyfeed, which never faults, loads from `bin/plugins/sim`. Run 9 removes the
directory as a variable: `ai-commander` was placed in `bin/plugins/sim` next to skyfeed, with
`userPlugins/sim` empty, and the run log confirms
`Plugins loaded from C:\N8RO\bin\plugins\sim (loaded: 2, discovered: 2)` and no user-plugin
directory read at all. It faulted. **The load path is exonerated; the fault follows the plugin.**

**Note for the record: the crash log is written by the *next* run, not by the faulting one.** The
original report stated that no crash log was produced, and checked immediately after a crash that is
exactly what you see. The logger writes a sentinel `n8ro-logger-<app>.log.running` containing the
run's start timestamp and deletes it on a clean exit; on startup it finds a stale sentinel and only
*then* rotates the previous log to `…log.crash-<end-timestamp>.log`. So each faulting run's crash log
materialises when the engine is next started — which is why run 9's, the last run made here, does not
exist yet.

Two consequences worth having:

- **The stale sentinel is a cheaper crash detector than the exit code.** After run 9,
  `C:\N8RO\logs\n8ro-logger-n8ro-sim-local.log.running` still exists and contains `20260811T091532Z`,
  its own start time. A clean run leaves none.
- **The `.crash-*` file carries no stack trace.** It is a verbatim copy of the run log, so it
  establishes *that* a run faulted and nothing about where.

---

## 4. What this refutes

The hypothesis under test was: `AiCommanderPlugin::shutdown()` nulls `threadRunner_` without
draining, so a worker dispatched on the final cadence cycle is still running at teardown and
dereferences a freed `ILlmClient*`, a freed `LatestWinsSlot*`, or code in an unloaded DLL. Three
independent lines of evidence refute it.

### 4.1 The fault survives the commander being off

Run 3 set `commander.enabled=false`. `CommanderRuntime::isOperational()` is then false,
`dispatchRequests()` returns at its first statement, and the run-end counters confirm the
consequence: `requested=0 accepted=0 rejected=0` over 1,201 frames. **No worker lambda was ever
constructed**, so no `client` pointer and no `slot` pointer ever crossed a thread boundary and
`CommanderRuntime::runWorkerCall` was never entered. The process still faulted.

### 4.2 The fault survives the plugin being absent

Run 4 removed `ai-commander.dll` from the tree entirely — the run log shows
`Plugins loaded from C:\N8RO\userPlugins\sim (loaded: 1, discovered: 1)` and no `arkheon.aiCommander`
line anywhere. The process still faulted. Run 6 is the mirror image: `ai-commander` alone, no
`sim-scripting`, same fault. A defect that reproduces with the accused component removed is not that
component's defect.

### 4.3 Nothing was in flight at teardown in the failing baseline run

`orders.jsonl` for run 1 shows the last of six orders accepted at `t=43.85 s` against a run that
ended at `t=60.05 s`, with `requested=6 accepted=6` — every request dispatched, completed, and
drained on the simulation thread with **16 seconds to spare**. The timing argument in the original
plan (`cadenceS=20` fires a cycle at t≈60 that is still in flight at teardown) does not describe this
run: the t≈60 cycle never fired. Accumulated floating-point error left
`simTimeS - lastRequestSimTimeS` at 19.99999 s against a 20 s cadence — visible in the record as
`order.requested … t=40.14999999999986` followed directly by `commander.disabled … t=60.049999999998725`.

---

## 5. What the evidence does support

The fault is in the host's plugin teardown, and it is a property of **which plugin** is loaded:

- `n8ro-skyfeed.dll` is present in all nine runs, including the three clean ones. Loading *a* plugin
  is not sufficient to trigger the fault.
- `ai-commander` and `sim-scripting` are each sufficient on their own.
- The directory is not the variable (run 9), and neither is the plugin's runtime workload — run 3
  faulted with the commander disabled and no request issued.

**The one property that separates the faulting plugins from the clean one is scripting
registration**, and it is offered as a hypothesis rather than a finding: it is a correlation over
three plugins, which is thin. `ai-commander` and `sim-scripting` both register mission-scripting
namespaces through `IScriptingPluginService::missionRegistrar()`; `n8ro-skyfeed` registers none — it
spawns entities and logs no registration line. If the registrar retains `std::function` callbacks
whose code and captured state live in the plugin DLL, and the DLLs are unloaded before the registrar
is destroyed, then destroying those callbacks calls into unmapped memory and faults exactly here:
after `runLoop exiting`, with no plugin frame that any plugin author could fix. The plugin interface
as shipped (`plugin/IPlugin.h`) offers no unregister hook, so a plugin cannot withdraw its callbacks
even if it wanted to.

Two experiments would settle it, in this order:

1. **Remove the registration, keep the plugin.** Build the `sim-scripting` sample with its
   `registerWith` call removed and nothing else changed. Exit `0` confirms the mechanism; a fault
   refutes it and points at something every plugin does — static destruction order, or `FreeLibrary`
   against a DLL the host still holds references into.
2. **Add registration to the clean plugin.** If skyfeed registers a single trivial Lua function and
   then faults, the mechanism is confirmed from the other direction, which is the stronger result.

Neither was run here: both need a rebuild of a plugin this investigation had no mandate to change.

---

## 6. What was not established

**No stack was captured, and no dump was taken.** Both intended routes were unavailable on this
host, and the failure modes are worth recording so the next attempt does not repeat them:

- **Windows Error Reporting `LocalDumps`.** The key is under
  `HKLM\SOFTWARE\Microsoft\Windows\Windows Error Reporting\LocalDumps`. Writing it requires
  elevation; this session was not elevated. Any instruction to configure it needs an elevated shell.
- **A debugger.** There is no `cdb.exe` or `windbg.exe` on this machine.
  `C:\Program Files (x86)\Windows Kits\10\Debuggers\x64` exists but contains only `dbghelp.dll`,
  `dbgcore.dll`, `srcsrv.dll` and `symsrv.dll` — the libraries, not the tools.

Consequently this document does not name a faulting module, a faulting frame, or a thread count. It
narrows the defect by controlled substitution instead. **The isolation matrix is what stands; the
mechanism in §5 does not.**

---

## 7. Reproducing it

```bat
call C:\N8RO\setup.cmd
cd /d C:\N8RO
bin\n8ro-sim-local.exe --scenario "Mariana Shield" --run-ms 60000
echo %errorlevel%
```

`-1073741819` with either `ai-commander.dll` or `sim-scripting.dll` present — in `userPlugins/sim`
or in `bin/plugins/sim`, it makes no difference; `0` with neither. The run must be allowed to finish
— the fault is at teardown, so an interrupted run does not show it. No configuration, no API key and
no network are needed to see it: `sim-scripting.dll` alone reproduces it, and that plugin makes no
outbound connection.

---

## 8. Separately — three lifetime defects in `ai-commander`, deliberately unfixed

These were found while reading the dispatch path to test the hypothesis. They are real. **None of
them is this crash**, and none is fixed in the deployed binary. They are recorded here so the
distinction is on the record rather than in someone's memory.

The worker lambda at `src/AiCommanderPlugin.cpp:719` captures two raw pointers into plugin-owned
memory — `client` (`ILlmClient*`) and `slot` (`LatestWinsSlot<CandidateOrder>*`) — and nothing waits
for the worker before that memory is freed:

1. **`onStop()`** calls `runtime_.reset()`, which clears `entities_` and so destroys the
   `EntityCommandState` that owns the `slot` the worker publishes into. The window is not
   theoretical: the 600 s soak in `orders.jsonl` dispatched
   `order.requested RedSu35_01 serial=8` at `t=600.05` and recorded no matching `order.accepted` or
   `order.rejected` — a request genuinely in flight when the run ended.
2. **`releaseCommand()`**, reachable from mission Lua, erases the same state mid-run.
3. **`rebuildBackend()` → `setClient()`** frees the `ILlmClient` a worker may be inside `request()`
   on. The comment at `AiCommanderPlugin.cpp:850` is correct that the in-flight *result* is
   discarded by the serial check, and silent about the *pointer*.

`shutdown()` then nulls `threadRunner_` without draining and the destructor frees the rest, which is
what the original plan described. Any fix must **quiesce** — a bounded wait on an outstanding-task
count — rather than merely extend object lifetime with a `shared_ptr`: the worker's *code* lives in a
DLL the host unloads, and no amount of keeping the data alive helps once the text segment is gone.

They are unfixed because the schedule says so, not because they are acceptable. The deployed DLL is
soaked — a 240-order run, the 2026-08-05 hosted run, and today's 6/6/0 — and a rebuilt one is not.
Fixing a latent defect that cannot produce the observed symptom, in the last hours before a
recording, trades a proven binary for an unproven one and buys nothing.

---

## 9. Impact and disposition

- **The commander is unaffected.** Runs 1 and 2 both closed at `requested=6 accepted=6 rejected=0`
  with `applied 34 field(s)` and `backend=claude`, and frame cost held at p95 ≈ 0.006 ms.
- **Every run completes its work before the fault.** The order log, the run-end statistics and the
  `commander.disabled` record are all written and flushed before teardown begins; nothing is lost.
- **The exit code is wrong, and only the exit code.** A caller that gates on `%errorlevel%` — CI, a
  wrapper script, a scheduled run — will read a clean run as a failure. That is the cost worth
  quoting when this is prioritized.
- **There is no workaround by relocation.** Deploying the plugin to `bin/plugins/sim` was the one
  candidate mitigation that needed no rebuild, and run 9 disposes of it.
- **The release tree is as it was found** (see §2), and nothing was changed in this repository
  beyond the addition of this document.
