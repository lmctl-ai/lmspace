# Linux glibc policy (Rule 14)

From v0.1.3, Linux uses dynamic glibc >= 2.35. CSPICE and orbit are explicit STATIC CMake targets; all scientific code and the GCC C++ runtimes remain static. Linux find_library searches prefer .a only. The executable-link helper uses full -static only on Windows; macOS keeps its system-library policy.

Both Linux CI jobs run on Ubuntu 22.04. test.sh runs eight regression fixtures for scripts/check-linux-linkage.py, then requires dynamic libc.so.6 and permits only glibc library/loader names. readelf rejects GLIBC requirements above 2.35; the ldd fallback explicitly reports that it audits dependency names and missing libraries without a symbol-version check. No relink-object archive is generated or required.

Local verification: all 20 test.sh checks passed, including two native suites, 28 self-running examples and 41 deterministic files. The lunar-transfer, gravity-assist and porkchop README examples also passed in fresh directories using build/lmspace and published lmplot v0.2.1. Linux x64 is 2,824,808 bytes, NEEDED is libm.so.6, libc.so.6, ld-linux-x86-64.so.2, and the maximum symbol requirement is GLIBC_2.34; the supported release baseline is glibc >= 2.35. --version stays exactly lmspace X.Y.Z, with matching CMake and source fallback versions.
