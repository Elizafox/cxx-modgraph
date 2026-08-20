# C++ module dependency conformance corpus

Each directory/file is a standalone input with its expected outcome encoded in
`manifest.json`. The corpus deliberately includes valid graphs as well as inputs
that must be diagnosed. Paths and compiler identities are data, not assumptions
about the host running the corpus.

The cases cover chains, diamonds, partitions, implementation units, duplicate
providers, unresolved imports, direct and indirect cycles, generated interfaces,
changed imports, multiple roots, unusual legal names, differing Clang/GCC scans,
native/cross target metadata, external modules, and malformed/extended P1689.
Consumers may use the embedded `facts` or `p1689` documents without running this
project's test harness.
