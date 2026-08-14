# Registering the self-hosted runner

`ci-selfhosted.yml` carries everything that needs the SDK: the `Release | x64` build, the exports
check, the 67-test suite, the AddressSanitizer run, and the deployed-artifact smoke. None of it can
run on a GitHub-hosted runner — those have no SDK headers, no `n8ro-*.lib`, and no Visual Studio
2026 — so until a runner is registered, **nothing in this repository is compiled by CI at all.**

That is the current state, and it is deliberately visible rather than hidden: the workflow's
`pull_request` and `push` triggers are commented out, so it does not queue a job that can never
start. A perpetually-pending check looks like a slow build at a glance and blocks branch protection.

> ### ⚠ A consequence of the above that the PRD stated wrongly for eleven revisions
>
> **§Corrections item 56 repaired `tests/live/`, a harness that had not compiled since v1.8.28, and
> closed with *"the root cause is fixed for good: `tests/live/` now builds in CI."*** That build step
> is in **this** workflow — **so it has never run.** The compile guard for the instrument behind a
> green metric **has never fired once**, while the PRD described it as closed.
>
> **That is the same shape as the defect it was written to fix** — *a guard that reads as a guard and
> is not one.* Corrected at **PRD v1.8.51, §Corrections item 67**, which corrects the **claim**
> rather than buying the runner.
>
> **Registering a runner was considered and declined**, and the reasoning belongs here rather than
> only in the PRD:
>
> - At maintenance, with one developer on one machine, a runner on **this** box executes against
>   **the same Visual Studio 2026, the same `C:\N8RO` tree and the same toolchain that already builds
>   locally.** It automates *the running*, not *the environment* — which is most of what a runner
>   buys a team and very little of what it buys a single developer.
> - Against that it carries the standing exposure this document already names below: **an agent
>   executing pull-request code on a machine that holds a licensed install.**
>
> **The build evidence is real; it is simply local.** As of 2026-08-10: unit suite **169/169**, the
> DLL builds and auto-deploys, `dumpbin` shows all three required exports.
>
> **Register a runner when a second contributor appears, or when you come back to this after a gap** —
> the workflow is written and the steps below are current. **Until then, do not read a green
> `CI (hosted)` badge as evidence that anything compiles.**

## What the runner machine needs

| Requirement | Why |
|---|---|
| A **licensed N8RO release** installed, with `setup.cmd` at its root | Every build path resolves through `$(N8RO_RELEASE)`; the repository deliberately vendors no SDK |
| **Visual Studio 2026 (v18.x)** with the C++ x64 workload | The project pins `PlatformToolset v145`, which ships only with VS 2026. VS 2022's v143 will not build it |
| Windows x64 | The plugin is a Windows DLL |

Set `N8RO_RELEASE_ROOT` in the workflow's `env:` block if the tree is not at `C:\N8RO`.

## Steps

1. **Register the runner** — repository → Settings → Actions → Runners → New self-hosted runner,
   and follow the generated commands on the target machine.

2. **Apply the labels the workflow selects on.** All three are required:

   ```
   self-hosted, windows, n8ro-release
   ```

   `n8ro-release` is the load-bearing one. `self-hosted` and `windows` say where the job runs;
   `n8ro-release` promises a licensed install is present. The workflow's first step verifies that
   promise and fails with an explicit "this runner is mislabelled" error rather than producing a
   confusing compiler error twenty steps later.

3. **Uncomment the triggers** at the top of `ci-selfhosted.yml`:

   ```yaml
   on:
     pull_request:
     push:
       branches: [main]
     workflow_dispatch:
   ```

4. **Run it once manually first** — Actions → CI (self-hosted) → Run workflow — before enabling the
   automatic triggers, so a runner misconfiguration surfaces on a run you are watching rather than
   on someone else's PR.

## Two things to know before you register

**The release tree is shared with interactive use.** The workflow deploys the built plugin into
`%N8RO_RELEASE_USER_SIM_PLUGINS%` because that is what the post-build event does and what the smoke
test needs. Its final step deletes the deployed DLL again, including on failure — otherwise the next
scenario anyone ran interactively on that machine would silently load a PR's build. If you point the
runner at a tree you also use for demos, that cleanup is the only thing standing between a branch
build and a live run.

**A self-hosted runner executes code from pull requests.** On a private repository with only trusted
contributors that is normal practice. It is worth stating anyway: this is a standing exposure on a
machine that holds a licensed install, and it is a different risk from anything else in this
repository.

> ### ⚠ PUBLICATION INVERTS THE PARAGRAPH ABOVE — read this before registering anything
>
> **Added PRD v1.8.58.** Everything on this page was written for a **private** repository, and the
> sentence directly above names the premise it rests on: *"on a private repository with only trusted
> contributors."* **Publication was authorized at PRD v1.8.56 on 2026-08-14.** The moment visibility
> flips, that premise is false, and the conclusion it supports does not survive it.
>
> On a **public** repository, a self-hosted runner with a `pull_request` trigger means **any stranger
> on the internet can propose code that executes on a machine holding a licensed N8RO install** — the
> release tree, the SDK, the terrain and AI databases, and whatever else that box carries. This is
> the single most consequential configuration change available in this repository, and it is
> available by uncommenting two lines.
>
> **The decision therefore moves rather than merely standing.** The original reasoning — *"a runner
> on this box automates the running, not the environment"* — was a cost-benefit argument at
> maintenance with one developer. It did not price a public attack surface, because there wasn't one.
>
> **If you register a runner after publication, all three of these are required, not advisory:**
>
> 1. **The fork guard stays.** `ci-selfhosted.yml`'s job carries
>    `github.event.pull_request.head.repo.full_name == github.repository`, so a fork's PR is skipped
>    rather than run. It is already in place and inert while the triggers are commented.
> 2. **Never add `pull_request_target` to this workflow.** It runs with repository write scope
>    against a PR's content and is the standard way self-hosted runners get compromised.
> 3. **Require approval for all outside contributors** — Settings → Actions → *"Require approval for
>    all external contributors"*. The fork guard is the belt; this is the braces.
>
> **The safe default, and the recommendation, is narrower than any of that:** leave this workflow on
> `workflow_dispatch` and `push: branches: [main]` only. Post-merge compilation on trusted code buys
> most of what a runner is for here, and buys it without ever executing a proposal from a stranger.
>
> **Until a runner exists, the original guidance is unchanged and still correct:** do not read a
> green `CI (hosted)` badge as evidence that anything compiles.

## Badges

If a build badge is added to `README.md`, it must say which runner produced it. A green **CI
(hosted)** badge means the PRD linted, no classified artifact was committed, and the Lua parses —
**it does not mean the plugin compiles.** Only **CI (self-hosted)** substantiates that. The PRD
states this obligation in §Source control and repository.
