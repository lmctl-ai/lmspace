#!/usr/bin/env bash
# Regenerate every picture in docs/images by running the examples with docs/ as the working directory
# (each example writes images/<name>.png and its scratch files under build/). Needs lmscript, lmspace and
# lmplot (all auto-installed from GitHub releases unless already on PATH).
set -eu
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export PATH="$HERE/build:$PATH"
cd "$HERE/docs"
for ex in orbit_csv transfers viz_figure_eight viz_hohmann viz_lambert viz_energy_error viz_second_law viz_orbits_3d; do
  timeout 600 lmscript "../examples/$ex.lms"
done
ls -l images
