# Core Layer

`core/` contains ADPP protocol plumbing that hardware providers can reuse:

- `main.cpp`: process entrypoint and mode wiring
- `handlers.*`: ADPP request handlers
- `transport/`: stdio + framed message transport

This layer should stay independent from device-specific logic.
