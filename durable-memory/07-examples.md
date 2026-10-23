# Examples and reference values

- `examples/orbit_csv.lms`: equal-mass circular binary with mu=1 for each body, separation 1 and period 2*pi/sqrt(2). Adaptive RK45 returns to the initial position and writes a trajectory CSV for lmplot.
- `examples/transfers.lms`: 7000 km to 42164 km Hohmann maneuvers (2.336795782386 and 1.433931450918 km/s with Earth mu=398600.4418), plus quarter-circle Lambert velocities validated against circular speed.
- `tests/physics.lms`: Kepler period and inclined eccentric state/element conversion.
- `tests/systems.lms`: independent systems, stable body ownership, sampled energy and angular momentum conservation.
- `tests/frames.lms`: built-in J2000/ecliptic transform, no kernel required.
- `tests/ephemeris.lms`: loads the original synthetic SPK generated at test runtime and checks position/velocity through the full DSL host. Run `./test.sh` to prepare this fixture.

The DSL compiler does not accept scientific notation in literal floats. Use explicit decimal literals, float(i) to widen integer indices, and accessor calls rather than properties. CSV output uses 17 significant digits and is byte-identical on reruns of the same binary.

Physical reference: JPL's [astrodynamic parameters](https://ssd.jpl.nasa.gov/astro_par.html) publish DE440 gravitational parameters. The ideal two-body Earth–Moon period at 384400 km is about 27.28 days; real lunar sidereal motion differs due to perturbations. NASA's [Moon facts](https://nssdc.gsfc.nasa.gov/planetary/factsheet/moonfact.html) give the physical orbital scale. Transfer checks use vis-viva and the circular speed closed forms; see JPL [orbital mechanics](https://spsweb.fltops.jpl.nasa.gov/portaldataops/mpg/MPG%20Book/Release/Chapter7-OrbitalMechanics.pdf).

## Pictures

The README opens with 14 images in `docs/images/` (13 PNG + 1 GIF), grouped by theme. Every image is made by a
self-contained example: `lmctl autorun <raw url>` in an empty directory runs the computation
(`system ${dsl_cli(sim)} --dsl ...`), draws with lmplot or lmdraw and writes `images/<name>.png` plus scratch files under
`build/`. `examples/render-all.sh` runs all of them with `docs/` as the working directory.

| example | picture | story and check |
| --- | --- | --- |
| `viz_figure_eight`, `anim_figure_eight` | figure_eight.png/.gif | the figure-eight choreography closes after one period (RK45, 1e-12); lmdraw frames + ffmpeg GIF |
| `viz_trojans` | trojans.png | three asteroids near L4 stay at Jupiter's radius within 10 % for 24 Jupiter years; drawn in the rotating frame |
| `orbit_csv` | binary_orbit.png | equal-mass binary closed form |
| `viz_halley` | halley.png | period 75.3 yr, perihelion speed 54.5 km/s (vis-viva), speed on a log axis |
| `viz_second_law` | second_law.png | angular momentum equal at 24 samples |
| `viz_hohmann`, `transfers` | hohmann.png, hohmann_sweep.png | arrival at GEO radius; delta-v and flight time sweep |
| `viz_bielliptic` | bielliptic.png | Hohmann cheaper at R = 10 (0.5298), far bi-elliptic cheaper at R = 14; crossovers 11.94 and 15.58 |
| `viz_lambert` | lambert.png | each Lambert departure velocity lands on the target |
| `viz_porkchop` | porkchop.png | 40 x 31 Lambert transfers Earth to Mars; minimum 5.6 +- 0.5 km/s at 260 +- 40 days |
| `viz_orbits_3d` | orbits_3d.png | four Earth orbits close after one period |
| `viz_orbit_elements` | orbit_elements.png | elements -> state -> elements round trip, r = a(1-e^2)/(1+e cos nu); lmdraw geometry diagram |
| `viz_energy_error` | energy_error.png | leapfrog vs RK45 energy error |

How the hosts talk: a sweep prints one line per case with `print`; lmscript saves `r.stdout`; lmplot reads the file with
`every ::0::N` (skips the JSON status line) and `set datafile missing '?'` for gaps. lmspace's `save_csv` has no header;
lmscript prepends one before `csv_parse`. `Matrix` output of a zero-duration propagate (`propagate(0.0, 2, ...)`) is the
way to get a state out as a CSV row. lmscript has no trig (hard-code cos/sin of fixed angles, or use Taylor `fn`s);
`${x}` substitutes an integral number as an int literal, so add `+ 0.0001` where the DSL wants a float. The DSL has no
`assert`: use `assert_close` (and `clamp(diff * 1e5, 0, 1)` for inequalities). lmdraw may only write under
`build/rendered/` (copy_file to `images/`), its `end _dsl_` line must start at column 1, and `plot.data` truncates long strings.
Notes: propagate tolerance must be > 0 even for `"symplectic"`; the RK45 `step` is an upper bound; an apostrophe in a
gnuplot title breaks the command.

`test.sh` checks the gallery (PNG/GIF magic, <= 3 MB, linked examples exist), runs the examples in a scratch directory
when lmplot is available (`LMSPACE_TEST_NETWORK=1`; the anim example is skipped loudly without ffmpeg) and compares the
CSVs of a second run byte for byte.
