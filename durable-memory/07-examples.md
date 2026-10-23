# Examples and reference values

- `examples/orbit_csv.lms`: equal-mass circular binary with mu=1 for each body, separation 1 and period 2*pi/sqrt(2). Adaptive RK45 returns to the initial position and writes a trajectory CSV for lmplot.
- `examples/transfers.lms`: 7000 km to 42164 km Hohmann maneuvers (2.336795782386 and 1.433931450918 km/s with Earth mu=398600.4418), plus quarter-circle Lambert velocities validated against circular speed.
- `tests/physics.lms`: Kepler period and inclined eccentric state/element conversion.
- `tests/systems.lms`: independent systems, stable body ownership, sampled energy and angular momentum conservation.
- `tests/frames.lms`: built-in J2000/ecliptic transform, no kernel required.
- `tests/ephemeris.lms`: loads the original synthetic SPK generated at test runtime and checks position/velocity through the full DSL host. Run `./test.sh` to prepare this fixture.

The DSL compiler does not accept scientific notation in literal floats. Use explicit decimal literals, float(i) to widen integer indices, and accessor calls rather than properties. CSV output uses 17 significant digits and is byte-identical on reruns of the same binary.

Physical reference: JPL's [astrodynamic parameters](https://ssd.jpl.nasa.gov/astro_par.html) publish DE440 gravitational parameters. The ideal two-body Earth–Moon period at 384400 km is about 27.28 days; real lunar sidereal motion differs due to perturbations. NASA's [Moon facts](https://nssdc.gsfc.nasa.gov/planetary/factsheet/moonfact.html) give the physical orbital scale. Transfer checks use vis-viva and the circular speed closed forms; see JPL [orbital mechanics](https://spsweb.fltops.jpl.nasa.gov/portaldataops/mpg/MPG%20Book/Release/Chapter7-OrbitalMechanics.pdf).
