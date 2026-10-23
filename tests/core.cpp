#include "orbit.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
using namespace lmspace;
static bool close(double a, double b, double e = 1e-8) {
  return std::abs(a - b) <= e * (1 + std::max(std::abs(a), std::abs(b)));
}
static double mag(Vec3 a) {
  return std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
}
static Vec3 delta(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
static int checks = 0;
static void check(bool ok, const std::string &s) {
  ++checks;
  if (!ok)
    throw std::runtime_error("FAIL: " + s);
}
int main() {
  try {
    const double mu = 1;
    check(close(kepler(0, 0), 0, 1e-14) && close(kepler(1, 0), 1, 1e-14) &&
              close(kepler(0, .1), 0, 1e-14),
          "circular zero mean anomaly");
    double m = kepler(.7, .2);
    check(close(m - .2 * std::sin(m), .7, 1e-11), "Kepler equation");
    double mh = kepler(-3, 2.5);
    check(close(2.5 * std::sinh(mh) - mh, -3, 1e-11),
          "hyperbolic Kepler equation");
    double md = kepler(.001, 1);
    check(close(md + md * md * md / 3, .001, 1e-12),
          "parabolic Kepler equation");
    check(close(period(1, mu), 2 * 3.141592653589793), "Kepler period");
    Elements q{2, .4, .6, .3, .5, 1.1};
    State s = from_elements(q, mu);
    Elements back = to_elements(s, mu);
    check(close(back.a, q.a, 1e-9) && close(back.e, q.e, 1e-9) &&
              close(back.i, q.i, 1e-9) && close(back.raan, q.raan, 1e-9) &&
              close(back.argp, q.argp, 1e-9) && close(back.nu, q.nu, 1e-9),
          "inclined eccentric elements");
    State one = propagate_kepler(s, mu, period(q.a, mu));
    check(mag(delta(one.r, s.r)) < 1e-8 && mag(delta(one.v, s.v)) < 1e-8,
          "full vector Kepler closure");
    Elements hyper{-3, 1.5, .2, .4, .7, .3};
    Elements hb = to_elements(from_elements(hyper, mu), mu);
    check(close(hb.a, hyper.a, 1e-8) && close(hb.e, hyper.e, 1e-8),
          "hyperbolic elements");
    Elements ci{2, 0, .8, .3, 0, 1.1};
    check(close(to_elements(from_elements(ci, mu), mu).nu, ci.nu, 1e-8),
          "circular inclined anomaly");
    Elements eq{2, .4, 0, .3, .5, 1.1};
    Elements eb = to_elements(from_elements(eq, mu), mu);
    check(close(eb.argp + eb.raan, .8, 1e-8) && close(eb.nu, eq.nu, 1e-8),
          "equatorial periapsis longitude");
    for (double eccentricity : {0.0, 0.4}) {
      State original =
          from_elements({2, eccentricity, 3.141592653589793, 0.3, 0.5, 1.1}, 1);
      State restored = from_elements(to_elements(original, 1), 1);
      check(mag(delta(original.r, restored.r)) < 1e-10 &&
                mag(delta(original.v, restored.v)) < 1e-10,
            "retrograde equatorial state roundtrip");
    }
    State weak = from_elements({2, .1, .2, .3, .4, .5}, 0.000000000000001);
    check(close(to_elements(weak, 0.000000000000001).a, 2, 1e-10),
          "elements scale-independent binding energy");
    Matrix h = hohmann(1, 2, mu);
    check(close(h.get(0, 0), std::sqrt(4.0 / 3.0) - 1, 1e-12) &&
              close(h.get(0, 1), 1 / std::sqrt(2.0) - std::sqrt(1.0 / 3.0),
                    1e-12),
          "Hohmann textbook delta-v");
    Matrix l = lambert({1, 0, 0}, {0, 1, 0}, 3.141592653589793 / 2, mu, true);
    check(close(l.get(0, 1), 1, 1e-7) && close(l.get(1, 0), -1, 1e-7),
          "Lambert circular quarter orbit");
    Matrix short_l = lambert({1, 0, 0}, {0, 1, 0}, .01, mu, true);
    check(std::isfinite(short_l.get(0, 0)), "Lambert short transfer");
    Matrix inclined_l = lambert({1, 0, 0}, {0, 0, 1}, 1, mu, true);
    Matrix retro_l = lambert({1, 0, 0}, {0, 0, 1}, 1, mu, false);
    check(std::isfinite(inclined_l.get(0, 0)) &&
              std::isfinite(retro_l.get(0, 0)),
          "Lambert inclined and retrograde");
    for (double transfer_time : {.01, .1, .5, 1.0, 3.0, 10.0, 100.0}) {
      Matrix velocities = lambert({1, 0, 0}, {0, 1, 0}, transfer_time, 1, true);
      State endpoint = propagate_kepler(
          {{1, 0, 0}, {velocities.get(0, 0), velocities.get(0, 1), 0}}, 1,
          transfer_time);
      check(mag(delta(endpoint.r, {0, 1, 0})) < 0.0000001,
            "Lambert endpoint across flight times");
    }
    Matrix tiny = propagate({{"free", 1, {{0, 0, 0}, {1, 0, 0}}}}, 1e-16, 2,
                            1e-17, "rk45", 1e-10);
    check(tiny.get(1, 0) == 1e-16 && std::abs(tiny.get(1, 1) - 1e-16) < 1e-28,
          "subfemtosecond output time");
    check(close(period(384400.0, 398600.435436 + 4902.800066) / 86400.0, 27.284,
                .002),
          "Earth Moon period");
    try {
      (void)period(std::numeric_limits<double>::quiet_NaN(), 1);
      throw std::runtime_error("period accepted NaN");
    } catch (const std::invalid_argument &) {
    }
    std::vector<Body> b{{"a", 1, {{-.5, 0, 0}, {0, -.7071067811865475, 0}}},
                        {"b", 1, {{.5, 0, 0}, {0, .7071067811865475, 0}}}};
    double e0 = energy(b);
    Vec3 l0 = angular_momentum(b);
    Matrix p = propagate(b, 1, 11, .01, "symplectic", 1e-10);
    check(p.rows == 11 && p.cols == 13, "symplectic shape");
    std::vector<Body> end = b;
    for (int i = 0; i < 2; ++i) {
      end[i].state.r = {p.get(10, 1 + 6 * i), p.get(10, 2 + 6 * i),
                        p.get(10, 3 + 6 * i)};
      end[i].state.v = {p.get(10, 4 + 6 * i), p.get(10, 5 + 6 * i),
                        p.get(10, 6 + 6 * i)};
    }
    check(std::abs(energy(end) - e0) < 1e-4, "symplectic energy conservation");
    check(mag(delta(angular_momentum(end), l0)) < 1e-8,
          "symplectic angular momentum conservation");
    Matrix rk = propagate(b, .1, 3, .02, "rk45", 1e-10);
    State expected = propagate_kepler(b[0].state, .25, .1);
    check(rk.rows == 3 && mag(delta({rk.get(2, 1), rk.get(2, 2), rk.get(2, 3)},
                                    expected.r)) < 1e-7,
          "RK45 two body accuracy");
    std::vector<Body> three{{"a", 1, {{-1, 0, 0}, {0, -.3, 0}}},
                            {"b", 1, {{1, 0, 0}, {0, .3, 0}}},
                            {"c", .01, {{0, 2, 0}, {-.2, 0, 0}}}};
    double three_e = energy(three);
    Vec3 three_l = angular_momentum(three);
    Matrix three_out = propagate(three, .1, 11, .001, "rk45", 1e-10);
    std::vector<Body> three_end = three;
    for (int i = 0; i < 3; ++i) {
      three_end[i].state.r = {three_out.get(10, 1 + 6 * i),
                              three_out.get(10, 2 + 6 * i),
                              three_out.get(10, 3 + 6 * i)};
      three_end[i].state.v = {three_out.get(10, 4 + 6 * i),
                              three_out.get(10, 5 + 6 * i),
                              three_out.get(10, 6 + 6 * i)};
    }
    check(std::abs(energy(three_end) - three_e) < 1e-8 &&
              mag(delta(angular_momentum(three_end), three_l)) < 1e-8,
          "three body conservation");
    std::cout << checks << " core physics checks passed\n";
    return 0;
  } catch (const std::exception &ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }
}
