# AGENTS.md — anolis-provider-sim

> Per-repo conventions for coding agents (Claude Code, OpenCode, …). The
> canonical cross-repo rules — Conventional Commits, minimal-first/YAGNI, no
> secrets, run checks before asserting success — live in the user's **global**
> `AGENTS.md` and are not repeated here. This file records only what is
> **specific to this repo**: the commands, the gate, and the non-obvious things
> agents get wrong here.

C++20 ADPP simulation provider for the Anolis system.

## Build / test

- Configure + build: `cmake --preset ci-linux-release` then
  `cmake --build --preset ci-linux-release`; test with `ctest`.
- FluxGraph-enabled lanes use the `*-fluxgraph` presets (e.g.
  `ci-linux-release-fluxgraph`).
- The required CI status check is the **`ok`** job (it aggregates the lanes);
  never bypass it, and never merge red.

## Tooling

- **C++ repos:** clang-format / clang-tidy are pinned to **18.1.8** via the
  shared `setup-clang-tools` action (matches workstation-configs) — do NOT use
  pip/apt/pre-commit/container versions. Run `clang-format -i` before **every**
  commit (CI fails otherwise). vcpkg comes from the shared `setup-vcpkg` action.
- Shared `.github` actions/workflows are SHA-pinned with a `# <tag>` comment so
  Renovate can track them — keep that comment when bumping.

## Repo-specific gotchas

- **C++20**, and use **`std::format`** for diagnostics/log messages.
- **It's a simulator — there is no hardware.** The mock/sim behavior IS the
  product; don't add real-device assumptions or hardware dependencies.
- **Optional FluxGraph integration** is pulled in via `FetchContent`
  (`CMakeLists.txt`); the base build works without it. Only the `*-fluxgraph`
  presets enable it.
- The **Windows + FluxGraph-ON build lane runs NIGHTLY, not per-PR** (scheduled
  in `ci.yml`; cache-eviction fix, anolis-provider-sim#83). Don't be surprised
  it is absent from a PR's checks — that is intentional, not a missing lane.
- **Conformance harness** is selected via `--provider-profile` (see
  `config/conformance.toml`).
- **Test-only clang-tidy relaxations** live in `tests/.clang-tidy`; don't
  "fix" test code to satisfy a check the test config already suppresses.

## Backlog

Backlog lives in GitHub issues, not a `TODO.md`.
