# mIDE compiler plans

This compiler intentionally targets a C-like subset that maps cleanly to
Mindustry Logic. Keywords are implemented only when their semantics can be
represented without pretending that mlog has C's memory and execution model.
The deferred items below are not considered impossible: Mindustry Logic
community members have implemented and published comparable techniques, and
these features are planned for future compiler support.

## Implemented

- Basic C type spellings: `int`, `char`, `short`, `long`, `signed`, `unsigned`,
  `float`, `double`, `bool`, `_Bool`, and `void` are accepted by the current
  scalar compiler.
- Non-semantic declaration qualifiers: `static`, `extern`, `auto`, `register`,
  `inline`, and `restrict` are accepted where a declaration specifier is
  accepted. Their storage, linkage, and optimization semantics are not
  represented by mlog.
- `const` declarations are accepted and reassignment is rejected.
- `true` and `false` compile as `1` and `0`.
- `if`/`else`, `while`, and `for` control flow are lowered to mlog jumps.
- `break` and `continue` are supported inside loops.

## Deferred

- `volatile`: requires a defined memory/device model and observable-access
  rules; mlog variables do not provide C volatile semantics.
- `sizeof`: needs a stable size model for scalar values, arrays, pointers, and
  target-specific device data.
- `typedef`, enums, structs, unions, and user-defined type names: require a
  type table rather than the current scalar declaration model.
- Pointers, arrays, address-of, dereference, and `NULL`: require addressable
  storage and pointer representation.
- `switch`, `case`, `default`, `do`, and `goto`: require additional structured
  control-flow lowering and branch analysis.
- `static` local lifetime and `extern` linkage: acceptance is currently
  syntactic only and does not provide those C linkage guarantees.
- `volatile`, atomics, threads, and signal-related keywords: mlog has no
  equivalent memory-ordering or asynchronous execution model.
