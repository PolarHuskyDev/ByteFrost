# ByteFrost E2E Layout

E2E cases are organized by target and feature category:

```
tests/
  e2e/
    compiler/
      <category>/
        <test_case>/
    orca/
      <category>/
        <test_case>/
```

The runner recursively discovers every `metadata.json` under `tests/e2e/**`,
so categories can be added freely without changing the harness.

## Compiler Categories

- `core_language`: entrypoints, function calls, recursion, literals
- `flow_control`: if/elseif/else, match, loops, break/continue
- `operators`: arithmetic, comparison, logical, precedence, bitwise
- `strings`: chars, interpolation, concatenation, escaping/unicode (planned)
- `const_and_readonly`: const init/reassign, readonly member semantics
- `structs_and_composition`: struct init, composition, constructor behavior
- `enums`: enum values, comparisons, match behavior, exhaustiveness (planned)
- `containers`: arrays/maps behavior, overwrite semantics, iteration
- `diagnostics`: undefined names, duplicates, type errors, invalid operations
- `scope_and_bindings`: block scope, shadowing, redeclaration checks
- `nullable_flow`: nullable assignment and flow-sensitive narrowing
- `module_system`: import/override behavior and module-level semantics

## Orca Categories

- `lifecycle`: build/run/check/clean command workflows
- `module_resolution`: aliasing, missing modules, override interactions
- `smoke_apps`: realistic multi-file projects (for runtime and integration confidence)
- `incremental_build` (planned): cache hits, invalidation, changed-file rebuild
- `dependency_graph` (planned): transitive rebuild correctness and cycles
- `cache_recovery` (planned): partial cache corruption and auto-recovery

## Priority Expansion Backlog

1. Compiler diagnostics explosion (`compiler/diagnostics/*`) with focused negative cases.
2. Compiler nullable semantics (`compiler/nullable_flow/*`) for non-null checks and narrowing.
3. Struct semantic hardening (`compiler/structs_and_composition/*`) for unknown/duplicate/wrong-type fields.
4. Enum exhaustiveness and invalid-arm diagnostics (`compiler/enums/*`).
5. Orca incremental and dependency graph tests (`orca/incremental_build/*`, `orca/dependency_graph/*`).
6. Add larger smoke apps (`orca/smoke_apps/*`) such as json/csv/math packages.
