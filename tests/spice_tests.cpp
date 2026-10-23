#include "SpiceUsr.h"
#include "orbit.hpp"
#include "spice.hpp"
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
int checks = 0;
void close(double a, double b, double tolerance) {
  ++checks;
  if (!std::isfinite(a) || std::abs(a - b) > tolerance)
    throw std::runtime_error("SPICE numeric mismatch: " + std::to_string(a) +
                             " vs " + std::to_string(b));
}
template <class F> void fails(F f) {
  ++checks;
  try {
    f();
  } catch (const std::exception &) {
    return;
  }
  throw std::runtime_error("SPICE call should have failed");
}
} // namespace
int main(int argc, char **argv) {
  if (argc < 2 || argc > 3)
    return 2;
  const bool keep = argc == 3 && std::string(argv[2]) == "--keep";
  const std::string path = argv[1];
  try {
    auto rotation = lmspace::spice_frame("J2000", "ECLIPJ2000", 0.0);
    close(rotation.get(0, 0), 1, 1e-14);
    close(rotation.get(1, 1), 0.9174820620691818, 1e-13);
    close(rotation.get(1, 2), 0.3977771559319137, 1e-13);
    fails([] { lmspace::spice_load("lmspace-missing-kernel.bsp"); });
    // The library must recover after a SPICE error rather than retain failed
    // state.
    close(lmspace::spice_frame("J2000", "J2000", 0).get(2, 2), 1, 1e-15);
    fails([] { lmspace::spice_state("MOON", "EARTH", "J2000", 0); });
    // Generate an original tiny degree-one SPK for linear motion: no network,
    // external kernels, real mission data or NAIF kernel redistribution
    // required.
    std::filesystem::remove(path);
    SpiceInt handle;
    spkopn_c(path.c_str(), "lmspace synthetic linear test", 0, &handle);
    if (failed_c())
      throw std::runtime_error("spkopn failed");
    SpiceDouble states[2][6] = {{100, 200, 300, 1, 2, 3},
                                {110, 220, 330, 1, 2, 3}};
    spkw08_c(handle, 1000001, 0, "J2000", 0, 10,
             "original synthetic linear state", 1, 2, states, 0, 10);
    spkcls_c(handle);
    if (failed_c())
      throw std::runtime_error("synthetic SPK write failed");
    lmspace::spice_load(path);
    auto state = lmspace::spice_state("1000001", "0", "J2000", 5);
    for (int i = 0; i < 3; ++i) {
      close(state.get(0, i), 105.0 * (i + 1), 1e-12);
      close(state.get(0, i + 3), i + 1, 1e-12);
    }
    lmspace::spice_clear();
    fails([] { lmspace::spice_state("1000001", "0", "J2000", 5); });
    if (!keep)
      std::filesystem::remove(path);
    std::cout << checks << " SPICE checks passed\n";
  } catch (const std::exception &e) {
    std::filesystem::remove(path);
    std::cerr << e.what() << '\n';
    return 1;
  }
}
