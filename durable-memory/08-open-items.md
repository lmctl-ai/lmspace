# Scope beyond v0.1

- WHFast mixed-variable splitting and correctors, IAS15 and additional high-order integrators.
- Multi-revolution Lambert branches and collinear transfer regularization.
- Relativity, oblateness, drag, tides, collisions and close-encounter regularization.
- Automatic kernel acquisition and kernel distribution are deliberately excluded; users supply files with suitable provenance and coverage.
- Long-running scripts retain host objects until process exit. Reuse builder objects and bound matrix sizes.
- Physics tests cover representative trajectories; they are not mission-design certification.
- Energy-drift warning covers the mass-weighted total only; a test particle (tiny mu) passing too close to a planet under a coarse fixed step is not flagged. A per-particle check would need a reference potential.
- The drift warning and kernel validation are on main after v0.1.1; they ship with the next tag (v0.1.2).
