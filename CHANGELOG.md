# Changelog

All notable changes to `anolis-provider-sim` are documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Historical note: this changelog was written retrospectively from git history at the
time of the first tagged release (`v0.1.0`). Earlier development was tracked in
commit messages only.

---

## [Unreleased]

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
