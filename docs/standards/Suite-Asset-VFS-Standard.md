# Suite Asset And VFS Standard

## Standard

- Every Creation app must be able to locate one suite-level configuration
  and discovery location.
- That suite-level configuration is authoritative. Existing app code,
  legacy folder conventions, and stale local behavior are not.
- That suite-level location stores shared configuration, shared resources,
  and project-root discovery metadata.
- Every app must use centralized suite storage and shared asset management.
  No app may invent its own working storage model.
- Asset identity is shared across the suite through stable IDs, version
  IDs, and reference modes.
- Project archives are treated as virtual filesystem containers.
- Reads should happen through the shared asset/VFS layer rather than
  direct file assumptions inside tools.
- A temporary real file may be materialized only for integrations that
  explicitly require filesystem paths.

## Non-Negotiable Storage Rules

- There is no per-app storage authority. The suite configuration system
  defines the storage roots once, and apps consume them.
- There is no recurring "is storage configured?" workflow inside apps.
  After suite settings exist, apps must start ready to use.
- Default suite roots count as configured. Explicit user-selected roots
  also count as configured. Both are valid.
- If a configured directory does not exist yet, the app should create it
  silently. Missing directories are a runtime initialization detail, not
  a configuration failure.
- OS file or folder pickers are valid only for explicit external import
  and export actions, or when the user intentionally opens suite storage
  settings to change locations.
- OS pickers are not valid for normal startup, project open, project
  save, recording, drag-drop ingest, or routine app use.
- Working project data lives in suite-managed project containers and VFS
  storage, not ad hoc app folders.
- External files may enter the system only through explicit import or
  drag-drop ingest, after which they become managed project assets.

## Immediate Goal

Unify Creation Station and Creation Engine on one asset and VFS core
before broader migrations begin.

## First Shared Module

The first shared module should be `shared/AssetSystem/`, containing:

- asset identity/value types
- suite settings/bootstrap types
- zip-backed VFS implementation
- materialization bridge interfaces
- project registry/discovery types
