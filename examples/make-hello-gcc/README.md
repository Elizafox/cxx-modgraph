# GCC and Make module graph

This example uses GCC 16, GNU Make, and the auto-detecting adapter entry point
to select the GCC adapter and build libstdc++'s `std` and `std.compat` modules
automatically, then scan and build a primary module interface with two
partitions in nested directories.

From the repository root:

```sh
cmake -S . -B build
cmake --build build
make -C examples/make-hello-gcc
./examples/make-hello-gcc/hello
```

The expected output is:

```text
Hello from the GCC module graph!
```

GCC emits one P1689R5 document per source. The adapter combines those inputs
through `cxx-modgraph`, generates `.gcm` paths, and writes a GCC module-mapper
file for each BMI and object compilation. Setting
`CXX_MODGRAPH_USE_LIBSTDCXX_STD := 1` invokes GCC's `--compile-std-module` mode and
adds the resulting standard-module objects to the link.

The adapter targets GCC 16 or newer. Other toolchain modules can be registered as
prebuilt mappings through `CXX_MODGRAPH_EXTERNAL_MODULES`; their build recipes
remain the project's responsibility.
