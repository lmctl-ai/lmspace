# Numerical methods

All integrators and orbital algorithms in `cpp/orbit.cpp` are original code under the project MIT license. They do not incorporate REBOUND code.

The symplectic option is fixed-step velocity Verlet (kick–drift–kick leapfrog). Pairwise Newtonian accelerations are evaluated directly. It conserves angular momentum up to rounding and has bounded oscillatory energy error on resolved bound orbits. This is a second-order method, not WHFast's mixed-variable mapping or correctors. Choose a step much shorter than the shortest orbital period; close encounters require smaller steps or adaptive RK.

The RK option is adaptive Dormand–Prince 5(4), with local error control and a bounded work budget. It is useful for eccentric motion but does not preserve the symplectic structure. It is not IAS15.

Kepler propagation and orbital elements use two-body point masses. Lambert solves the zero-revolution transfer branch. Perturbations, relativistic corrections, oblateness, tides, collisions and maneuver optimization are outside v0.1. All numbers use double precision. Tests compare closed forms and conserved quantities instead of bitwise comparisons between platforms; deterministic output is byte-compared on repeated runs of the same binary.

Near e=1 and zero mean anomaly, the Kepler solver evaluates the linear term and the odd sine/sinh remainder separately. A small-argument series avoids subtracting nearly equal numbers; termination uses the anomaly correction rather than a fixed absolute equation residual. Regression cases include means as small as 1e-16 on either side of e=1.

## Energy-drift warning (after v0.1.1)

`propagate` samples the mass-weighted system energy on up to ~256 output rows plus the last one and keeps the largest
|E(t) - E(0)| divided by the energy scale (sum of |kinetic| and |pair potential| terms at t = 0). Above 1e-3 the host
prints one `lmspace: warning: ...` line on stderr; the run still succeeds. `System.energy_drift()` and
`Space.energy_drift()` return the value of the last propagation. Test particles (tiny mu) barely contribute to the
total, so their errors stay invisible here; the README says so. test.sh checks a close binary encounter: leapfrog with
step 0.05 gives a drift of ~14 and exactly one warning, RK45 at tol 1e-12 stays below 1e-6 with no warning.

## Kernel validation (after v0.1.1)

`load_kernel` used to accept any file: CSPICE reads a file without a DAF/DAS ID word as a text kernel, and a text file
without `\begindata` loads as an empty kernel, so a wrong download failed only at the next query. The host now accepts
a file starting with `DAF/`, `DAS/` or `NAIF/DAF`, or one containing `\begindata`; anything else is a run error
(exit 3) naming the file. A missing file is still reported by CSPICE itself.
