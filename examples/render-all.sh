#!/usr/bin/env bash
# Regenerate every picture in docs/images by running the examples with docs/ as the working directory
# (each example writes images/<name>.png and its scratch files under build/). Needs lmscript, lmspace and
# lmplot, lmdraw and lmblender (all auto-installed from GitHub releases unless already on PATH), ffmpeg (for the
# GIFs) and network access (viz_ground_tracks fetches a Natural Earth map, viz_spice_time two NAIF kernels).
set -eu
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export PATH="$HERE/build:$PATH"
cd "$HERE/docs"
for ex in orbit_csv transfers viz_figure_eight anim_figure_eight viz_hohmann viz_lambert viz_energy_error viz_second_law viz_orbits_3d viz_porkchop viz_bielliptic viz_halley viz_trojans viz_orbit_elements \
    viz_ground_tracks viz_sun_synchronous viz_gravity_assist viz_lagrange viz_halo_orbits viz_three_body_zoo viz_lunar_transfer \
    viz_eclipse viz_spice_time viz_orbit_determination viz_delta_v_map viz_rendezvous anim_solar_system render_solar_system; do
  timeout 600 lmscript "../examples/$ex.lms"
done
ls -l images
