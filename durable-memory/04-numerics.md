# Numerical methods

All integrators and orbital algorithms in `cpp/orbit.cpp` are original code under the project MIT license. They do not incorporate REBOUND code.

The symplectic option is fixed-step velocity Verlet (kick–drift–kick leapfrog). Pairwise Newtonian accelerations are evaluated directly. It conserves angular momentum up to rounding and has bounded oscillatory energy error on resolved bound orbits. This is a second-order method, not WHFast's mixed-variable mapping or correctors. Choose a step much shorter than the shortest orbital period; close encounters require smaller steps or adaptive RK.

The RK option is adaptive Dormand–Prince 5(4), with local error control and a bounded work budget. It is useful for eccentric motion but does not preserve the symplectic structure. It is not IAS15.

Kepler propagation and orbital elements use two-body point masses. Lambert solves the zero-revolution transfer branch. Perturbations, relativistic corrections, oblateness, tides, collisions and maneuver optimization are outside v0.1. All numbers use double precision. Tests compare closed forms and conserved quantities instead of bitwise comparisons between platforms; deterministic output is byte-compared on repeated runs of the same binary.
