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

The README opens with 28 images in `docs/images/` (26 PNG + 2 GIF), grouped by theme (missions and transfers,
Earth orbits, three bodies, the Solar System). Every image is made by a
self-contained example: `lmctl autorun <raw url>` in an empty directory runs the computation
(`system ${dsl_cli(sim)} --lmcode ...`), draws with lmplot, lmdraw or lmblender and writes `images/<name>.png` plus scratch files under
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
| `viz_ground_tracks` | ground_tracks.png | ISS/SSO/Molniya/GPS; max latitude = inclination, GPS track closes after a sidereal day, Molniya 88 % of the time north; Natural Earth 110m land (v5.1.2 raw URL) drawn by lmdraw, packed into 8 paths (16 KB path limit) |
| `viz_sun_synchronous` | sun_synchronous.png | J2 inclination by bisection: 98.21 deg at 705 km, 98.54 at 786 km, none above 5950-6000 km; ISS node drift -5.0 deg/day |
| `viz_gravity_assist` | gravity_assist.png | Sun + Jupiter + probe, approach leg by time reversal; turn 129.4 deg and dv 10.1 km/s predicted vs 127.5 deg / 10.6 km/s integrated (Sun's pull during 0.3 yr) |
| `viz_lagrange` | lagrange.png | L1..L3 by Newton, Jacobi constants to 5e-6; zero-velocity curves via gnuplot `set table` contours; L1 particle escapes, L4 one librates |
| `viz_halo_orbits` | halo_orbits.png | Newton on (z0, vy0) at fixed x0 with crossing interpolation; L1 x0 0.83 -> z0 0.11406, 12.12 d; L2 x0 1.17 -> z0 0.09440, 14.51 d (matched scipy DOP853) |
| `viz_three_body_zoo` | three_body_zoo.png | butterfly I/II, moth I, yin-yang Ia, dragonfly, goggles (Suvakov-Dmitrasinovic 2013); return error < 0.005 |
| `viz_lunar_transfer` | lunar_transfer.png | bisection on Moon lead angle (123.07 deg) for 100 km periapsis, fine re-propagation at 0.1 s to place LOI; TLI 3.136, LOI 0.828 km/s, orbit 99.5 km. A free return was tried and dropped: too sensitive to tune robustly |
| `viz_eclipse` | eclipse.png | shadow fraction by sampling vs closed form (acos by bisection) within 0.003; ISS 36 min at beta 0, ends at 70.2 deg; GEO 69 min, ends at 8.7 deg |
| `viz_spice_time` | spice_time.png | naif0012.tls + pck00011.tpc fetched with http_get; ET-UTC 42.184 (1972) to 69.184 s; sidereal-day rotation and obliquity checks via matrix traces |
| `viz_orbit_determination` | orbit_determination.png | Gibbs with Park-Miller + Irwin-Hall noise; exact without noise; RMS a error 961 km (60 s) -> 0.99 km (1500 s) |
| `viz_delta_v_map` | delta_v_map.png | lmdraw subway map; incremental edges (escape 3.22, +0.39 to Mars thanks to Oberth) |
| `viz_rendezvous` | rendezvous.png | Kepler-exact relative motion in LVLH vs Clohessy-Wiltshire; radial loop 3.54 km, forward drift 16.7 km/orbit |
| `anim_solar_system` | solar_system.gif | J2000 mean elements; opposition day 529 (13 June 2001), retrograde 69 days; right panel is apparent rate in deg/day |
| `render_solar_system` | solar_system_3d.png | lmblender strands for orbits, glow_orb Sun as the only light, 160 samples, ~7 s |

How the hosts talk: a sweep prints one line per case with `print`; lmscript saves `r.stdout`; lmplot reads the file with
`every ::0::N` (skips the JSON status line) and `set datafile missing '?'` for gaps. lmspace's `save_csv` has no header;
lmscript prepends one before `csv_parse`. `Matrix` output of a zero-duration propagate (`propagate(0.0, 2, ...)`) is the
way to get a state out as a CSV row. lmscript has no trig (hard-code cos/sin of fixed angles, or use Taylor `fn`s); the lmspace DSL has sin/cos/sqrt/pow/floor but
no atan2/asin/acos, so the examples use Newton (atan2: start within 45 deg, 5 steps) or bisection (acos);
`cmd` is a reserved lmscript name; lmscript lists are immutable except `push`; the host's `{"ok": ...}` line must be
stripped (`regex_replace`) before `csv_parse`; gnuplot enhanced text treats `~` as overprint and `_` as subscript;
`${x}` substitutes an integral number as an int literal, so add `+ 0.0001` where the DSL wants a float. The DSL has no
`assert`: use `assert_close` (and `clamp(diff * 1e5, 0, 1)` for inequalities). lmdraw may only write under
`build/rendered/` (copy_file to `images/`), its `end _dsl_` line must start at column 1, and `plot.data` truncates long strings.
Notes: propagate tolerance must be > 0 even for `"symplectic"`; the RK45 `step` is an upper bound; an apostrophe in a
gnuplot title breaks the command.

`test.sh` checks the gallery (PNG/GIF magic, <= 3 MB, linked examples exist), runs the examples in a scratch directory
when lmplot is available (`LMSPACE_TEST_NETWORK=1`; anim examples are skipped loudly without ffmpeg, and
`viz_ground_tracks`, `viz_spice_time` and `render_*` without `LMSPACE_TEST_NETWORK=1`) and compares the
CSVs of a second run byte for byte.
