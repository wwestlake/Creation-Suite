# Shared Services

This module owns suite-level services that are not tied to one app's
domain.

Shared here:

- BYOK provider/account definitions
- suite-wide AI routing settings
- shared orchestration policy for capability matching, fallback, health, and dry-run route planning
- centralized suite activity logging for shared services and app integrations
- shared account/session contracts
- service discovery contracts used by multiple apps
- the EULA's text and version (`SuiteEula`)

Left per app:

- domain prompts and task context
- app-specific helper behavior
- app-local workflows that call the shared services

## Storage

Everything this module keeps is a suite entry in the VFS, through
`SuiteVfsJsonStore`, never a file on the OS: `ai-settings.json` (AI accounts,
their keys, per-app routing), `suite-ai-health.json`,
`suite-ai-diagnostics.json` and `suite-activity-log.json`. A key is entered
once, in the suite's settings, and every app reads it from there.

Tests call `SuiteVfsJsonStore::setScopeForTesting` so their entries go to
`tests/<scope>/` and never touch the real ones (see `tests/ServicesSmoke.cpp`).
See `docs/architecture/Suite-Shared-Project-Model.md`.
