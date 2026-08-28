# FRust Plugin Runtime

`PluginRuntime` is the Suite wrapper around FRust's independently loadable
plugin handles. A runtime owns a keyed set of loaded plugins; loading,
reloading, or unloading one key does not affect any other key.

Use the keyed API for application features:

- `load(key, path, error)` loads a new plugin and refuses to replace an
  existing key.
- `reload(key, error)` replaces only that plugin's compiled code.
- `unload(key)` releases only that plugin.
- `getFunction(key, name)` and `callEvent(key, id, argument)` target one
  plugin; `callEventAll` dispatches to every loaded key in stable order.

The original one-plugin calls remain as compatibility helpers for a named
`default` slot. They affect only that slot and must not be used for new
multi-plugin features.

Host functions are registered before plugins load and remain a deliberate
capability boundary. A plugin receives only the functions its host exposes.
