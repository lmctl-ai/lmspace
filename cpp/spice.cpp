#include "spice.hpp"

#include "orbit.hpp"
#include "SpiceUsr.h"

#include <array>
#include <cmath>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace lmspace {
namespace {
std::mutex spice_mutex;

[[noreturn]] void throw_spice(const char* operation) {
  SpiceChar message[1841] = {};
  getmsg_c("LONG", static_cast<SpiceInt>(sizeof(message)), message);
  reset_c();
  throw std::runtime_error(std::string("CSPICE ") + operation + ": " + message);
}

void check_spice(const char* operation) {
  if (failed_c()) throw_spice(operation);
}

Matrix row_matrix(const SpiceDouble* values, int count) {
  return Matrix{1, count, std::vector<double>(values, values + count)};
}

Matrix square_matrix(const SpiceDouble values[3][3]) {
  std::vector<double> flat;
  flat.reserve(9);
  for (int row = 0; row < 3; ++row)
    for (int col = 0; col < 3; ++col) flat.push_back(values[row][col]);
  return Matrix{3, 3, std::move(flat)};
}

void configure_errors() {
  // CSPICE defaults to aborting the process on an error. lmspace is a host,
  // so every error must return through the C++ exception boundary instead.
  SpiceChar action[] = "RETURN";
  SpiceChar report[] = "NONE";
  erract_c("SET", 6, action);
  errprt_c("SET", 5, report);
}
}  // namespace

void spice_load(const std::string& path) {
  std::lock_guard<std::mutex> lock(spice_mutex);
  configure_errors();
  furnsh_c(path.c_str());
  check_spice("furnsh");
}

void spice_clear() {
  std::lock_guard<std::mutex> lock(spice_mutex);
  configure_errors();
  kclear_c();
  check_spice("kclear");
}

Matrix spice_state(const std::string& target, const std::string& observer,
                   const std::string& frame, double et) {
  std::lock_guard<std::mutex> lock(spice_mutex);
  configure_errors();
  if (!std::isfinite(et)) throw std::invalid_argument("CSPICE spkezr: ET must be finite");
  SpiceDouble state[6] = {};
  SpiceDouble light_time = 0.0;
  spkezr_c(target.c_str(), et, frame.c_str(), "NONE", observer.c_str(), state,
           &light_time);
  check_spice("spkezr");
  return row_matrix(state, 6);
}

Matrix spice_frame(const std::string& from, const std::string& to, double et) {
  std::lock_guard<std::mutex> lock(spice_mutex);
  configure_errors();
  if (!std::isfinite(et)) throw std::invalid_argument("CSPICE pxform: ET must be finite");
  SpiceDouble rotation[3][3] = {};
  pxform_c(from.c_str(), to.c_str(), et, rotation);
  check_spice("pxform");
  return square_matrix(rotation);
}

double spice_time(const std::string& utc) {
  std::lock_guard<std::mutex> lock(spice_mutex);
  configure_errors();
  SpiceDouble et = 0.0;
  str2et_c(utc.c_str(), &et);
  check_spice("str2et");
  return et;
}

}  // namespace lmspace
