# FRust built into the apps: libraries with swappable front ends

Status: plan for review. Nothing in section 6 onward is built. Written 2026-09-20 after the user restated the design.

## 1. Requirements

These are the user's words from 2026-09-20, in order. The original requirements document (written with another LLM) was never checked in and could not be found; if it turns up, it replaces or extends this list.

1. **The compiler is embedded in the app.** It runs inside the app's own process. The app never starts `frust_compiler`, `frate.exe` or any other tool to compile or build.
2. **The VFS is the compiler's only file system.** The compiler and Frate read and write "files" through the VFS, the way every other part of the suite does.
3. **The compiler writes its log output to the VFS.**
4. **Static linking.** The compiler and Frate are static libraries linked into the app. The app ships no library files of ours. (Windows system DLLs are fine.)
5. **FRust is libraries, and the front end is swappable.** The compiler and Frate are libraries. A front end supplies how they touch storage and where they report. The command line is one front end. An app is another. Neither is "the compiler".
6. There may be more requirements. This list is incomplete until the user says otherwise.

Consequences the user stated or implied:

- Nothing requires Windows file paths. Paths are virtual names; where they live is the front end's business.
- The command-line `frust_compiler` and `frate` are front ends for people in a terminal. They are not what the apps use.

## 2. The design in one picture

```text
                +--------------------------------------------------+
                |  FRust libraries (statically linked)             |
                |                                                  |
                |   frust_lang   compiler: parse, generate, JIT    |
                |   frate_core   pods: build, package, install     |
                |                                                  |
                |   both do ALL I/O through one interface:         |
                |        HostEnvironment                           |
                |          - FileSystem  (read, write, list, ...)  |
                |          - LogSink     (structured log lines)    |
                +----------+------------------+--------------------+
                           |                  |
        +------------------+---+   +----------+-----------------+   +------------------+
        | Command-line front   |   | Suite front end            |   | Test front end   |
        | end (FrustLang repo) |   | (suite repo, shared/)      |   | (in memory)      |
        |                      |   |                            |   |                  |
        | FileSystem = disk    |   | FileSystem = the VFS       |   | FileSystem = map |
        | LogSink   = console  |   | LogSink   = VFS log entries|   | LogSink = vector |
        +----------------------+   +----------------------------+   +------------------+
        frust_compiler, frate       Station, Developer, Engine, ...     smoke tests
```

The libraries know nothing about disks, consoles or the VFS. They only know `HostEnvironment`. That is what makes the front end swappable.

## 3. HostEnvironment

Defined in the FrustLang repository, in `frust_lang` (both libraries include it). Small on purpose.

```cpp
namespace frust {

// Virtual paths: '/'-separated, relative to the environment's own root.
// No drive letters, no "..": the environment decides what a path means.
class FileSystem {
public:
    virtual ~FileSystem() = default;
    virtual bool exists(const std::string& path) = 0;
    virtual bool read(const std::string& path, std::string& bytes) = 0;
    virtual bool write(const std::string& path, const std::string& bytes) = 0;  // creates parents
    virtual bool remove(const std::string& path) = 0;
    virtual bool list(const std::string& directory, std::vector<std::string>& names) = 0;
};

enum class LogLevel { Info, Warning, Error };
class LogSink {
public:
    virtual ~LogSink() = default;
    virtual void log(LogLevel level, const std::string& source, const std::string& message) = 0;
};

struct HostEnvironment {
    FileSystem& files;
    LogSink&    log;
};

}
```

Rules that keep it honest:

- No library function opens a file, creates a directory, writes to `std::cout`/`std::cerr`, or starts a process. Everything goes through `HostEnvironment`. A test enforces this (section 8).
- Network (the pod registry) is not file I/O. The registry client returns bytes; the library stores them through `FileSystem`.
- The interface is synchronous and thread-safe to call from one thread at a time per environment. Two environments can be used from two threads.

## 4. What each library does through it

**Compiler (`frust_lang`).**

- `compileFiles(env, sourcePaths, options)`: reads each source through `env.files`, resolves `use self::x;` by reading `x.fr` from the same virtual directory, resolves pods through Frate's pod layout in the same file system, writes the object file to the output path through `env.files`, and reports diagnostics through `env.log` (and returns them as data).
- `runFile(env, path)`: the JIT run mode. Same reads, no object.
- The REPL (`ReplSession`) is unchanged.
- The text-in, bytes-out entry point already built (`frust::Compile` in `CompilerApi.h`) stays as the lowest layer: it is the compiler with no I/O at all. `compileFiles` is that plus an environment. Nothing built so far is thrown away.

**Frate (`frate_core`).** Everything Frate does to files, through the environment:

| Frate operation | Through the environment |
|---|---|
| new | writes the scaffold |
| build | reads `frate.json` and sources, calls the compiler, writes `build/<name>.o` |
| package | reads sources, writes `<name>-<version>.frpod` |
| install / update | registry bytes in, pod files written into the pod cache path |
| add | rewrites `frate.json` |
| publish | reads the `.frpod`, sends it to the registry |
| cache location | a virtual path in the environment; no `frate_settings.json`, no prompt |

The interactive "choose a cache folder" prompt and the settings file are command-line concerns and move into the command-line front end.

**Linking an executable.** Building a library pod produces an object file and needs no linker. Building a `bin` pod into an `.exe` needs a linker, which today is `link.exe` from Visual Studio, started as a program. That is a toolchain, not the compiler. Decision needed (section 10): the apps never need it (they run programs with the JIT), so it stays in the command-line front end.

## 5. The front ends

**Command-line front end (FrustLang repository).** `DiskFileSystem` (virtual paths map to a directory on disk), `ConsoleLogSink`. `frust_compiler` and `frate` become thin `main()`s: parse arguments, build a disk environment, call the library. `frate build` calls the compiler library in-process instead of starting `frust_compiler_x.exe`. It stays a terminal tool.

**Suite front end (suite repository, `shared/FrustHost`).**

- `VfsFileSystem`: virtual paths map to entries in the open project's VFS, under a fixed root such as `Code/<pod or project>/`. Reads and writes are the VFS calls apps already use (`readEntry`, `writeEntry`).
- `VfsLogSink`: log lines are appended to log entries in the project's VFS, for example `Logs/frust/<date>.log`, so the log is inside `vfs.bin` and never on the OS disk. A viewer panel reads them.
- The pod cache is a virtual path in the Suite-level VFS, shared by every app and project.
- One class, `SuiteFrust`, exposes the operations an app needs: compile, check, build pod, run, load a plugin from source. Apps call this and nothing else.

**Test front end.** `MemoryFileSystem`, `VectorLogSink`. Used by every FRust test, so tests need no disk.

## 6. What exists today, honestly

| Piece | State | Under this plan |
|---|---|---|
| `frust::Compile` (text in, object bytes out, diagnostics as data) | Built, tested, in FrustLang `master` | Stays: the no-I/O core beneath `compileFiles` |
| Compiler diagnostics routed through a capturable stream | Built | Stays |
| Plugin host `load_source` and `loadFromSource` (sibling files, pods from providers) | Built on a branch, not merged | Stays, gets an environment-based overload |
| `frate::buildPod`, `packPod`, `unpackPod`, `PodFiles` | Built on a branch, tested, not merged | Becomes the pure core of `frate_core` |
| Station Script panel compiles in-process | Merged | Moves onto `SuiteFrust` |
| Station pod pipeline (VFS resolver, workspace, pod service) | Written, uncommitted, partly built | Rebuilt on `SuiteFrust` instead of its own glue |
| `HostEnvironment`, `compileFiles`, `frate_core` over an environment | **Not started** | This plan |
| Suite `VfsFileSystem`, `VfsLogSink`, `SuiteFrust` | **Not started** | This plan |
| Command lines rebuilt on the libraries | **Not started** | This plan |
| Creation Developer and Creation Engine | Not read yet; may build through the external tools | Audit in phase 5 |

## 7. Phases

Each phase ends with tests passing and a merge the user can review.

1. **Requirements.** The user confirms or corrects section 1. This file is committed so it cannot be lost.
2. **`HostEnvironment` and `MemoryEnvironment`** in `frust_lang`. `compileFiles` and `runFile` over an environment. Every existing compiler test moves to the memory environment. Acceptance: the whole compiler test set passes with no disk touched.
3. **`frate_core` over an environment.** Build, package, install, update, add, publish through `FileSystem`; the registry client returns bytes. Acceptance: the pod test set passes on the memory environment.
4. **Command-line front ends rebuilt** on the libraries (`DiskFileSystem`, `ConsoleLogSink`). `frate build` calls the compiler library, not the compiler program. Acceptance: FrustLang's own regression sweep (plugin host examples, JUCE host rebuild) passes unchanged in behavior.
5. **Suite front end** (`VfsFileSystem`, `VfsLogSink`, `SuiteFrust`) in `shared/`. Station moves onto it, including the Script panel and the pod pipeline. Then Developer and Engine. Acceptance: no app starts a compiler or Frate; the log appears in the VFS.
6. **Guards** (section 8) turned on for every PR.
7. **The agent's coder** (`Suite-Agent-Runtime-Spec.md`, section 6A) uses `SuiteFrust`.

## 8. Guards, so this cannot quietly come back

Automatic checks that run on every PR and fail the build:

- **No external tools.** No app or shared source references `frust_compiler`, `frate.exe`, `link.exe` or starts a process to compile.
- **No I/O in the libraries.** A test builds `frust_lang` and `frate_core` with a hooked file layer and fails if the library opens a file or writes to the console outside `HostEnvironment`.
- **VFS-only storage.** After exercising compile, build-pod, run and plugin load in the suite front end, the storage root holds only `vfs.bin` and the heartbeat file, and nothing is written to Roaming or the temp folder.
- **No shipped libraries.** The built app's imports contain only Windows system DLLs and the Visual C++ runtime; the release package contains no library file of ours.

## 9. Risks

- The FrustLang repository is shared by every app and by the command line; the environment refactor touches core paths. Mitigation: phased, the regression sweep in phase 4, and the old file-based entry points kept until every caller is moved.
- Memory use: reading whole files into strings is fine for source; object files and packages are small.
- The registry needs the network. Offline, only what is already in the VFS pod cache resolves.

## 10. Open questions for the user

1. **Executables.** Confirm: the apps never build a `.exe`; they run programs with the JIT; producing an `.exe` (which needs a linker program) stays in the command-line front end.
2. **Where the code lives in the VFS.** Proposed: `Code/` in each project for sources and build output, and the Suite-level `frate-cache/` for pods, as today. Logs in `Logs/frust/`. Is that right?
3. **Log content and size.** Diagnostics and build steps, one line each, rotated by date and pruned after a set number of days. Anything else the log must hold?
4. **Repository split.** `HostEnvironment` and the libraries live in FrustLang (a repo other things use); the VFS front end lives in the suite. Agreed?
5. **Anything missing** from section 1.
