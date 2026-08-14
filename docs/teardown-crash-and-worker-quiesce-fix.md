<!--
Arkheon Technologies
Proprietary and Confidential.
Unauthorized copying of this file, via any medium, is strictly prohibited.
© Arkheon Technologies. All rights reserved.
-->

# The teardown crash, and the fix I recommend

**Date:** 2026-08-11. **Author's note:** written to be verified. §7 lists what a correct diff must
contain and §8 lists the diffs that would look correct and be wrong.

**Read this first:** there are **two separate defects** in this document and they are routinely
conflated, including by the fix plan this investigation started from.

| | What it is | Who owns it | Is it the crash? | Fix here? |
|---|---|---|---|---|
| **Issue A** | `n8ro-sim-local.exe` exits `0xC0000005` during host plugin teardown | Arkheon platform | **Yes** | **No** — no host source in this tree |
| **Issue B** | `ai-commander` frees memory and code a live worker still holds | this repository | **No** | **Yes** — §5 |

**Fixing Issue B will not change the exit code.** Any verification that gates on `%errorlevel% == 0`
will fail on a correct Issue-B diff. §9 gives the acceptance criteria that actually apply.

---

## 1. Issue A — the crash

`bin\n8ro-sim-local.exe --scenario "Mariana Shield" --run-ms 60000` completes every frame, logs
`run end`, resets the scenario, logs `Simulation engine runLoop exiting`, and then exits
`-1073741819` (`0xC0000005`) during final teardown.

Ruled out by controlled substitution — nine runs, one variable each (full matrix in
`teardown-av-2026-08-11.md`):

| Ruled out | By |
|---|---|
| `ai-commander`'s non-drained worker (the original hypothesis) | faults with `commander.enabled=false` and `requested=0` — no worker is ever constructed |
| `ai-commander` at all | faults with the DLL absent; `sim-scripting.dll` alone reproduces it |
| the hosted/HTTP path | same run made no request; `sim-scripting` opens no socket |
| the `userPlugins/sim` load path | faults identically when the plugin is loaded from `bin/plugins/sim` |
| the plugin's runtime workload | `sim-scripting` has no threads, no backend, no network |

What remains: **the host's plugin teardown, conditioned on which plugin is loaded.** That is a
location, not a cause. The leading candidate is that `IScriptingPluginService::missionRegistrar()`
retains plugin-supplied `std::function` callbacks — `C:\N8RO\CLAUDE.md` says so in as many words,
instructing authors to capture `ScriptingApiContext` **by value** because *"the registrar keeps the
callback"* — and `plugin/IPlugin.h` offers no unregister hook, so a plugin cannot withdraw them. If
the DLL is unmapped before the registrar is destroyed, destroying those callables runs a destructor
at an unmapped address. Both faulting plugins register namespaces; `n8ro-skyfeed`, which never
faults, registers none.

**This is a candidate, not a finding.** No stack was obtained (no `cdb`/`windbg` on the host; WER
`LocalDumps` needs elevation). Static-destruction order and `FreeLibrary`-with-live-references
predict the same symptom and are not excluded.

### 1.1 Recommended action for Issue A

**File it with Arkheon; change no plugin code for it.** There is no mitigation available from this
side — relocating the plugin to `bin/plugins/sim` was the only no-rebuild candidate and run 9
disposes of it. What Arkheon needs to establish, cheapest first:

1. **A stack**, from an elevated shell:
   `HKLM\SOFTWARE\Microsoft\Windows\Windows Error Reporting\LocalDumps` with `DumpFolder` and
   `DumpType=2`, then re-run. Ends the argument outright.
2. **Remove registration, keep the plugin** — rebuild the `sim-scripting` sample with its
   `registerWith` call deleted, nothing else changed. Exit `0` confirms the mechanism.
3. **Add registration to the clean plugin** — have `n8ro-skyfeed` register one trivial Lua function.
   A fault there is the stronger direction: it turns a correlation into a manipulation.

If confirmed, the host fix is an ordering constraint, not an algorithm: **the scripting registrar,
and anything else holding plugin-supplied callables, must be destroyed before the plugin DLLs are
unloaded** — or `IPlugin` must gain an unregister hook that `shutdown()` can call while its own code
is still mapped.

---

## 2. Issue B — what is actually wrong in this repository

The worker lambda at `src/AiCommanderPlugin.cpp:719` captures two **raw pointers into plugin-owned
memory**:

```cpp
auto work = [snapshot, prompt, client, slot, prefixLength, defaultOrbitRadiusM]() mutable {
    (void)slot->publish(CommanderRuntime::runWorkerCall(
        std::move(snapshot), prompt, *client, prefixLength, defaultOrbitRadiusM));
};
threadRunner_->submitBackgroundTask(std::move(work));
```

- `client` is `ILlmClient*`, owned by `CommanderRuntime::client_` (a `std::unique_ptr`).
- `slot` is `&state->completed`, owned by `CommanderRuntime::entities_`
  (`std::map<std::string, std::unique_ptr<EntityCommandState>>`).

Nothing waits for that worker before either owner is destroyed. Four sites free it out from under a
live worker:

| # | Site | What it frees | Reachable from |
|---|---|---|---|
| B1 | `onStop()` → `runtime_.reset()` → `entities_.clear()` | the `slot` | every scenario stop |
| B2 | `releaseCommand()` → `entities_.erase()` | the `slot` | mission Lua, mid-run |
| B3 | `rebuildBackend()` → `setClient()` | the `ILlmClient` | any `applyConfigFields` changing backend |
| B4 | `shutdown()`, then `~AiCommanderPlugin` | both, then the DLL's code | process teardown |

**The window is real, not theoretical.** `logs/ai-commander/orders.jsonl` from the 600 s soak
records `order.requested RedSu35_01 serial=8` at `t=600.05` with no matching `order.accepted` or
`order.rejected` — a request genuinely in flight when the run ended.

### 2.1 The constraint that shapes the fix

**An in-flight request cannot be cancelled.** `ILlmClient::request()` (`include/ILlmClient.h:107`)
is blocking and takes no cancellation token, and the SDK's `IHttpClient::send()`
(`core/net/IHttpClient.h:59`) is the same — its only bound is `HttpRequest::timeoutS`, which this
plugin sets from `commander.requestTimeoutS` (**90** in the deployed config).

So a cancellation flag can only be honoured *before* a worker enters `request()`. A worker already
inside it runs to the HTTP timeout, and **worst-case drain latency is `requestTimeoutS`.** Any design
that blocks unconditionally for that long on scenario stop stalls the engine's stop path for a minute
and a half. §5 splits the budget accordingly.

---

## 3. Why the obvious fix is insufficient

Making the captures `shared_ptr` keeps the *data* alive. It does not help at **B4**, where the host
calls `FreeLibrary` and the worker's **code** — the lambda body, `runWorkerCall`,
`LatestWinsSlot::publish`, the `shared_ptr` destructors themselves — is unmapped. Extending object
lifetime cannot save a call into an unmapped text segment.

Conversely, a drain alone is insufficient at **B1–B3**, where the drain may legitimately time out and
the plugin must then *not* free what a live worker holds.

**The fix therefore needs both**, and they cover different sites:

- **Shared ownership** makes B1, B2 and B3 safe outright — an erased or replaced object simply
  outlives the map entry.
- **A bounded drain** is what B4 needs, because only a worker that has actually finished is safe
  against `FreeLibrary`.

---

## 4. Files this touches

| File | Change |
|---|---|
| `include/WorkerGate.h` | **new** — the outstanding-count gate |
| `src/WorkerGate.cpp` | **new** |
| `include/CommanderRuntime.h` | `client_` and the `entities_` mapped type become `shared_ptr`; `client()` returns `shared_ptr` |
| `src/CommanderRuntime.cpp` | follow the type changes; `setClient` takes `shared_ptr` |
| `include/AiCommanderPlugin.h` | holds `std::shared_ptr<WorkerGate> gate_` |
| `src/AiCommanderPlugin.cpp` | dispatch acquires a lease and captures shared handles; `onStop()` and `shutdown()` drain |
| `tests/WorkerGateTests.cpp` | **new** |
| `tests/ai-commander-tests.vcxproj` | add `..\src\WorkerGate.cpp` and `WorkerGateTests.cpp` |
| `ai-commander.vcxproj` | add `..\src\WorkerGate.cpp` |

Every file created or edited keeps the Arkheon proprietary header.

---

## 5. The recommended fix

### 5.1 `WorkerGate`

A counted gate whose lease is **copyable**, because `IThreadRunner::submitBackgroundTask` takes
`std::function<void()>` and `std::function` requires its target to be *CopyConstructible*. A
move-only RAII lease will not compile into it. `std::shared_ptr<void>` with a custom deleter is a
copyable RAII decrement:

```cpp
class WorkerGate : public std::enable_shared_from_this<WorkerGate> {
public:
    // Returns null once the gate is closed, which is how a late dispatch declines to start work.
    // The returned handle is copyable so it can live inside a std::function; the count drops when
    // the last copy dies, which happens whether the worker returned or threw.
    [[nodiscard]] std::shared_ptr<void> acquire();

    // Refuses new leases. Does not wait — a caller that must not block still stops the bleeding.
    void close();

    // Blocks until the outstanding count reaches zero or the budget expires. Returns true when it
    // reached zero. NEVER call while holding a lock a worker needs.
    [[nodiscard]] bool drain(std::chrono::milliseconds budget);

    [[nodiscard]] int outstanding() const;

private:
    void release();
    mutable std::mutex mutex_;
    std::condition_variable idle_;
    int outstanding_ = 0;
    bool closed_ = false;
};
```

`acquire()` captures `shared_from_this()` in the deleter. That is deliberate: **if a worker outlives
the plugin, it keeps the gate alive rather than decrementing into freed memory.**

### 5.2 Dispatch

The lease is taken **on the simulation thread, before `submitBackgroundTask`** — not inside the
worker. A task that is queued and never runs must still count, or the drain races with the queue.

```cpp
std::shared_ptr<void> lease = gate_->acquire();
if (lease == nullptr) {
    return;   // Shutting down. Not an error, and not a rejection to count.
}
std::shared_ptr<ILlmClient> client = runtime_.client();
std::shared_ptr<EntityCommandState> state = ...;   // shared, from the roster

auto work = [snapshot, prompt, client, state, prefixLength, defaultOrbitRadiusM, lease]() mutable {
    (void)state->completed.publish(CommanderRuntime::runWorkerCall(
        std::move(snapshot), prompt, *client, prefixLength, defaultOrbitRadiusM));
};
```

The existing comment above this lambda explains what crosses by value and why. It stays, extended to
say that `client` and `state` are now shared handles rather than raw pointers, and that `lease` is
what makes the count outlive the queue. **The comment is load-bearing; do not delete it to make room
for the new captures.**

### 5.3 The two drains, with different budgets

The asymmetry is the point, and it follows from §2.1 and §3.

**`onStop()` — short budget, safe on timeout.** The DLL stays mapped; shared ownership already makes
a timeout harmless. Drain briefly so the counters and the order log are complete, then proceed
regardless:

```cpp
void AiCommanderPlugin::onStop() {
    // A worker inside client.request() cannot be interrupted (ILlmClient has no cancellation), so
    // this is a courtesy drain, not a guarantee: it collects a request that is nearly done and
    // gives up on one that is not. Giving up is SAFE here and only here - the roster entries and
    // the client are shared handles, so a worker that outlives this call holds them alive and
    // publishes into a slot nobody is reading. The DLL is not being unloaded; shutdown() is where
    // that matters, and it waits properly.
    if (!gate_->drain(std::chrono::seconds(2))) {
        N8RO_LOG_INFO(
            std::string("ai-commander: ") + std::to_string(gate_->outstanding())
                + " request(s) still in flight at scenario stop; their results are discarded. "
                  "Bounded by commander.requestTimeoutS.",
            kLogCategory);
    }
    writeRunEndStats();
    ...
    runtime_.reset();
}
```

**`shutdown()` — long budget, and the only place a timeout is dangerous.** This is the one site
`FreeLibrary` follows, so the wait must actually succeed:

```cpp
void AiCommanderPlugin::shutdown() {
    if (!shutdown_) {
        writeRunEndStats();
    }
    shutdown_ = true;

    // Close first, then wait. Closing alone stops new work; waiting is what makes the DLL safe to
    // unload, because a worker's CODE lives in this image and no amount of keeping its DATA alive
    // survives FreeLibrary. Budget is requestTimeoutS plus margin because a worker already inside
    // client.request() cannot be interrupted - that is the adapter's bound, and it is the honest
    // worst case rather than a guess.
    gate_->close();
    const auto budget = std::chrono::seconds(runtime_.config().requestTimeoutS + 5);
    if (!gate_->drain(budget)) {
        // Deliberate leak, and it must stay deliberate. Releasing the runtime now would free the
        // client a live worker is inside. A leaked buffer at process exit is invisible; a freed one
        // is a crash with our name on it. The gate outlives us by design - acquire() holds a
        // shared_from_this in every lease.
        N8RO_LOG_ERROR(
            std::string("ai-commander: ") + std::to_string(gate_->outstanding())
                + " worker(s) did not finish within " + std::to_string(budget.count())
                + "s of shutdown. Plugin state is INTENTIONALLY LEAKED rather than freed beneath "
                  "them. If the host unloads this DLL now, the process may fault in unmapped code - "
                  "report this line rather than the fault.",
            kLogCategory);
        leaked_ = true;
        return;   // Null nothing, release nothing.
    }

    runtime_.recorder().close();
    entityManager_ = nullptr;
    threadRunner_ = nullptr;
    world_.reset();
}
```

### 5.4 What shared ownership alone fixes

**B2 and B3 need no drain and no new call.** Once `entities_` holds `shared_ptr<EntityCommandState>`
and `client_` is a `shared_ptr<ILlmClient>`, `releaseCommand()`'s `erase` and `setClient()`'s
replacement simply drop one reference; the worker's copy keeps the object alive until it returns. The
comment at `AiCommanderPlugin.cpp:850` — correct that the in-flight *result* is discarded by the
serial check, silent about the *pointer* — should gain a sentence saying the pointer is now safe too.

---

## 6. What must not change

- Order schema, Stage-A and Stage-B validation, cadence, fallback ladder, config field names,
  counters, the order-log record format.
- The egress gate (`claude.enabled`) and the spend ceiling.
- `commander.requestTimeoutS` — it is read as the drain budget, but **changing its value is a
  behaviour change and is out of scope.** 90 s is sized for a cold Ollama load and documented as
  such.
- The by-value capture discipline at the dispatch site, and its comment.
- `CommanderRuntime::runWorkerCall` stays `static` and free of runtime state.

---

## 7. Verification checklist — what a correct diff contains

A reviewer should be able to tick every line.

1. `WorkerGate::acquire()` returns a **copyable** handle. If the diff introduces a move-only RAII
   type and puts it in the lambda, it does not compile against `std::function`.
2. The lease is acquired **on the simulation thread before `submitBackgroundTask`**, not inside the
   worker body.
3. The lease is **captured by value** in the lambda, alongside the existing captures.
4. `acquire()` returns null when closed, and the dispatch **returns without counting a request or a
   rejection** in that case.
5. `client_` and the `entities_` mapped type are `shared_ptr`; the worker captures those handles, not
   `&state->completed` and not `runtime_.client()` raw.
6. `onStop()` drains with a **short** budget **before** `runtime_.reset()`, and proceeds on timeout.
7. `shutdown()` calls `close()` **then** `drain()`, and on timeout **returns without nulling
   `entityManager_`, `threadRunner_`, or resetting `world_`,** and without closing the recorder.
8. The timeout path logs at **ERROR** and names the outstanding count.
9. `drain()` is never called while a mutex a worker needs is held.
10. Every touched file keeps the Arkheon header; the dispatch-site comment survives and is extended.
11. `WorkerGate.cpp` is added to **both** `.vcxproj` files.
12. The new test submits real work on a real thread, calls the drain, and asserts ordering — see §9.

---

## 8. Diffs that would look correct and be wrong

These are the failure modes I would expect, and each is worth checking explicitly.

1. **Fixing only `shutdown()`.** The plan this work started from named only that site. It leaves B1
   — `onStop()` → `reset()` — which is the window the 600 s soak actually hit, on every scenario
   stop.
2. **`shared_ptr` without a drain.** Keeps the data alive, does nothing about `FreeLibrary`
   unmapping the worker's code. This is called out in §3 because it is the intuitive fix and it is
   incomplete.
3. **A drain without shared ownership.** Correct while the drain succeeds; on timeout it frees the
   client and slot beneath a live worker — converting a rare hang into a rare crash.
4. **An unbounded wait** (`while (outstanding) {}`, or `waitForAll()` on the host's runner). A worker
   stuck on a socket hangs process exit forever. Note `IThreadRunner::waitForAll()` also waits on
   *host* tasks, which is not ours to block.
5. **Freeing anyway after logging the timeout.** The log line is not the fix; the `return` is. A diff
   that logs loudly and then nulls the members has implemented the crash with a warning attached.
6. **Draining inside `dispatchRequests()` or `onTickFrame()`.** Blocks the simulation thread on a
   frame. The frame-cost budget (p95 ≈ 0.006 ms today) is a Success Metric.
7. **A 90 s drain in `onStop()`.** Correct but unusable — it stalls every scenario stop for up to a
   minute and a half. The asymmetry in §5.3 is deliberate and a diff that "simplifies" it to one
   budget has regressed the stop path.
8. **Cancelling an in-flight request.** There is no such capability (§2.1). A diff claiming to abort
   a running `request()` has either changed `ILlmClient` — out of scope — or is not doing what it
   says.
9. **Deleting the dispatch-site comment** to make room. It is correct and load-bearing.

---

## 9. Acceptance criteria

**The headless repro will still exit `-1073741819`.** That is Issue A, it reproduces with this plugin
absent, and it is not evidence against the diff. Do not gate on it.

| Check | Expected |
|---|---|
| `ai-commander-tests.exe` | all green, including the new gate tests |
| New test: drain ordering | a worker holding a lease for ~200 ms; the drain must return **after** the worker sets its done-flag, asserted on the flag and not on elapsed time |
| New test: bounded timeout | a worker holding a lease longer than the budget; `drain()` returns `false` inside the budget rather than blocking |
| New test: closed gate | `acquire()` returns null after `close()`, and the count never rises |
| New test: exception safety | a worker that throws still decrements |
| `tests\smoke\run-smoke.ps1 -ReleaseRoot C:\N8RO` | zero failures; the script counts its own checks and prints `checks : N` — compare N against a pre-change run rather than against a number quoted here |
| 60 s headless run, `backend=claude` | `requested=6 accepted=6 rejected=0`, unchanged |
| Frame cost | p95 within noise of 0.006 ms — the drain must not touch the frame path |
| Log at a clean stop | no timeout line at INFO or ERROR |
| Stale sentinel `logs\n8ro-logger-n8ro-sim-local.log.running` | still present — Issue A is untouched, and pretending otherwise would mean the diff did something out of scope |

**A note on the test's honesty.** `AiCommanderPlugin.cpp` is not in
`tests/ai-commander-tests.vcxproj`, and driving the real `shutdown()` would require a fake
`IThreadRunner` implementing ~35 pure virtuals plus a `PluginContext` the suite has no way to build.
The tests above therefore exercise **`WorkerGate`, the component `shutdown()` delegates to** — not
`AiCommanderPlugin::shutdown()` end to end. A diff whose test claims otherwise is overstating what it
proves.

---

## 10. Recommendation on sequencing

**Do not land this before the demo.** The deployed DLL is soaked — a 240-order run, the 2026-08-05
hosted run, and today's 6/6/0 — and a rebuilt one is not. Issue B is latent, has never been observed
to fire, and cannot produce the symptom that prompted the investigation. Rebuilding now trades a
proven binary for an unproven one and removes no crash.

Land it after. Rollback for the deployed binary is one command:

```powershell
Copy-Item 'C:\N8RO\demo-backup\ai-commander-KNOWN-GOOD.dll' `
          'C:\N8RO\userPlugins\sim\ai-commander.dll' -Force
```
