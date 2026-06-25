# anolis-provider-sim - TODO

## Docs

- [ ] Write overview.md page in docs/ describing the general provider aspects versus the simulation-specific aspects.
- [ ] Document the architecture and key components of the simulator, including how it interacts with the core Anolis application and providers.
- [ ] Create a troubleshooting guide for common issues encountered during development and testing with the simulator.

## Concurrency / Correctness

- [ ] Expand negative tests for signal mismatch/schema drift between provider config and FluxGraph outputs.
- [ ] Expand restart/recovery scenarios while FluxGraph server is active.

## Stress / Performance

- [ ] Add benchmark baselines (tick latency, ADPP throughput, signal sync overhead).
- [ ] Run soak tests (>1h local, periodic >24h) including disconnect/reconnect fault injection.

## Simulation Capability / Docs

- [ ] Expand hardware-style fault-injection mocks (disconnect, timeout, partial availability, stale telemetry).
- [ ] Add practical multi-provider coordination examples for operator workflows.
- [ ] Publish concise troubleshooting and performance-tuning guides for simulation modes.
