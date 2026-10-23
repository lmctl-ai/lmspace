#include "orbit.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace lmspace {
namespace {
constexpr double PI = 3.141592653589793238462643383279502884;
constexpr double TWO_PI = 2.0 * PI;
constexpr double EPS = 1e-13;

Vec3 add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 sub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 mul(Vec3 a, double k) { return {a.x * k, a.y * k, a.z * k}; }
double dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 cross(Vec3 a, Vec3 b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
double norm(Vec3 a) { return std::sqrt(dot(a, a)); }
double wrap(double x) {
  x = std::fmod(x, TWO_PI);
  return x < -PI ? x + TWO_PI : (x > PI ? x - TWO_PI : x);
}
void check_mu(double mu) {
  if (!(mu > 0) || !std::isfinite(mu))
    throw std::invalid_argument("mu must be positive and finite");
}
bool finite3(Vec3 a) {
  return std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z);
}
double positive_angle(double x) {
  x = std::fmod(x, TWO_PI);
  return x < 0 ? x + TWO_PI : x;
}

double stump_c(double z) {
  if (std::abs(z) < 1e-7)
    return .5 - z / 24 + z * z / 720 - z * z * z / 40320;
  if (z > 0) {
    const double q = std::sqrt(z);
    return (1 - std::cos(q)) / z;
  }
  const double q = std::sqrt(-z);
  return (std::cosh(q) - 1) / (-z);
}
double stump_s(double z) {
  if (std::abs(z) < 1e-7)
    return 1. / 6 - z / 120 + z * z / 5040 - z * z * z / 362880;
  if (z > 0) {
    const double q = std::sqrt(z);
    return (q - std::sin(q)) / (q * q * q);
  }
  const double q = std::sqrt(-z);
  return (std::sinh(q) - q) / (q * q * q);
}

using V = std::vector<double>;
V deriv(const V &y, const std::vector<Body> &b) {
  const int n = static_cast<int>(b.size());
  V d(6 * n, 0.0);
  for (int i = 0; i < n; ++i) {
    d[6 * i] = y[6 * i + 3];
    d[6 * i + 1] = y[6 * i + 4];
    d[6 * i + 2] = y[6 * i + 5];
    Vec3 ri{y[6 * i], y[6 * i + 1], y[6 * i + 2]}, a{};
    for (int j = 0; j < n; ++j)
      if (i != j) {
        Vec3 dr = sub(Vec3{y[6 * j], y[6 * j + 1], y[6 * j + 2]}, ri);
        double r = norm(dr);
        if (r < EPS)
          throw std::domain_error("body collision or zero separation");
        a = add(a, mul(dr, b[j].mu / (r * r * r)));
      }
    d[6 * i + 3] = a.x;
    d[6 * i + 4] = a.y;
    d[6 * i + 5] = a.z;
  }
  return d;
}
V plus(const V &a, const V &b, double s) {
  V x = a;
  for (size_t i = 0; i < x.size(); ++i)
    x[i] += s * b[i];
  return x;
}
bool finite_v(const V &x) {
  for (double q : x)
    if (!std::isfinite(q))
      return false;
  return true;
}
double error_norm(const V &a, const V &b, const V &y, double tol) {
  double q = 0;
  for (size_t i = 0; i < a.size(); ++i) {
    double v = std::abs(a[i] - b[i]) / (tol * (1 + std::abs(y[i])));
    if (!std::isfinite(v))
      return std::numeric_limits<double>::infinity();
    q = std::max(q, v);
  }
  return q;
}

} // namespace

double Matrix::get(int row, int col) const {
  if (row < 0 || row >= rows || col < 0 || col >= cols)
    throw std::out_of_range("matrix index outside matrix");
  return data[static_cast<size_t>(row) * cols + col];
}

double kepler(double M, double e) {
  if (!std::isfinite(M) || !(e >= 0) || !std::isfinite(e))
    throw std::invalid_argument(
        "Kepler inputs must be finite and eccentricity nonnegative");
  if (M == 0)
    return 0;
  if (e == 1) {
    // Barker's equation D+D^3/3=M, including arbitrarily small nonzero M.
    const double scaled_asinh =
        std::abs(M) > 1e150 ? std::asinh(M) + std::copysign(std::log(1.5), M)
                            : std::asinh(1.5 * M);
    return 2 * std::sinh(scaled_asinh / 3);
  }
  const bool hyperbolic = e > 1;
  const double mean = hyperbolic ? M : wrap(M);
  // The elliptic equation is solved for the wrapped mean anomaly; adding the
  // removed whole revolutions back keeps E - e sin(E) = M for any M.
  const double turns = M - mean;
  if (mean == 0)
    return turns;
  // Evaluating E-e*sin(E) directly loses precision close to e=1, E=0.
  // Sum the small odd remainder separately from the linear term.
  auto remainder = [hyperbolic](double x) {
    if (std::abs(x) >= .1)
      return hyperbolic ? std::sinh(x) - x : x - std::sin(x);
    double term = x * x * x / 6, sum = term;
    for (int n = 5; n <= 15; n += 2) {
      term *= (hyperbolic ? 1 : -1) * x * x / ((n - 1) * n);
      sum += term;
    }
    return sum;
  };
  auto equation = [&](double x) {
    return (hyperbolic ? e - 1 : 1 - e) * x + e * remainder(x) - mean;
  };
  double lo = -PI, hi = PI, x = mean;
  if (hyperbolic) {
    x = std::asinh(mean / e);
    hi = std::max(1.0, std::abs(x) + 1);
    while (equation(hi) < 0)
      hi *= 2;
    lo = -hi;
    while (equation(lo) > 0)
      lo *= 2;
  }
  for (int k = 0; k < 150; ++k) {
    const double f = equation(x);
    const double half = hyperbolic ? std::sinh(x / 2) : std::sin(x / 2);
    const double derivative =
        (hyperbolic ? e - 1 : 1 - e) + 2 * e * half * half;
    const double correction = f / derivative;
    if (f == 0 ||
        std::abs(correction) <= 8 * std::numeric_limits<double>::epsilon() *
                                    std::max(std::abs(x), 1e-8))
      return x + turns;
    if (f > 0)
      hi = x;
    else
      lo = x;
    const double next = x - correction;
    x = (next > lo && next < hi && std::isfinite(next)) ? next : (lo + hi) / 2;
  }
  throw std::runtime_error("Kepler solver did not converge");
}

double period(double a, double mu) {
  check_mu(mu);
  if (!std::isfinite(a) || !(a > 0))
    throw std::invalid_argument("semimajor axis must be positive and finite");
  return TWO_PI * std::sqrt(a * a * a / mu);
}

State from_elements(Elements q, double mu) {
  check_mu(mu);
  if (!std::isfinite(q.a) || !std::isfinite(q.e) || !std::isfinite(q.i) ||
      !std::isfinite(q.raan) || !std::isfinite(q.argp) ||
      !std::isfinite(q.nu) || !(q.e >= 0) || !(q.a != 0))
    throw std::invalid_argument("invalid orbital elements");
  double p = q.a * (1 - q.e * q.e);
  if (!(p > 0))
    throw std::invalid_argument("elements have nonpositive semilatus rectum");
  double c = std::cos(q.nu), s = std::sin(q.nu), r = p / (1 + q.e * c),
         h = std::sqrt(mu * p);
  if (!std::isfinite(r) || !(r > 0))
    throw std::invalid_argument("true anomaly is outside the physical orbit");
  Vec3 rp{r * c, r * s, 0}, vp{-mu * s / h, mu * (q.e + c) / h, 0};
  double cr = std::cos(q.raan), sr = std::sin(q.raan), ci = std::cos(q.i),
         si = std::sin(q.i), cw = std::cos(q.argp), sw = std::sin(q.argp);
  auto rot = [&](Vec3 x) {
    return Vec3{
        (cr * cw - sr * sw * ci) * x.x + (-cr * sw - sr * cw * ci) * x.y,
        (sr * cw + cr * sw * ci) * x.x + (-sr * sw + cr * cw * ci) * x.y,
        (sw * si) * x.x + (cw * si) * x.y};
  };
  return {rot(rp), rot(vp)};
}

Elements to_elements(State s, double mu) {
  check_mu(mu);
  if (!finite3(s.r) || !finite3(s.v))
    throw std::invalid_argument("state must be finite");
  double r = norm(s.r), v = norm(s.v);
  if (r < EPS)
    throw std::invalid_argument("position is zero");
  Vec3 h = cross(s.r, s.v);
  double hn = norm(h);
  if (hn < EPS)
    throw std::invalid_argument("angular momentum is zero");
  Vec3 n{-h.y, h.x, 0};
  double nn = norm(n);
  Vec3 evec = sub(mul(cross(s.v, h), 1 / mu), mul(s.r, 1 / r));
  double e = norm(evec), energy2 = v * v / 2 - mu / r;
  double a = std::abs(energy2) < 8 * std::numeric_limits<double>::epsilon() *
                                     (v * v / 2 + mu / r)
                 ? std::numeric_limits<double>::infinity()
                 : -mu / (2 * energy2);
  double inc = std::acos(std::clamp(h.z / hn, -1.0, 1.0)),
         raan = nn < EPS ? 0 : positive_angle(std::atan2(n.y, n.x)), argp = 0,
         nu = 0;
  if (e >= EPS) {
    argp =
        nn < EPS
            ? positive_angle((h.z < 0 ? -1 : 1) * std::atan2(evec.y, evec.x))
            : positive_angle(std::atan2(dot(cross(n, evec), h) / (nn * e * hn),
                                        dot(n, evec) / (nn * e)));
    nu = positive_angle(std::atan2(dot(cross(evec, s.r), h) / (e * r * hn),
                                   dot(evec, s.r) / (e * r)));
  } else if (nn >= EPS)
    nu = positive_angle(std::atan2(dot(cross(n, s.r), h) / (nn * r * hn),
                                   dot(n, s.r) / (nn * r)));
  else
    nu = positive_angle((h.z < 0 ? -1 : 1) * std::atan2(s.r.y, s.r.x));
  return {a, e, inc, raan, argp, nu};
}

State propagate_kepler(State s, double mu, double dt) {
  check_mu(mu);
  if (!std::isfinite(dt))
    throw std::invalid_argument("propagation time must be finite");
  Elements q = to_elements(s, mu);
  if (!std::isfinite(q.a))
    throw std::invalid_argument("parabolic propagation is not supported");
  double M0;
  if (q.e < 1) {
    double E = 2 * std::atan2(std::sqrt(1 - q.e) * std::sin(q.nu / 2),
                              std::sqrt(1 + q.e) * std::cos(q.nu / 2));
    M0 = E - q.e * std::sin(E);
    double n = std::sqrt(mu / (q.a * q.a * q.a));
    q.nu = 2 * std::atan2(
                   std::sqrt(1 + q.e) * std::sin(kepler(M0 + n * dt, q.e) / 2),
                   std::sqrt(1 - q.e) * std::cos(kepler(M0 + n * dt, q.e) / 2));
  } else {
    double H =
        2 * std::atanh(std::sqrt((q.e - 1) / (q.e + 1)) * std::tan(q.nu / 2));
    double M = q.e * std::sinh(H) - H + std::sqrt(mu / (-q.a * q.a * q.a)) * dt;
    double hh = kepler(M, q.e);
    q.nu = 2 * std::atan2(std::sqrt(q.e + 1) * std::sinh(hh / 2),
                          std::sqrt(q.e - 1) * std::cosh(hh / 2));
  }
  return from_elements(q, mu);
}

Matrix lambert(Vec3 r1, Vec3 r2, double dt, double mu, bool prograde) {
  check_mu(mu);
  if (!(dt > 0) || !std::isfinite(dt) || !finite3(r1) || !finite3(r2))
    throw std::invalid_argument("invalid Lambert inputs");
  double R1 = norm(r1), R2 = norm(r2);
  if (R1 < EPS || R2 < EPS)
    throw std::invalid_argument("Lambert position is zero");
  double c12 = dot(r1, r2) / (R1 * R2);
  c12 = std::clamp(c12, -1.0, 1.0);
  Vec3 cr = cross(r1, r2);
  if (norm(cr) < EPS * R1 * R2)
    throw std::invalid_argument("Lambert geometry is collinear");
  double dtheta = std::acos(c12);
  if ((cr.z < 0) != (!prograde))
    dtheta = TWO_PI - dtheta;
  double A = std::sin(dtheta) * std::sqrt(R1 * R2 / (1 - std::cos(dtheta)));
  if (std::abs(A) < EPS)
    throw std::invalid_argument("Lambert geometry is singular");
  auto F = [&](double z, double *y) {
    double C = stump_c(z), S = stump_s(z);
    double yy = R1 + R2 + A * (z * S - 1) / std::sqrt(C);
    if (yy < 0)
      return std::numeric_limits<double>::quiet_NaN();
    if (y)
      *y = yy;
    return std::pow(yy / C, 1.5) * S + A * std::sqrt(yy) - std::sqrt(mu) * dt;
  };
  // The zero-revolution time of flight increases monotonically below z=4*pi^2.
  // Negative z can cross y=0; find that physical boundary instead of sampling
  // a grid, which misses the very short transfer branch.
  double lo = 0, hi = 0, flo = F(0, nullptr), fhi = flo;
  if (!std::isfinite(flo))
    throw std::runtime_error("Lambert invalid time-of-flight geometry");
  if (flo > 0) {
    double valid = 0;
    lo = -1;
    for (int k = 0; k < 60; ++k) {
      flo = F(lo, nullptr);
      if (!std::isfinite(flo)) {
        double invalid = lo;
        lo = valid;
        for (int j = 0; j < 100; ++j) {
          const double trial = (invalid + lo) / 2;
          const double value = F(trial, nullptr);
          if (!std::isfinite(value))
            invalid = trial;
          else {
            lo = trial;
            flo = value;
          }
          if (std::isfinite(value) && value <= 0)
            break;
        }
        break;
      }
      if (flo <= 0)
        break;
      valid = lo;
      lo *= 2;
    }
  } else if (flo < 0) {
    const double limit = 4 * PI * PI;
    hi = 1;
    for (int k = 0; k < 100; ++k) {
      fhi = F(hi, nullptr);
      if (std::isfinite(fhi) && fhi >= 0)
        break;
      hi = (hi + limit) / 2;
    }
  }
  if (!std::isfinite(flo) || !std::isfinite(fhi) || flo > 0 || fhi < 0)
    throw std::runtime_error("Lambert solver could not bracket a solution");
  double z = 0, fz = 0;
  for (int k = 0; k < 120; ++k) {
    z = .5 * (lo + hi);
    fz = F(z, nullptr);
    if (!std::isfinite(fz)) {
      lo = z;
      continue;
    }
    if (std::abs(fz) <= 1e-11 * std::max(1.0, std::sqrt(mu) * dt))
      break;
    if (flo * fz <= 0) {
      hi = z;
    } else {
      lo = z;
      flo = fz;
    }
  }
  double y;
  fz = F(z, &y);
  if (!std::isfinite(fz) ||
      std::abs(fz) > 1e-9 * std::max(1.0, std::sqrt(mu) * dt) || !(y > 0))
    throw std::runtime_error("Lambert solver did not converge");
  double f = 1 - y / R1, g = A * std::sqrt(y / mu), gdot = 1 - y / R2;
  if (std::abs(g) < EPS)
    throw std::runtime_error("Lambert solution is singular");
  Vec3 v1 = mul(sub(r2, mul(r1, f)), 1 / g),
       v2 = mul(sub(mul(r2, gdot), r1), 1 / g);
  Matrix m{2, 3, {v1.x, v1.y, v1.z, v2.x, v2.y, v2.z}};
  return m;
}

Matrix hohmann(double r1, double r2, double mu) {
  check_mu(mu);
  if (!std::isfinite(r1) || !std::isfinite(r2) || !(r1 > 0 && r2 > 0))
    throw std::invalid_argument("radii must be positive and finite");
  double a = .5 * (r1 + r2), v1 = std::sqrt(mu / r1), v2 = std::sqrt(mu / r2),
         vp = std::sqrt(mu * (2 / r1 - 1 / a)),
         va = std::sqrt(mu * (2 / r2 - 1 / a));
  return {1, 3, {vp - v1, v2 - va, PI * std::sqrt(a * a * a / mu)}};
}

double energy(const std::vector<Body> &b) {
  double e = 0;
  for (const auto &x : b)
    e += .5 * x.mu * dot(x.state.v, x.state.v);
  for (size_t i = 0; i < b.size(); ++i)
    for (size_t j = i + 1; j < b.size(); ++j) {
      double r = norm(sub(b[i].state.r, b[j].state.r));
      if (r < EPS)
        throw std::domain_error("body collision or zero separation");
      e -= b[i].mu * b[j].mu / r;
    }
  return e;
}
Vec3 angular_momentum(const std::vector<Body> &b) {
  Vec3 h{};
  for (const auto &x : b)
    h = add(h, mul(cross(x.state.r, x.state.v), x.mu));
  return h;
}

double energy_scale(const std::vector<Body> &b) {
  double s = 0;
  for (const auto &x : b)
    s += .5 * x.mu * dot(x.state.v, x.state.v);
  for (size_t i = 0; i < b.size(); ++i)
    for (size_t j = i + 1; j < b.size(); ++j) {
      double r = norm(sub(b[i].state.r, b[j].state.r));
      if (r >= EPS)
        s += b[i].mu * b[j].mu / r;
    }
  return s;
}

Matrix propagate(std::vector<Body> b, double duration, int points, double step,
                 const std::string &method, double tol, double *max_energy_drift) {
  if (b.empty())
    throw std::invalid_argument("propagation needs at least one body");
  if (points < 2 || points > 1000000 || !std::isfinite(duration) ||
      duration < 0 || !std::isfinite(step) || step <= 0 ||
      !std::isfinite(tol) || tol <= 0)
    throw std::invalid_argument("invalid propagation grid or tolerance");
  if (method != "symplectic" && method != "rk45")
    throw std::invalid_argument("unknown propagation method");
  for (const auto &x : b) {
    check_mu(x.mu);
    if (!finite3(x.state.r) || !finite3(x.state.v))
      throw std::invalid_argument("body state must be finite");
  }
  int n = static_cast<int>(b.size());
  if (n > 10000)
    throw std::invalid_argument("too many bodies");
  V y(6 * n);
  for (int i = 0; i < n; ++i) {
    y[6 * i] = b[i].state.r.x;
    y[6 * i + 1] = b[i].state.r.y;
    y[6 * i + 2] = b[i].state.r.z;
    y[6 * i + 3] = b[i].state.v.x;
    y[6 * i + 4] = b[i].state.v.y;
    y[6 * i + 5] = b[i].state.v.z;
  }
  const size_t cells =
      static_cast<size_t>(points) * (1 + 6 * static_cast<size_t>(n));
  if (cells > 100000000)
    throw std::invalid_argument("propagation output is too large");
  Matrix out{points, 1 + 6 * n, {}};
  out.data.resize(cells);
  double t = 0;
  int work = 0;
  // Energy is sampled on at most ~256 output rows (plus the last) so the check
  // stays cheap next to the integration itself.
  const bool track = max_energy_drift != nullptr && n > 1;
  const int stride = std::max(1, (points - 1) / 256);
  const double e0 = track ? energy(b) : 0, scale = track ? energy_scale(b) : 0;
  if (max_energy_drift)
    *max_energy_drift = 0;
  auto emit = [&](int row) {
    out.data[row * out.cols] = t;
    for (int k = 0; k < 6 * n; ++k)
      out.data[row * out.cols + 1 + k] = y[k];
    if (track && scale > 0 && (row % stride == 0 || row == points - 1)) {
      std::vector<Body> now = b;
      for (int i = 0; i < n; ++i)
        now[i].state = State{{y[6 * i], y[6 * i + 1], y[6 * i + 2]},
                             {y[6 * i + 3], y[6 * i + 4], y[6 * i + 5]}};
      double drift;
      try {
        drift = std::abs(energy(now) - e0) / scale;
      } catch (const std::domain_error &) {
        drift = std::numeric_limits<double>::infinity();
      }
      *max_energy_drift = std::max(*max_energy_drift, drift);
    }
  };
  emit(0);
  for (int row = 1; row < points; ++row) {
    double target = duration * (static_cast<double>(row) / (points - 1));
    while (t < target) {
      double h = std::min(step, target - t);
      if (!(h > 0) || t + h == t)
        throw std::runtime_error("integration made no progress");
      if (++work > 1000000)
        throw std::runtime_error("integration work limit exceeded");
      if (method == "symplectic") {
        V d = deriv(y, b);
        for (int i = 0; i < n; ++i) {
          y[6 * i + 3] += .5 * h * d[6 * i + 3];
          y[6 * i + 4] += .5 * h * d[6 * i + 4];
          y[6 * i + 5] += .5 * h * d[6 * i + 5];
        }
        for (int i = 0; i < n; ++i) {
          y[6 * i] += h * y[6 * i + 3];
          y[6 * i + 1] += h * y[6 * i + 4];
          y[6 * i + 2] += h * y[6 * i + 5];
        }
        d = deriv(y, b);
        for (int i = 0; i < n; ++i) {
          y[6 * i + 3] += .5 * h * d[6 * i + 3];
          y[6 * i + 4] += .5 * h * d[6 * i + 4];
          y[6 * i + 5] += .5 * h * d[6 * i + 5];
        }
        if (!finite_v(y))
          throw std::runtime_error("symplectic numerical overflow");
        t += h;
      } else {
        V k1 = deriv(y, b), k2 = deriv(plus(y, k1, h * .2), b),
          k3 = deriv(plus(plus(y, k1, h * 3. / 40), k2, h * 9. / 40), b),
          k4 = deriv(plus(plus(plus(y, k1, h * 44. / 45), k2, h * -56. / 15),
                          k3, h * 32. / 9),
                     b),
          k5 = deriv(plus(plus(plus(plus(y, k1, h * 19372. / 6561), k2,
                                    h * -25360. / 2187),
                               k3, h * 64448. / 6561),
                          k4, h * -212. / 729),
                     b),
          k6 = deriv(plus(plus(plus(plus(plus(y, k1, h * 9017. / 3168), k2,
                                         h * -355. / 33),
                                    k3, h * 46732. / 5247),
                               k4, h * 49. / 176),
                          k5, h * -5103. / 18656),
                     b);
        V y5 = plus(plus(plus(plus(plus(plus(y, k1, h * 35. / 384), k3,
                                        h * 500. / 1113),
                                   k4, h * 125. / 192),
                              k5, h * -2187. / 6784),
                         k6, h * 11. / 84),
                    V(y.size(), 0), 0);
        V k7 = deriv(y5, b),
          y4 = plus(plus(plus(plus(plus(plus(plus(y, k1, h * 5179. / 57600), k3,
                                             h * 7571. / 16695),
                                        k4, h * 393. / 640),
                                   k5, h * -92097. / 339200),
                              k6, h * 187. / 2100),
                         k7, h * 1. / 40),
                    V(y.size(), 0), 0);
        if (!finite_v(y5) || !finite_v(y4))
          throw std::runtime_error("RK45 numerical overflow");
        double er = error_norm(y5, y4, y, tol);
        double factor =
            std::clamp(.9 * std::pow(std::max(er, 1e-12), -.2), .2, 5.0);
        if (er <= 1) {
          y = y5;
          t += h;
          step = h * factor;
        } else {
          step = h * std::max(.1, factor);
          if (step < 1e-14 * (1 + std::abs(t)))
            throw std::runtime_error("RK45 step underflow");
        }
      }
    }
    emit(row);
  }
  return out;
}
} // namespace lmspace
