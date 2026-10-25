# Build, run and test

Measured 2026-10-23 on x86_64 Linux with GCC and Clang.

- Linux uses dynamic glibc >= 2.35, with static CSPICE, orbital code and GCC C++ runtimes. test.sh enforces the ELF dependency allowlist and glibc 2.35 ABI ceiling. No astronomy kernels are needed at build time.
- `./test.sh`: 20 grouped checks (including the memory-budget runaway), two native suites, four DSL test programs, 28 examples, 41 deterministic output files and fifteen runtime error cases. All pass locally with GCC; an earlier baseline also passed a full Clang build.
- The numerical suite currently reports 42 always-active checks (including Release/NDEBUG). It tests elliptic/hyperbolic/parabolic Kepler equations, period, element degeneracies, full-vector two-body closure, zero-revolution Lambert endpoints over short/long flight times, Hohmann impulses, Earth–Moon scale, RK45 accuracy, energy/angular momentum conservation and tiny output timesteps, and near-parabolic elliptic/hyperbolic anomalies.
- The SPICE suite reports 13 checks: built-in frames, missing kernel and ephemeris errors, failure recovery, runtime generation/loading of an original synthetic degree-one SPK, its six state components and kernel clearing. A second generated SPK fixture exercises the entire DSL ephemeris path.
- Sanitized native numerical tests pass under AddressSanitizer and UndefinedBehaviorSanitizer. Clang static analysis of our numerical implementation and shellcheck of all scripts pass. Python schema generation compiles and registry/compiler/embedded schema parity passes.

Build: `scripts/sync-deps.sh`, then `cmake -S . -B build` and `cmake --build build --parallel 4`. Tests require the lmscript v5 compiler (`@lmctl-ai/lmctl@0.2.419` in CI). `scripts/select-compiler.sh` verifies emitted bytecode capability. `scripts/sync-sdk.sh` vendors `cpp/dslvm.hpp` and every `cpp/stdlib/*.hpp` from one pinned lmcode-cpp commit; `sdk.lock` holds `commit=` and one `sha256.<path>=` line per vendored header, and `--verify` checks each file against its line. Dependency verification checks the pinned commit and tracked source cleanliness.

CSV values use 17 significant digits; deterministic reruns are compared within a platform, not across different libm implementations. Numerical assertions use physical tolerances. Limits bound output cells, body count and integration work; large or unresolved close encounters can stop with a clear host error.

Local v0.1.3 rollout validation: 20 checks passed, including both native suites, all 28 gallery examples and 41 byte-identical output files on rerun. The built Linux x64 binary is 2,824,808 bytes, needs libm.so.6, libc.so.6 and ld-linux-x86-64.so.2, and has maximum GLIBC_2.34. Three extra README runs (lunar transfer, gravity assist, porkchop) passed in fresh directories with the candidate and released lmplot v0.2.1.
