# Linux Systems Programming Toolkit

A C++17 learning project exploring operating-system and parallel-programming fundamentals. It can be built on macOS for development; the process demo uses POSIX APIs available on macOS and Linux.

## Current features

- **Process lifecycle:** creates a child with `fork()` and waits for it with `waitpid()`.
- **Producer-consumer queue:** demonstrates a bounded queue protected by a mutex and condition variables.
- **Parallel benchmark suite:** compares sequential execution against 1, 2, 4, and 8 worker threads.
- **Repeated measurements:** reports every run, the median runtime, speedup relative to the sequential baseline, and correctness.
- **Portable build:** CMake + C++17 + standard threading library.

## Build and run

Requirements: C++17 compiler and CMake 3.16+.

```bash
cmake -S . -B build
cmake --build build
./build/systems_toolkit
```

Direct compilation on macOS is also possible:

```bash
clang++ -std=c++17 -Wall -Wextra -pthread src/main.cpp -o systems_toolkit
./systems_toolkit
```

## Benchmark methodology

The benchmark runs a deterministic CPU workload sequentially and with several worker counts. Each configuration is measured multiple times, and the median is reported to reduce the effect of individual noisy runs. All versions must produce the same result.

Results depend on the processor, compiler, power state, background activity, and runtime environment. A thread count larger than the number of available CPU cores may not improve performance. Do not compare results across machines without documenting the hardware and conditions.

## Suggested next experiments

- Run the executable several times and compare medians across runs.
- Try changing `kWorkItems` and `kRepetitions` in `src/main.cpp`.
- Explain why speedup is not necessarily linear with thread count.
- Later, run in a Linux environment and explore `time`, `top`, and `perf stat` where available.

## Scope

This is a learning project, not a kernel module or GPU driver. It does not currently use CUDA, OpenGL, DirectX, or Linux kernel APIs beyond portable POSIX process calls.
