#pragma once

#include <string>

namespace lmspace {

struct Matrix;

// Load and clear user-provided SPICE kernels. No kernel is needed to build or
// use the numerical parts of lmspace; ephemeris calls require suitable kernels.
void spice_load(const std::string &path);
void spice_clear();

// Return the geometric (no aberration correction) state as a 1x6 matrix
// [x,y,z,vx,vy,vz], in km and km/s, at ET seconds past J2000.
Matrix spice_state(const std::string &target, const std::string &observer,
                   const std::string &frame, double et);

// Return the 3x3 rotation taking coordinates in `from` into `to` at ET.
Matrix spice_frame(const std::string &from, const std::string &to, double et);

// Parse a UTC or other CSPICE-supported time string into ET seconds past J2000.
double spice_time(const std::string &utc);

} // namespace lmspace
