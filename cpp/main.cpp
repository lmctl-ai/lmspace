// lmspace: orbital mechanics and astronomy scripting host.
#include "dslvm.hpp"
#include "memory_budget.hpp"
#include "notices_embedded.hpp"
#include "orbit.hpp"
#include "schema_embedded.hpp"
#include "spice.hpp"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
#ifndef LMSPACE_VERSION
#define LMSPACE_VERSION "0.1.3"
#endif

namespace {
using dslvm::Args;
using dslvm::Value;
using lmspace::Body;
using lmspace::Elements;
using lmspace::Matrix;
using lmspace::State;
using lmspace::Vec3;

struct System;
struct Host {
  std::vector<std::unique_ptr<System>> systems;
  std::vector<std::unique_ptr<Elements>> elements;
  std::vector<std::unique_ptr<Matrix>> matrices;
  std::vector<std::unique_ptr<Vec3>> vectors;
  std::vector<std::unique_ptr<State>> states;
  template <class T>
  Value hold(std::vector<std::unique_ptr<T>> &a, const char *type, T x) {
    if (a.size() >= 4000000)
      throw dslvm::Error(std::string(type) + " object limit exceeded");
    a.push_back(std::make_unique<T>(std::move(x)));
    return Value::object(type, a.back().get());
  }
  Value matrix(Matrix x) { return hold(matrices, "Matrix", std::move(x)); }
  Value vector(Vec3 x) { return hold(vectors, "Vector", std::move(x)); }
  Value state(State x) { return hold(states, "State", std::move(x)); }
};
struct System {
  std::vector<Body> bodies;
  double drift = 0; // relative energy drift of the last propagate()
};
struct Space {
  std::vector<std::unique_ptr<Body>> bodies;
  std::vector<std::unique_ptr<System>> systems;
  double drift = 0;
};

// A fixed-step run through a close encounter (or a far too coarse step) returns
// a wrong trajectory without failing; the energy drift exposes it.
constexpr double kDriftWarning = 1e-3;
Matrix checked_propagate(const std::vector<Body> &b, Args &a, int rows,
                         double &drift) {
  Matrix m = lmspace::propagate(b, a[0].f, rows, a[2].f, a[3].s, a[4].f, &drift);
  if (drift > kDriftWarning) {
    char line[256];
    std::snprintf(line, sizeof line,
                  "lmspace: warning: %s propagation changed the system energy by "
                  "%.3g (relative); the trajectory is unreliable, use a smaller step "
                  "or \"rk45\" (close encounter?)\n",
                  a[3].s.c_str(), drift);
    std::fputs(line, stderr);
  }
  return m;
}

template <class F> Value translate(F &&f) {
  try {
    return f();
  } catch (const dslvm::Error &) {
    throw;
  } catch (const std::exception &e) {
    throw dslvm::Error(e.what());
  }
}

void finite(double x, const char *who) {
  if (!std::isfinite(x))
    throw dslvm::Error(std::string(who) + ": value must be finite");
}
void check(dslvm::Registry &r, const char *type, const char *name,
           std::vector<std::string> ps, std::string ret, dslvm::Method fn) {
  const std::string who = std::string(type) + "." + name;
  r.method(type, name, ps, ret, [who, ps, fn](void *self, Args &a) {
    for (size_t i = 0; i < ps.size(); ++i) {
      const auto &p = a[i];
      const bool ok = ps[i] == "float"    ? p.kind == Value::FLOAT
                      : ps[i] == "int"    ? p.kind == Value::INT
                      : ps[i] == "string" ? p.kind == Value::STR
                      : ps[i] == "bool"
                          ? p.kind == Value::BOOL
                          : p.kind == Value::OBJ && p.obj && p.type == ps[i];
      if (!ok)
        throw dslvm::Error(who + " argument " + std::to_string(i + 1) +
                           " must be " + ps[i]);
    }
    return translate([&] { return fn(self, a); });
  });
}
void checkfn(dslvm::Registry &r, const char *name, std::vector<std::string> ps,
             std::string ret, dslvm::Function fn) {
  r.function(name, ps, ret, [name, ps, fn](Args &a) {
    for (size_t i = 0; i < ps.size(); ++i) {
      const auto &p = a[i];
      const bool ok = ps[i] == "float"    ? p.kind == Value::FLOAT
                      : ps[i] == "int"    ? p.kind == Value::INT
                      : ps[i] == "string" ? p.kind == Value::STR
                      : ps[i] == "bool"
                          ? p.kind == Value::BOOL
                          : p.kind == Value::OBJ && p.obj && p.type == ps[i];
      if (!ok)
        throw dslvm::Error(std::string(name) + " argument " +
                           std::to_string(i + 1) + " must be " + ps[i]);
    }
    return translate([&] { return fn(a); });
  });
}
Vec3 v3(Args &a, size_t i) { return Vec3{a[i].f, a[i + 1].f, a[i + 2].f}; }
Vec3 &V(void *p) { return *static_cast<Vec3 *>(p); }
State &S(void *p) { return *static_cast<State *>(p); }
Body &B(void *p) { return *static_cast<Body *>(p); }
Elements &E(void *p) { return *static_cast<Elements *>(p); }
Matrix &M(void *p) { return *static_cast<Matrix *>(p); }
Space &P(void *p) { return *static_cast<Space *>(p); }
Body &ab(Value &v) { return B(v.obj); }
State &as(Value &v) { return S(v.obj); }
Elements &ae(Value &v) { return E(v.obj); }
void index(const Matrix &m, int64_t i, int64_t j, const char *who) {
  if (i < 0 || j < 0 || i >= m.rows || j >= m.cols)
    throw dslvm::Error(std::string(who) + ": index (" + std::to_string(i) +
                       "," + std::to_string(j) + ") outside " +
                       std::to_string(m.rows) + "x" + std::to_string(m.cols));
}
int points(int64_t count) {
  if (count < 2 || count > 1000000)
    throw dslvm::Error("propagation grid points must be in [2,1000000]");
  return static_cast<int>(count);
}
// Check the budget for a propagation table before integrating (rows x (1 + 6 bodies) doubles);
// sizes over propagate()'s own 1e8-cell cap are left to its clearer message.
int grid(int64_t count, size_t bodies) {
  const int rows = points(count);
  const size_t cells = static_cast<size_t>(rows) * (1 + 6 * bodies);
  if (bodies <= 10000 && cells <= 100000000)
    membudget::require(cells * sizeof(double));
  return rows;
}
std::string fmt(double x) {
  char b[48];
  std::snprintf(b, sizeof b, "%.17g", x);
  return b;
}

void register_host(dslvm::Registry &r, Host &h) {
  r.context("space", "Space");
  check(r, "Space", "body", {"string", "float"}, "Body", [](void *p, Args &a) {
    Body b;
    b.name = a[0].s;
    b.mu = a[1].f;
    finite(b.mu, "Space.body");
    if (b.mu <= 0)
      throw dslvm::Error("Space.body: mu must be positive");
    P(p).bodies.push_back(std::make_unique<Body>(std::move(b)));
    return Value::object("Body", P(p).bodies.back().get());
  });
  check(
      r, "Space", "body",
      {"string", "float", "float", "float", "float", "float", "float", "float"},
      "Body", [](void *p, Args &a) {
        for (size_t i = 1; i < a.size(); ++i)
          finite(a[i].f, "Space.body");
        Body b;
        b.name = a[0].s;
        b.mu = a[1].f;
        if (b.mu <= 0)
          throw dslvm::Error("Space.body: mu must be positive");
        b.state = State{v3(a, 2), v3(a, 5)};
        P(p).bodies.push_back(std::make_unique<Body>(std::move(b)));
        return Value::object("Body", P(p).bodies.back().get());
      });
  check(r, "Space", "system", {}, "System", [&h](void *, Args &) {
    h.systems.push_back(std::make_unique<System>());
    return Value::object("System", h.systems.back().get());
  });
  check(r, "Space", "propagate", {"float", "int", "float", "string", "float"},
        "Matrix", [&h](void *p, Args &a) {
          std::vector<Body> b;
          for (const auto &x : P(p).bodies)
            b.push_back(*x);
          return h.matrix(checked_propagate(b, a, grid(a[1].i, b.size()), P(p).drift));
        });
  check(r, "Space", "energy_drift", {}, "float",
        [](void *p, Args &) { return Value::real(P(p).drift); });
  check(r, "Space", "energy", {}, "float", [](void *p, Args &) {
    std::vector<Body> b;
    for (const auto &x : P(p).bodies)
      b.push_back(*x);
    return Value::real(lmspace::energy(b));
  });
  check(r, "Space", "angular_momentum", {}, "Vector", [&h](void *p, Args &) {
    std::vector<Body> b;
    for (const auto &x : P(p).bodies)
      b.push_back(*x);
    return h.vector(lmspace::angular_momentum(b));
  });
  check(r, "Space", "load_kernel", {"string"}, "void", [](void *, Args &a) {
    lmspace::spice_load(a[0].s);
    return Value::none();
  });
  check(r, "Space", "clear_kernels", {}, "void", [](void *, Args &) {
    lmspace::spice_clear();
    return Value::none();
  });
  check(r, "Space", "ephemeris", {"string", "string", "string", "float"},
        "Matrix", [&h](void *, Args &a) {
          return h.matrix(lmspace::spice_state(a[0].s, a[1].s, a[2].s, a[3].f));
        });
  check(r, "Space", "frame", {"string", "string", "float"}, "Matrix",
        [&h](void *, Args &a) {
          return h.matrix(lmspace::spice_frame(a[0].s, a[1].s, a[2].f));
        });
  check(r, "Space", "time", {"string"}, "float", [](void *, Args &a) {
    return Value::real(lmspace::spice_time(a[0].s));
  });
  check(r, "System", "add", {"Body"}, "System", [](void *p, Args &a) {
    auto *s = static_cast<System *>(p);
    s->bodies.push_back(ab(a[0]));
    return Value::object("System", p);
  });
  check(r, "System", "propagate", {"float", "int", "float", "string", "float"},
        "Matrix", [&h](void *p, Args &a) {
          auto *s = static_cast<System *>(p);
          return h.matrix(checked_propagate(s->bodies, a, grid(a[1].i, s->bodies.size()),
                                            s->drift));
        });
  check(r, "System", "energy_drift", {}, "float", [](void *p, Args &) {
    return Value::real(static_cast<System *>(p)->drift);
  });
  check(r, "System", "energy", {}, "float", [](void *p, Args &) {
    return Value::real(lmspace::energy(static_cast<System *>(p)->bodies));
  });
  check(r, "System", "angular_momentum", {}, "Vector", [&h](void *p, Args &) {
    return h.vector(
        lmspace::angular_momentum(static_cast<System *>(p)->bodies));
  });

  check(r, "Body", "name", {}, "string",
        [](void *p, Args &) { return Value::string(B(p).name); });
  check(r, "Body", "mu", {}, "float",
        [](void *p, Args &) { return Value::real(B(p).mu); });
  check(r, "Body", "state", {}, "State",
        [&h](void *p, Args &) { return h.state(B(p).state); });
  check(r, "Body", "elements", {"float"}, "Elements", [&h](void *p, Args &a) {
    return h.hold(h.elements, "Elements",
                  lmspace::to_elements(B(p).state, a[0].f));
  });
  check(r, "Body", "propagate", {"float", "float"}, "State",
        [&h](void *p, Args &a) {
          return h.state(lmspace::propagate_kepler(B(p).state, a[0].f, a[1].f));
        });

  check(r, "State", "position", {}, "Vector",
        [&h](void *p, Args &) { return h.vector(S(p).r); });
  check(r, "State", "velocity", {}, "Vector",
        [&h](void *p, Args &) { return h.vector(S(p).v); });
  check(r, "State", "propagate", {"float", "float"}, "State",
        [&h](void *p, Args &a) {
          return h.state(lmspace::propagate_kepler(S(p), a[0].f, a[1].f));
        });
  check(r, "State", "elements", {"float"}, "Elements", [&h](void *p, Args &a) {
    return h.hold(h.elements, "Elements", lmspace::to_elements(S(p), a[0].f));
  });
  check(r, "Vector", "get", {"int"}, "float", [](void *p, Args &a) {
    if (a[0].i < 0 || a[0].i > 2)
      throw dslvm::Error("Vector.get: index outside 3");
    const Vec3 &v = V(p);
    return Value::real(a[0].i == 0 ? v.x : a[0].i == 1 ? v.y : v.z);
  });
  check(r, "Vector", "x", {}, "float",
        [](void *p, Args &) { return Value::real(V(p).x); });
  check(r, "Vector", "y", {}, "float",
        [](void *p, Args &) { return Value::real(V(p).y); });
  check(r, "Vector", "z", {}, "float",
        [](void *p, Args &) { return Value::real(V(p).z); });
  check(r, "Elements", "a", {}, "float",
        [](void *p, Args &) { return Value::real(E(p).a); });
  check(r, "Elements", "e", {}, "float",
        [](void *p, Args &) { return Value::real(E(p).e); });
  check(r, "Elements", "i", {}, "float",
        [](void *p, Args &) { return Value::real(E(p).i); });
  check(r, "Elements", "raan", {}, "float",
        [](void *p, Args &) { return Value::real(E(p).raan); });
  check(r, "Elements", "argp", {}, "float",
        [](void *p, Args &) { return Value::real(E(p).argp); });
  check(r, "Elements", "nu", {}, "float",
        [](void *p, Args &) { return Value::real(E(p).nu); });

  check(r, "Matrix", "rows", {}, "int",
        [](void *p, Args &) { return Value::integer(M(p).rows); });
  check(r, "Matrix", "cols", {}, "int",
        [](void *p, Args &) { return Value::integer(M(p).cols); });
  check(r, "Matrix", "get", {"int", "int"}, "float", [](void *p, Args &a) {
    index(M(p), a[0].i, a[1].i, "Matrix.get");
    return Value::real(M(p).get(a[0].i, a[1].i));
  });
  check(r, "Matrix", "save_csv", {"string"}, "void", [](void *p, Args &a) {
    std::ofstream f(a[0].s);
    if (!f)
      throw dslvm::Error("Matrix.save_csv: cannot open " + a[0].s);
    for (int i = 0; i < M(p).rows; i++) {
      for (int j = 0; j < M(p).cols; j++) {
        if (j)
          f << ',';
        f << fmt(M(p).get(i, j));
      }
      f << '\n';
    }
    f.flush();
    if (!f)
      throw dslvm::Error("Matrix.save_csv: write failed for " + a[0].s);
    return Value::none();
  });
  check(r, "Elements", "to_state", {"float"}, "State", [&h](void *p, Args &a) {
    return h.state(lmspace::from_elements(E(p), a[0].f));
  });

  checkfn(r, "vector", {"float", "float", "float"}, "Vector",
          [&h](Args &a) { return h.vector(v3(a, 0)); });
  checkfn(r, "state", {"Vector", "Vector"}, "State", [&h](Args &a) {
    return h.state(State{V(a[0].obj), V(a[1].obj)});
  });
  checkfn(r, "elements", {"float", "float", "float", "float", "float", "float"},
          "Elements", [&h](Args &a) {
            return h.hold(
                h.elements, "Elements",
                Elements{a[0].f, a[1].f, a[2].f, a[3].f, a[4].f, a[5].f});
          });
  checkfn(r, "kepler", {"float", "float"}, "float",
          [](Args &a) { return Value::real(lmspace::kepler(a[0].f, a[1].f)); });
  checkfn(r, "period", {"float", "float"}, "float",
          [](Args &a) { return Value::real(lmspace::period(a[0].f, a[1].f)); });
  checkfn(r, "from_elements", {"Elements", "float"}, "State", [&h](Args &a) {
    return h.state(lmspace::from_elements(ae(a[0]), a[1].f));
  });
  checkfn(r, "to_elements", {"State", "float"}, "Elements", [&h](Args &a) {
    return h.hold(h.elements, "Elements",
                  lmspace::to_elements(as(a[0]), a[1].f));
  });
  checkfn(r, "lambert", {"Vector", "Vector", "float", "float", "bool"},
          "Matrix", [&h](Args &a) {
            return h.matrix(lmspace::lambert(V(a[0].obj), V(a[1].obj), a[2].f,
                                             a[3].f, a[4].b));
          });
  checkfn(r, "hohmann", {"float", "float", "float"}, "Matrix", [&h](Args &a) {
    return h.matrix(lmspace::hohmann(a[0].f, a[1].f, a[2].f));
  });
  // Physical comparisons retain a finite-only variant beside the common testing API.
  checkfn(r, "space_assert_close", {"float", "float", "float"}, "void", [](Args &a) {
    if (!std::isfinite(a[0].f) || !std::isfinite(a[1].f) || !std::isfinite(a[2].f) || a[2].f < 0)
      throw dslvm::Error("space_assert_close: arguments and tolerance must be finite; tolerance must be nonnegative");
    dslvm::stdlib::testing_assert_close(a[0].f, a[1].f, a[2].f);
    return Value::none();
  });
}
const char *usage =
    "usage: lmspace --version | --notices | --dsl-schema "
    "[--json] | --dsl-signatures | [--dry-run] --dsl FILE [FILE ...]\n"
    "environment: LMSPACE_MAX_MEMORY_MB  total memory budget in MiB (default 4096); a program that "
    "needs more\n"
    "             stops with a run error (exit 3) instead of allocating\n";
} // namespace

int main(int argc, char **argv) {
#ifdef _WIN32
  _setmode(_fileno(stdout), _O_BINARY);
  _setmode(_fileno(stderr), _O_BINARY);
#endif
  Host h;
  dslvm::Registry reg;
  register_host(reg, h);
  if (argc == 2 && std::string(argv[1]) == "--version") {
    std::cout << "lmspace " LMSPACE_VERSION "\n";
    return 0;
  }
  if (argc == 2 && std::string(argv[1]) == "--notices") {
    std::cout.write(reinterpret_cast<const char *>(lmspace::embedded_notices),
                    (std::streamsize)lmspace::embedded_notices_size);
    return 0;
  }
  if (argc >= 2 && std::string(argv[1]) == "--dsl-schema") {
    if (argc == 2) {
      for (const auto &s : reg.signatures())
        std::cout << s << '\n';
      return 0;
    }
    if (argc == 3 && std::string(argv[2]) == "--json") {
      std::cout.write(reinterpret_cast<const char *>(lmspace::embedded_schema),
                      (std::streamsize)lmspace::embedded_schema_size);
      return 0;
    }
    std::cerr << usage;
    return 2;
  }
  if (argc == 2 && std::string(argv[1]) == "--dsl-signatures") {
    for (const auto &s : reg.signatures())
      std::cout << s << '\n';
    return 0;
  }
  bool dry = false;
  int dsl_arg = 1;
  if (argc > 1 && std::string(argv[dsl_arg]) == "--dry-run") {
    dry = true;
    ++dsl_arg;
  }
  if (argc <= dsl_arg || std::string(argv[dsl_arg]) != "--dsl") {
    std::cerr << usage;
    return 2;
  }
  std::vector<std::string> files;
  for (int i = dsl_arg + 1; i < argc; i++) {
    if (std::string(argv[i]) == "--dry-run") {
      if (dry) {
        std::cerr << usage;
        return 2;
      }
      dry = true;
    } else if (std::string(argv[i]).empty() ||
               std::string(argv[i]).rfind("--", 0) == 0) {
      std::cerr << usage;
      return 2;
    } else
      files.push_back(argv[i]);
  }
  if (files.empty()) {
    std::cerr << usage;
    return 2;
  }
  if (!membudget::configure("lmspace", "LMSPACE_MAX_MEMORY_MB")) {
    std::cerr << usage;
    return 2;
  }
  const std::string schema(
      reinterpret_cast<const char *>(lmspace::embedded_schema),
      lmspace::embedded_schema_size);
  dslvm::Context ctx;
  Space space;
  ctx.set("space", &space);
  std::vector<dslvm::Program> programs;
  for (const auto &file : files)
    try {
      programs.push_back(
          dslvm::Program::load(dslvm::read_file(file), schema, reg));
    } catch (const dslvm::Error &e) {
      return dslvm::report_error("load", e);
    } catch (const std::exception &e) {
      return dslvm::report_error("load", dslvm::Error(e.what()));
    }
  if (dry) {
    dslvm::report_ok();
    return 0;
  }
  for (const auto &p : programs) {
    try {
      p.run(ctx, reg);
    } catch (const dslvm::Error &e) {
      return dslvm::report_error("run", e);
    } catch (const std::exception &e) { // budget exceeded inside the VM
      return dslvm::report_error("run", dslvm::Error(e.what()));
    }
    dslvm::report_ok();
  }
  return 0;
}
