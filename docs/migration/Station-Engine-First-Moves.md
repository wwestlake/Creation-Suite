# Station And Engine First Moves

This checklist defines the first practical shared-code migration between
Creation Station and Creation Engine.

## Step 1: Move Shared Asset Types

Source currently seeded in:

- `D:\000 Creation Station\AssetSystem\include\creation\assets\AssetTypes.h`

Target:

- `Creation Suite/shared/AssetSystem/include/...`

## Step 2: Move Shared VFS Implementation

Sources currently seeded in:

- `D:\000 Creation Station\AssetSystem\include\creation\assets\VirtualFileSystem.h`
- `D:\000 Creation Station\AssetSystem\src\VirtualFileSystem.cpp`

Creation Engine currently has its own analogous implementation in:

- `D:\000 Creation Engine\AssetSystem\include\assets\VirtualFileSystem.h`
- `D:\000 Creation Engine\AssetSystem\src\VirtualFileSystem.cpp`

Target:

- one shared implementation in `Creation Suite/shared/AssetSystem/`

## Step 3: Add Suite Bootstrap Settings

Current seed in Creation Station:

- `D:\000 Creation Station\Source\Suite\SuiteSettings.h`
- `D:\000 Creation Station\Source\Suite\SuiteSettings.cpp`

Target:

- shared suite bootstrap/discovery module in `shared/AssetSystem/` or
  `shared/Interop/`

## Step 4: Rewire Creation Station

Creation Station should:

- stop owning the shared asset types
- stop owning the shared VFS implementation
- consume the shared suite settings/bootstrap layer
- continue migrating Foley from filename-based references to shared
  asset refs

## Step 5: Rewire Creation Engine

Creation Engine should:

- replace its local VFS implementation with the shared one
- adopt the shared asset identity model
- keep app-specific model/texture/script loaders in-engine

## Step 6: Verify No App-Specific Leakage

The shared module must remain free of:

- DAW-specific UI
- engine-specific render logic
- app-specific domain intrinsics

If any of that creeps in, it belongs back in the app repo.
