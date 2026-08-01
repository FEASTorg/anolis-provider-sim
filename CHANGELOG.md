# Changelog

All notable changes to `anolis-provider-sim` are documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Historical note: this changelog was written retrospectively from git history at the
time of the first tagged release (`v0.1.0`). Earlier development was tracked in
commit messages only.

---

## [Unreleased]

## [0.2.7] - 2026-08-01

### Added

- `--config-schema` (#115, executable profile v1 §2): prints the provider's
  config JSON Schema in the versioned envelope, emitted from the SDK v0.2.0
  declare-once toolkit. The SAME declaration now drives `--check-config`
  validation (all errors reported at once, with dotted paths) and typed value
  extraction — the advertised schema and the enforced validation cannot drift.
  The simulation-mode key matrix (inert / non_interacting / sim) is declared
  as schema conditionals; per-device-type config subtrees remain open and are
  passed through to the device implementations unchanged. The one
  non-declarable rule (devices[].physics_bindings only under mode=sim) stays
  in load_config, documented in the schema header.

### Changed

- Config validation is schema-honest and STRICTER in corners the old
  hand-written parser let through (all shipped configs unaffected): quoted
  numerics (`tick_rate_hz: "10"`) and stoi-style trailing junk are type
  errors; plain non-string scalars against string fields are type errors
  (notably numeric-looking `devices[].id: 123` must now be quoted); duplicated
  map keys are rejected outright; complex (non-scalar) YAML map keys are
  rejected. The historically-open surfaces
  stay open: unknown ROOT keys and unknown per-device keys are still
  accepted (multi-provider files and free-form device subtrees rely on it).
- SDK pin v0.1.2 → v0.2.0 (picks up the config toolkit; the 0.1.4/0.1.5
  health/i2c additions are defaulted hooks sim does not use).


## [0.2.6] - 2026-07-03

### Added

- linux-arm64 release assets (native arm64 build + tests) — sim was the only
  provider not installable on Raspberry Pi from releases; found by the
  workbench deploy-parity gate's arm64 lane. (#108)


## [0.2.5] - 2026-06-22

### Added

- **ADPP conformance level 2.** Declare `conformance_level = 2` in
  `config/conformance.toml` (no waivers). The provider satisfies the L2 clauses
  of the ADPP semantics: reject a non-Hello request received before a successful
  Hello with `CODE_FAILED_PRECONDITION` (§3.2), enforce declared numeric bounds
  as `CODE_OUT_OF_RANGE`, and reject non-finite doubles (`NaN`/`±Inf`) with
  `CODE_INVALID_ARGUMENT` (§8.3).
- `--version` flag: print the provider version and exit 0, per the Anolis
  executable profile.

### Fixed

- Resolve the harness-surfaced ADPP divergences: emit a single consistent
  unknown-signal policy (§7.4) and prefer `function_id` over `function_name`
  when both are supplied in a `Call` (§6.2).

### CI

- Add the ADPP `provider.conformance` lane: run the pinned
  `anolis-adpp-conformance` harness against the built binary using the
  provider-owned `config/conformance.toml` manifest.
- Add a ThreadSanitizer lane and the shared Valgrind leak-check hardening
  workflow, and a keyless dependency/CVE scan (`cve-bin-tool`) lane.

### Changed

- Routine dependency maintenance: refresh pinned GitHub Actions and Python
  dependencies to current revisions.

## [0.2.4] - 2026-06-16

### Changed

- Bump the vcpkg baseline to the vcpkg `2026.06.01` release: protobuf
  `5.29.5` → `6.33.4`, grpc `1.71.0` → `1.76.0` (FluxGraph feature), abseil and
  the rest refreshed. No source changes required.
- Centralize the vcpkg pin: the shared `setup-vcpkg` action now derives the
  vcpkg commit from `vcpkg-configuration.json`, so the per-workflow
  `VCPKG_COMMIT` env was removed.

### CI

- Migrate Windows build to Visual Studio 2026 / `v145`. The hosted `windows-2025`
  image moved from VS 2022 to VS 2026, breaking the hardcoded `Visual Studio 17
  2022` generator at CMake `project()`. Update the `base-windows-msvc` preset
  generator → `Visual Studio 18 2026`, toolset `v143` → `v145`, rename the
  overlay triplet `x64-windows-v143` → `x64-windows-v145` (and its
  `VCPKG_PLATFORM_TOOLSET`), and update the `triplet:` inputs to the
  `setup-vcpkg` steps in `ci.yml`.
- Add CI OK aggregator gate: removed `paths-ignore`, added `dorny/paths-filter`
  to detect code-vs-docs changes, gated all jobs behind the filter, and added a
  final `ok` job as the sole required status check for `main` branch protection.

## [0.2.3] - 2026-04-24

### Changed

- Updated `anolis-protocol` dependency from v1.1.4 to v1.2.0. The new release
  adds `optional` presence to `ArgSpec` bounds fields. No source changes
  required in this provider.

## [0.2.2] - 2026-04-23

### CI

- Fixed binary portability: added custom `triplets/x64-linux-static.cmake` vcpkg triplet
  (`VCPKG_LIBRARY_LINKAGE=static`, `VCPKG_CRT_LINKAGE=dynamic`, `VCPKG_CMAKE_SYSTEM_NAME=Linux`)
  and applied it to the `ci-linux-release` configure preset via `VCPKG_OVERLAY_TRIPLETS`.
  All vcpkg dependencies (protobuf, yaml-cpp, gtest) are now statically linked into the
  released binary. glibc remains dynamic. The tarball contains a single self-contained executable.

## [0.2.1] - 2026-04-23

### Fixed

- FluxGraph `v1.0.2` source tarball SHA256 corrected in FetchContent pin.

### Changed

- Bump `anolis-protocol` FetchContent pin from `v1.1.3` to `v1.1.4`.

### CI

- Version-sync check wired: `version-locations.txt` now tracks `CMakeLists.txt`
  and `vcpkg.json`; CI calls reusable `version-sync` workflow from `anolishq/.github`.
- `.anpkg` added to `.gitignore`.

### Docs

- Corrected description of `anolis-protocol` coupling.

## [0.2.0] - 2026-04-21

### Fixed

- Use `importlib.import_module` in plugin loader to satisfy mypy `no-any-return` rule.

### Changed

- Bump `anolis-protocol` FetchContent reference from `v1.0.0` to `v1.1.3`.
- Cut FluxGraph optional dependency to FetchContent release pin (`v1.0.2`, SHA256-verified). `FLUXGRAPH_DIR` source-override variable is still supported for local development.

### CI

- Pin org reusable workflow refs from `@main` to `@v1`.
- Add metrics collection to release workflow; `metrics.json` uploaded as release asset on each `v*` tag.

## [0.1.0] - 2026-04-20

First tagged release. The simulator was developed in full before tagging; this
entry summarizes the meaningful work that landed prior to `v0.1.0`.

### Added

- Full ADPP v1 device provider implementation over gRPC: `Handshake`, `Health`,
  `ListDevices`, `DescribeDevice`, `ReadDevice`, `CallDevice`, `StreamTelemetry`.
- Simulated device family: configurable multi-device inventory loaded from YAML
  config, with per-device capability surface matching RLHT and DCMT contracts.
- FluxGraph integration path: optional `ANOLIS_PROVIDER_SIM_ENABLE_FLUXGRAPH`
  build flag wires a FluxGraph engine into the sim tick loop for signal-graph
  driven simulation. Kept as explicit opt-in; baseline binary has no FluxGraph
  dependency.
- Strict/degraded startup policy: provider validates config and device
  initialization before accepting connections; rejects partial startup with clear
  diagnostics.
- `--check-config` flag for config validation without starting the server.
- Dedicated logging infrastructure with structured log levels; migrated all
  diagnostic output from raw stdout.
- C++ unit tests via GoogleTest (vcpkg); integration tests via pytest with a
  shared test harness and CTest registration.
- Provider-label CTest filtering (`-L provider`) for isolated test execution.
- Warnings-as-errors enforced on `ci-linux-release-strict` preset.
- TSAN build support via dedicated preset for data-race validation.
- CI: Linux build/test/strict lane and Windows build lane; shared org workflows.
- Release workflow: on `v*` tag, builds `ci-linux-release-strict`, packages
  binary + source tarball + `manifest.json` + `SHA256SUMS`.

### Changed

- Migrated protocol source from `external/anolis` to `external/anolis-protocol`
  submodule after protocol repository extraction.
- Preset naming consolidated to `ci-linux-release-strict` as the primary CI
  lane; FluxGraph advisory lane remains separate.
- Wrapper scripts removed; all build/test commands use CMake/CTest presets
  directly.
- License changed to AGPL-3.0.
- Org renamed from `FEASTorg` to `anolishq` throughout.
