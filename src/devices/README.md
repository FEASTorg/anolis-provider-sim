# Device Layer

`devices/` holds provider device implementations and shared device infrastructure.

- `common/`: factory, manager, registry, and shared ADPP helpers
- `tempctl/`, `motorctl/`, `relayio/`, `analogsensor/`: device modules

Hardware providers can copy this structure and replace module implementations.
