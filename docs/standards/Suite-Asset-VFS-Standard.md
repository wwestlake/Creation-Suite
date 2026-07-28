# Suite Asset And VFS Standard

## Standard

- Every Creation app must be able to locate one suite-level configuration
  and discovery location.
- That suite-level location stores shared configuration, shared resources,
  and project-root discovery metadata.
- Asset identity is shared across the suite through stable IDs, version
  IDs, and reference modes.
- Project archives are treated as virtual filesystem containers.
- Reads should happen through the shared asset/VFS layer rather than
  direct file assumptions inside tools.
- A temporary real file may be materialized only for integrations that
  explicitly require filesystem paths.

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
