# Shared CEL

This module will host the shared CEL language infrastructure used across
the Creation Suite.

Shared here:

- frontend/parser/sema
- runtime and JIT core
- node bridge/code generation core
- common host ABI rules
- shared build and LLVM integration

Left per app:

- app-specific domain intrinsics
- domain policy
- any capability gating that prevents code from running in the wrong app

## Grammar Generation

The CEL frontend normally regenerates its parser and lexer from
`grammar/cel.y` and `grammar/cel.l` when WinFlexBison is available
through `CE_WINFLEXBISON_DIR` or `PATH`.

For fresh agent workspaces that do not yet have WinFlexBison installed,
the repo also carries a checked-in fallback snapshot under
`shared/CEL/generated/`. CMake will use those generated files to keep
suite apps buildable, but grammar changes should still be regenerated
with the real tools and the fallback snapshot refreshed in the same
change.
