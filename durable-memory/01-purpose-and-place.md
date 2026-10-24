# Purpose and place

lmspace is an orbital mechanics and astronomy host for lmscript DSL bytecode. One executable (dynamic glibc >= 2.35 on Linux) contains the numerical implementation, schema, notices and NASA CSPICE modules. There is no general user CLI: scripts use the `space` context. The standard version, notices, signatures, schema and bytecode options follow lmnum.

Our numerical code is original MIT C++17. REBOUND and other GPL/AGPL libraries are excluded. The chosen integrators are leapfrog and adaptive Runge–Kutta, not implementations or copies of WHFast or IAS15.

No ephemeris kernel is fetched or required to build. Users can explicitly load their own kernels at runtime; precision and coverage of astronomical queries depend on those kernels. Numerical integration needs no kernels. Matrices and CSV provide exchange with lmplot without coupling it into the binary.
