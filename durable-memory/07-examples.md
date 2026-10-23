# Examples and reference values

- `examples/orbit_csv.lms`: equal-mass circular binary with mu=1 for each body, separation 1 and period 2*pi/sqrt(2). Adaptive RK45 returns to the initial position and writes a trajectory CSV for lmplot.
- `examples/transfers.lms`: 7000 km to 42164 km Hohmann maneuvers (2.336795782386 and 1.433931450918 km/s with Earth mu=398600.4418), plus quarter-circle Lambert velocities validated against circular speed.
- `tests/physics.lms`: Kepler period and inclined eccentric state/element conversion.
- `tests/systems.lms`: independent systems, stable body ownership, sampled energy and angular momentum conservation.
- `tests/frames.lms`: built-in J2000/ecliptic transform, no kernel required.
- `tests/ephemeris.lms`: loads the original synthetic SPK generated at test runtime and checks position/velocity through the full DSL host. Run `./test.sh` to prepare this fixture.

The DSL compiler does not accept scientific notation in literal floats. Use explicit decimal literals, float(i) to widen integer indices, and accessor calls rather than properties. CSV output uses 17 significant digits and is byte-identical on reruns of the same binary.

Physical reference: JPL's [astrodynamic parameters](https://ssd.jpl.nasa.gov/astro_par.html) publish DE440 gravitational parameters. The ideal two-body Earth–Moon period at 384400 km is about 27.28 days; real lunar sidereal motion differs due to perturbations. NASA's [Moon facts](https://nssdc.gsfc.nasa.gov/planetary/factsheet/moonfact.html) give the physical orbital scale. Transfer checks use vis-viva and the circular speed closed forms; see JPL [orbital mechanics](https://spsweb.fltops.jpl.nasa.gov/portaldataops/mpg/MPG%20Book/Release/Chapter7-OrbitalMechanics.pdf).

## Pictures (VISUALS.md)

README starts with a gallery of 8 PNGs in `docs/images/` (~900 px, each <= 300 KB). Every image is made by a
self-contained example: `lmctl autorun <raw url>` in an empty directory runs the computation
(`system ${dsl_cli(sim)} --dsl ...`), draws with lmplot (a second `_dsl_` block) and writes
`images/<name>.png` plus scratch files under `build/`. `examples/render-all.sh` runs them all with `docs/`
as the working directory to refresh `docs/images/`.

| example | image | what it checks |
| --- | --- | --- |
| `viz_figure_eight.lms` | figure_eight | the figure-eight choreography closes after one period (RK45, 1e-12) |
| `viz_hohmann.lms` | hohmann | transfer ellipse reaches GEO radius, arrival speed plus burn 2 = circular speed |
| `viz_orbits_3d.lms` | orbits_3d | elements -> state -> one period closes for four Earth orbits |
| `viz_lambert.lms` | lambert | propagating each Lambert departure velocity lands on the target with the predicted arrival velocity |
| `viz_second_law.lms` | second_law | angular momentum equal at 24 samples, back at perigee after a period |
| `viz_energy_error.lms` | energy_error | leapfrog error bounded and ~dt^2, RK45 error follows its tolerance |
| `orbit_csv.lms` | binary_orbit | existing binary closed-form assertions |
| `transfers.lms` | hohmann_sweep | existing Hohmann/Lambert assertions + a printed sweep fed to `plot.data` |

Notes: the propagate tolerance must be > 0 even for `"symplectic"`; the RK45 `step` is an upper bound, not the
output spacing; a title containing an apostrophe breaks the gnuplot command; scalars computed in a sweep can
leave the host as printed lines in `r.stdout` and enter lmplot with `plot.data` (use `every ::0::N-1` to skip the
trailing JSON status line). `test.sh` checks the gallery (PNG/GIF magic, <= 3 MB, linked examples exist), runs
all examples in a scratch directory when lmplot is available (`LMSPACE_TEST_NETWORK=1` or on PATH, else prints
`SKIPPED`) and compares the CSVs of a second run byte for byte.
