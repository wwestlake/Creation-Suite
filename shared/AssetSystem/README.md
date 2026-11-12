# Shared Asset System

This module is the suite-wide home for:

- shared asset identity types
- suite discovery/bootstrap metadata
- service-backed project sessions and asset catalog resolution
- project entry streaming through the sole-owner Suite VFS service

ProjectSession addresses projects by ID, never by an app-owned directory or
container. `readOptionalEntry` distinguishes an absent app-specific document from
an I/O failure. `refreshManifest` preserves local metadata/catalog edits while
reading peer changes. `commit` merges against the current manifest and uses an
atomic conditional save in the existing VFS API; conflicting edits fail without
overwriting peer metadata. Rebuilt clients require a service advertising this
capability, and fail clearly against an older service.
The service requires a revision on every manifest replacement, so an older
unguarded writer cannot bypass protection. Use ProjectSession for editing, or
`compareAndWriteManifest` with the revision read from the service for a deliberate
whole-manifest replacement.

See [the shared project model](../../docs/architecture/Suite-Shared-Project-Model.md)
and [Station/Movie Order 1](../../docs/architecture/Station-Movie-Shared-Project-Order-1.md).
