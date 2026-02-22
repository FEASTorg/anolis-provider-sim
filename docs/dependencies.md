# Dependency, Build, and CI Governance

This document defines provider-sim policy for dependencies, CI lane tiers, presets, and cross-repo compatibility.

## vcpkg Policy

1. `vcpkg-configuration.json` is the baseline source of truth.
2. Lockfile pinning is deferred for now.
3. Determinism is enforced via baseline pinning plus reviewed `vcpkg.json` changes.

## Python Dependency Policy

1. CI installs from `requirements-lock.txt`, not floating `requirements.txt`.
2. `requirements.txt` defines ranges for maintainers; `requirements-lock.txt` is the execution source in CI.
3. Lockfile updates must be committed in the same PR as dependency range changes.

## Versioning Policy

- `anolis-provider-sim` follows independent SemVer (`MAJOR.MINOR.PATCH`).
- Provider public behavior/build-surface changes require version-bump decision and release note.
- Compatibility with runtime is tracked in `anolis/.ci/compatibility-matrix.yml`.

## CI Lane Tiers

- **Required**:
  - Linux release (`fluxgraph OFF`)
  - Windows release (`fluxgraph OFF`)
- **Advisory**:
  - Linux release (`fluxgraph ON`)
- **Nightly/optional**:
  - heavy sanitizer/stress lanes

Promotion rule:
- Advisory lane can be promoted to required after 10 consecutive green default-branch runs and explicit promotion PR.

## Dual-Run Policy

During migration to presets/new CI paths:
- run legacy and new paths in parallel,
- minimum 5 consecutive green runs,
- preferred 10 runs before legacy path removal.

## Preset Baseline and Exception Policy

Baseline names:
- `dev-debug`, `dev-release`, `ci-linux-release`, `ci-windows-release`
- feature extension lane: `ci-linux-release-fluxgraph`

Rules:
1. CI jobs should call presets directly.
2. CI-only deviations must be explicit and documented.
3. Feature-specific extension presets are allowed when documented.
4. Every preset must have an active owner/use-case (CI lane, script default, or documented workflow); remove unreferenced presets.

Contributor quick check:

```bash
cmake --list-presets
ctest --list-presets
```

On Windows local development, use `dev-windows-*` presets.

## Cross-Repo Coupling Policy

- `anolis-protocol` is consumed as pinned submodule.
- FluxGraph integration remains explicit opt-in (`ENABLE_FLUXGRAPH`, `FLUXGRAPH_DIR`).
- No hidden network dependency resolution in normal configure paths.
- FluxGraph advisory CI consumes a pinned commit from `.ci/dependency-pins.yml` (never floating branch heads).
