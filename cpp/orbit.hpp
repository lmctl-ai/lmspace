#pragma once

#include <string>
#include <vector>

namespace lmspace {

struct Vec3 {
  double x{}, y{}, z{};
};
struct State {
  Vec3 r, v;
};
struct Body {
  std::string name;
  double mu{};
  State state;
};
struct Elements {
  double a{}, e{}, i{}, raan{}, argp{}, nu{};
};

struct Matrix {
  int rows{}, cols{};
  std::vector<double> data;
  double get(int row, int col) const;
};

double kepler(double M, double e);
double period(double a, double mu);
State from_elements(Elements elements, double mu);
Elements to_elements(State state, double mu);
State propagate_kepler(State state, double mu, double dt);
Matrix lambert(Vec3 r1, Vec3 r2, double dt, double mu, bool prograde = true);
Matrix hohmann(double r1, double r2, double mu);
double energy(const std::vector<Body> &bodies);
Vec3 angular_momentum(const std::vector<Body> &bodies);
// Integrate the N-body system. When max_energy_drift is given it receives the
// largest |E(t) - E(0)| over the output rows, divided by the energy scale
// sum |kinetic| + sum |pair potential| at t = 0 (mu-weighted, so test particles
// with tiny mu barely contribute); 0 for a single body.
Matrix propagate(std::vector<Body> bodies, double duration, int points,
                 double step, const std::string &method, double tol = 1e-10,
                 double *max_energy_drift = nullptr);
// Mass-weighted energy scale used for the relative drift above.
double energy_scale(const std::vector<Body> &bodies);

} // namespace lmspace
