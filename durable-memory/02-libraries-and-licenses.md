# Libraries and licenses

lmspace uses the NASA NAIF CSPICE Toolkit as an optional runtime ephemeris
backend. The numerical integrators and orbital mechanics code remain lmspace's
own MIT-licensed implementation. CSPICE is not REBOUND and no GPL or AGPL code
is linked.

| library | version (commit) | license / terms | how used |
| --- | --- | --- | --- |
| NAIF CSPICE | N0067 source, mirror `e44da83b8a7a420fdddf02d2e3accebd0ab86e5e` | NAIF SPICE Software License rules and Caltech/JPL/NASA disclaimer; see `THIRD_PARTY_NOTICES` | C sources compiled into a static library; user kernels loaded only at runtime |

The upstream CSPICE distribution is published by NAIF, not as a conventional
OSI license. NAIF's rules permit commercial use without fees, require the
unmodified Toolkit to be obtained from NAIF or an official flight project, and
restrict unmodified redistribution from a mirror without prior written NAIF
clearance. The selected Git mirror is used as a source packaging convenience;
its README identifies the NAIF origin. lmspace records that origin, the exact
mirror commit, the full source disclaimer, and the authoritative rules URL in
`THIRD_PARTY_NOTICES`. A release should retain that notice and obtain written
clearance if distributing the mirror's unmodified Toolkit as a standalone
package.

The CSPICE source is built directly by `cmake/Spice.cmake` as a static `cspice`
target. The mirror's CMake project is not invoked because it mutates the parent
cache and installs kernel/data files. No kernels are checked in, embedded, or
needed for compilation; callers may load their own kernels through the
`spice_load(path)` API, and those kernels retain their own distributors' terms.

The wrapper and lmspace adapter are MIT licensed. CSPICE source files remain
under the NAIF terms shown in their headers. NAIF does not provide support for
third-party interfaces or modified source; the adapter therefore reports
CSPICE errors directly and keeps the CSPICE source unchanged.
