# Shared Services

This module owns suite-level services that are not tied to one app's
domain.

Shared here:

- BYOK provider/account definitions
- suite-wide AI routing settings
- shared orchestration policy for capability matching, fallback, health, and dry-run route planning
- shared account/session contracts
- service discovery contracts used by multiple apps

Left per app:

- domain prompts and task context
- app-specific helper behavior
- app-local workflows that call the shared services
