# Ninja hello example

This project demonstrates the Clang Ninja adapter with one module interface and
one importing translation unit. From the repository root, first build
`cxx-modgraph`, then run:

```sh
cd examples/ninja-hello-simple
mkdir -p build
../../build/cxx-modgraph --input dependencies.json --emit ninja \
  --output build/modules.ninja
ninja
./hello
```

The committed `build.ninja` supplies the compiler rules and link edge. The
generated `build/modules.ninja` contains only the deterministic module graph and
can be regenerated whenever `dependencies.json` changes.
