# Core Layer

`core/` contains ADPP protocol plumbing that hardware providers can reuse:

- `main.cpp`: process entrypoint and mode wiring
- `handlers.*`: ADPP request handlers
- `startup_report.hpp`: startup initialization outcome model
- `runtime_state.*`: runtime startup snapshot shared with handlers
- `transport/`: stdio + framed message transport

This layer should stay independent from device-specific logic.
