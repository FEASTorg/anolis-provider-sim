# anolis-provider-sim - TODO

## Docs

- [ ] Write proper index.md page in docs/
- [ ] Write overview.md page in docs/ describing the general provider aspects versus the simulation-specific aspects.
- [ ] Add a "Getting Started" guide for new developers to quickly set up and run the simulator locally.
- [ ] Document the architecture and key components of the simulator, including how it interacts with the core Anolis application and providers.
- [ ] Create a troubleshooting guide for common issues encountered during development and testing with the simulator.

## CI / Quality

- [ ] configure and clean up clang-format and clang-tidy then enable checks in CI
- [ ] Setup precommit hooks for relevant tooling

## Concurrency / Correctness

- [ ] Run Linux TSAN suite (`ENABLE_TSAN=ON`) focused on ADPP + ticker + state access.
  - Build: `cmake -B build-tsan -DENABLE_TSAN=ON && cmake --build build-tsan`
- [ ] Expand negative tests for signal mismatch/schema drift between provider config and FluxGraph outputs.
- [ ] Expand restart/recovery scenarios while FluxGraph server is active.

## Stress / Performance

- [ ] Run valgrind memory leak analysis for long-running sim mode.
  - Command: `valgrind --leak-check=full --show-leak-kinds=all ./build/Release/anolis-provider-sim ...`
  - Focus: ticker lifecycle, gRPC reconnect paths, command drain loops
- [ ] Add benchmark baselines (tick latency, ADPP throughput, signal sync overhead).
- [ ] Run soak tests (>1h local, periodic >24h) including disconnect/reconnect fault injection.

## Simulation Capability / Docs

- [ ] Expand hardware-style fault-injection mocks (disconnect, timeout, partial availability, stale telemetry).
- [ ] Add practical multi-provider coordination examples for operator workflows.
- [ ] Publish concise troubleshooting and performance-tuning guides for simulation modes.
- [ ] Add dependency/CVE scanning workflow for this repository.
