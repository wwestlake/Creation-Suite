#pragma once

#include <llvm/ExecutionEngine/Orc/Core.h>

#include "intrinsic_trampolines.h"
#include "lang/type.h"

namespace llvm::orc {
class LLJIT;
}

namespace ce::lang::jit {

// Registers every real host-ABI trampoline (the Core set from
// intrinsic_trampolines.cpp, plus `extraSymbols`/`extraDomains`) plus
// the watchdog tick function into `lljit`'s main JITDylib via
// absoluteSymbols -- explicit registration, deliberately NOT
// DynamicLibrarySearchGenerator::GetForCurrentProcess() (see runtime.cpp's
// GS1 selftest comment for why that fails on Windows for non-dllexport'd
// .exe symbols).
//
// `extraSymbols`/`extraDomains` are how a host with its own domain (e.g.
// Creation Engine's World access) adds its own real trampolines on top
// of the Core set without this shared library needing to know anything
// about them -- combined with GetAbiTrampolines()/GetAbiSymbolDomains()
// before the same filter+register loop runs. This is the "apps add
// function calls to the core" extensibility point on the imperative
// side (the declarative side -- sema validation, domain tagging for
// symbols already known via intrinsics.def -- is handled by a host
// overriding intrinsics.def itself via
// `target_include_directories(... BEFORE ...)`, see intrinsics.def's own
// comment).
//
// `allowed` (GS-Interop, default All()) filters which symbols actually
// get registered against the combined domain map -- a symbol outside it
// is simply never given an address in this JITDylib, so a call to it
// fails as a genuine JIT symbol-lookup error, not just a convention
// violation. This is the defense-in-depth half of capability gating;
// sema.cpp's CheckCall is the compile-time half (and is expected to
// already have rejected the same call, so this filter is normally
// never actually exercised on the success path -- see the GS-Interop
// plan for why both layers still matter). ce_watchdog_tick (absent
// from both domain maps) is always registered regardless of `allowed`
// -- it's a runtime safety mechanism, not a script capability.
llvm::Error RegisterAbiTrampolines(llvm::orc::LLJIT& lljit, const IntrinsicDomainSet& allowed = IntrinsicDomainSet::All(),
                                   const std::vector<AbiSymbol>& extraSymbols = {},
                                   const std::unordered_map<std::string, IntrinsicDomain>& extraDomains = {});

} // namespace ce::lang::jit
