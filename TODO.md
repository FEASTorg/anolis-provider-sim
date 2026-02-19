# TODO

**Thread Safety Validation:**

- [ ] TSAN builds on Linux (already have `ENABLE_TSAN` option)
  - Build: `cmake -B build-tsan -DENABLE_TSAN=ON && cmake --build build-tsan`
  - Run full test suite under TSAN
  - Focus: Concurrent ADPP + ticker, device state access

**Advanced Integration:**

- [ ] Multi-provider coordination examples
- [ ] Provider restart while FluxGraph running
- [ ] Signal mismatch error handling (provider expects signal FluxGraph doesn't provide)

**Documentation:**

- [ ] Advanced troubleshooting guide (common errors, solutions)
- [ ] Performance tuning guide (tick rates, timeouts)

**Extended Stress Testing:**

- [ ] Valgrind memory leak analysis (Linux)
  - Command: `valgrind --leak-check=full --show-leak-kinds=all ./build/Release/anolis-provider-sim ...`
  - Focus: Long-running ticker thread, gRPC connection lifecycle
- [ ] Long-duration sim mode runs (>1 hour) with memory profiling
- [ ] Error recovery scenarios (FluxGraph server disconnect/reconnect)
- [ ] Performance benchmarks (tick latency, throughput under load)
- [ ] Cross-platform testing (Linux build and test execution)
