# TODO

- [x] Rename the testing scripts and docs and any other parts to use descriptive naming rather than referring to meaningless "phases" of development that have been done or similar stuff - things should use proper descriptive names that reflect their purpose and content, not the arbitrary phase number of development they were created in. This will make it easier to understand and maintain the documentation and codebase over time, especially as the project evolves beyond the initial development phases. Think thru best pratices and standard convention for naming test scripts and documentation in software projects and apply those here.
  - **Completed:** Renamed `test_phase2.py` → `test_adpp_integration.py` with updated documentation

- [x] Update existing content of the scripts to reflect current state and create any missing scripts etc if relevant
  - **Completed:** Created comprehensive `test_fault_injection.py` (570 lines, 6 test suites covering all fault injection types)
  - **Completed:** Updated all test scripts with proper docstrings and descriptive naming

- [x] Document fault injection capabilities in the simulation provider and test scenarios, and add more fault types (if relevant; this todo was made a while ago)
  - **Completed:** Fault injection already well documented in README.md with all 5 fault types
  - **Completed:** Added comprehensive test coverage for all fault injection scenarios
  - **Completed:** Updated README.md testing section with test script documentation
  - **Note:** function_id parameter clarified to use numeric ID as string (e.g., "1" not "set_mode")

---

## Future Enhancements

_(No immediate TODOs - provider-sim is feature complete for current requirements)._
