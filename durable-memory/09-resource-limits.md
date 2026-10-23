# Resource limits

Why: host objects live in arenas until exit (no `free`), so a DSL loop that keeps creating
propagation tables grows without bound. On 2026-10-23 a runaway lm* host (lmnum) reached 16 GB RSS
and filled the 24 GB WSL machine; every lm* host now has a total budget.

- **Total memory budget**, default 4096 MiB, set with `LMSPACE_MAX_MEMORY_MB` (positive whole MiB;
  anything else exits 2 with the usage text). Shown in the usage message.
- `cpp/memory_budget.{hpp,cpp}` (identical copy in every lm* host) replaces the global
  `operator new`/`delete` and counts block sizes from the C allocator. Everything lmspace keeps is
  C++ heap: the Matrix/Vector/State/Elements/System arenas, `Space` bodies, body names, and the
  propagation table (`std::vector<double>`, rows x (1 + 6 bodies), at most 1e8 cells = 763 MiB per
  table). `System.propagate`/`Space.propagate` also `require()` the table size before integrating,
  so an oversized table fails before any work.
- Exceeding it throws `membudget::Exceeded` (a `std::bad_alloc`); `translate` turns it into the
  usual run error, the first failure prints `lmspace: memory budget exceeded: ... (set
  LMSPACE_MAX_MEMORY_MB ...)` on stderr, exit 3. `main` also catches `std::exception` around load/run.
- CSPICE: no callbacks into C++, so no exception can unwind through its C frames (all throws happen
  in `spice.cpp` after the C call returned). Its kernel pool, kernel table and DAF/DAS handle
  tables are fixed static arrays: a huge text kernel fails with "no room available ... kernel pool"
  (measured: 200 loads of a 600000-value text kernel stop at once with 15 MB RSS), and repeated
  `load_kernel` is bounded by the kernel table. The f2c I/O `malloc`s (unit buffers, file names)
  are bounded by the unit count. None of this is charged (static, a few MB).
- Not covered: transient per-call vectors inside one RK45 step (a few x 6 x bodies doubles).
- No `setrlimit`: not portable to macOS static builds and would crash instead of reporting.
- Measured (x86_64 Linux, WSL): a loop of 16-body `propagate(0.0, 1000000, ...)` (741 MiB per
  table) stops at 64 MiB at once (pre-check) and at the default 4096 MiB after 8.5 s with 3.79 GB
  peak RSS (3700 MiB tracked). `test.sh` runs both; the default-budget run only on Linux, under
  `ulimit -v 6291456`.
