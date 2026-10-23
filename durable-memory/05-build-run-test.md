# Build, run and test

Measured 2026-10-23 on x86_64 Linux with GCC and Clang.

- Fully static stripped Linux executable: approximately 4.0 MB (3.81 MiB); `ldd` reports no dynamic executable. CSPICE is linked from its pinned C sources; no astronomy kernels are needed at build time.
- `./test.sh`: 17 grouped checks, two native suites, six DSL programs (four tests and two examples), two deterministic CSV files and thirteen runtime error cases. All pass locally with GCC; a full Clang build also passed.
- The numerical suite currently reports 28 always-active checks (including Release/NDEBUG). It tests elliptic/hyperbolic/parabolic Kepler equations, period, element degeneracies, full-vector two-body closure, zero-revolution Lambert endpoints over short/long flight times, Hohmann impulses, Earth–Moon scale, RK45 accuracy, energy/angular momentum conservation and tiny output timesteps.
- The SPICE suite reports 13 checks: built-in frames, missing kernel and ephemeris errors, failure recovery, runtime generation/loading of an original synthetic degree-one SPK, its six state components and kernel clearing. A second generated SPK fixture exercises the entire DSL ephemeris path.
- Sanitized native numerical tests pass under AddressSanitizer and UndefinedBehaviorSanitizer. Clang static analysis of our numerical implementation and shellcheck of all scripts pass. Python schema generation compiles and registry/compiler/embedded schema parity passes.

Build: `scripts/sync-deps.sh`, then `cmake -S . -B build` and `cmake --build build --parallel 4`. Tests require the lmscript v5 compiler (`@lmctl-ai/lmctl@0.2.369` in CI). `scripts/select-compiler.sh` verifies emitted bytecode capability. SDK verification checks the vendored SHA-256; dependency verification checks the pinned commit and tracked source cleanliness.

CSV values use 17 significant digits; deterministic reruns are compared within a platform, not across different libm implementations. Numerical assertions use physical tolerances. Limits bound output cells, body count and integration work; large or unresolved close encounters can stop with a clear host error.
