# Scope beyond v0.1

- WHFast mixed-variable splitting and correctors, IAS15 and additional high-order integrators.
- Multi-revolution Lambert branches and collinear transfer regularization.
- Relativity, oblateness, drag, tides, collisions and close-encounter regularization.
- Automatic kernel acquisition and kernel distribution are deliberately excluded; users supply files with suitable provenance and coverage.
- Long-running scripts retain host objects until process exit. Reuse builder objects and bound matrix sizes.
- Physics tests cover representative trajectories; they are not mission-design certification.
