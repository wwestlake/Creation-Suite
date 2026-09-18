# Shared Libraries

This folder contains code shared across the Creation Suite.

Rules:

- only move code here when it is truly cross-app
- keep app-specific workflow and UI logic inside the app repos
- shared code must avoid unnecessary dependencies on any one app
- shared FRust code provides language/runtime infrastructure, while apps
  provide their own domain intrinsics and capability surfaces
