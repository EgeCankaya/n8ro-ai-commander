# Registering the self-hosted runner

`ci-selfhosted.yml` carries everything that needs the SDK: the `Release | x64` build, the exports
check, the 67-test suite, the AddressSanitizer run, and the deployed-artifact smoke. None of it can
run on a GitHub-hosted runner — those have no SDK headers, no `n8ro-*.lib`, and no Visual Studio
2026 — so until a runner is registered, **nothing in this repository is compiled by CI at all.**

That is the current state, and it is deliberately visible rather than hidden: the workflow's
`pull_request` and `push` triggers are commented out, so it does not queue a job that can never
start. A perpetually-pending check looks like a slow build at a glance and blocks branch protection.

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

## Badges

If a build badge is added to `README.md`, it must say which runner produced it. A green **CI
(hosted)** badge means the PRD linted, no classified artifact was committed, and the Lua parses —
**it does not mean the plugin compiles.** Only **CI (self-hosted)** substantiates that. The PRD
states this obligation in §Source control and repository.
