#pragma once

#include <cstdint>
#include <string>

namespace ce::lang::jit {

// The opaque object every compiled CEL function and intrinsic call
// implicitly receives as its first parameter (see docs/SCRIPTING_ABI.md).
// This is the generic, host-agnostic base: the fields the CORE-domain
// trampolines (intrinsic_trampolines.cpp -- atan2, log/log_int/log_float/
// log_vec3, the watchdog) touch, and nothing else. A host with real
// per-domain state (an entity/world model, an audio engine handle, ...)
// derives its own concrete context type from this one -- e.g.
// `struct ScriptContext : ce::lang::jit::ScriptContext { World* world; };`
// -- so a pointer to the derived type is always safely usable wherever a
// `ce::lang::jit::ScriptContext*` is expected (a normal C++ upcast), and
// the base's fields sit at the same offsets either way. This is not
// optional: the Core trampolines are compiled once, here, against this
// exact layout, and every host's own scripts call them (every loop
// back-edge hits the watchdog), so the derived type's base subobject
// really does get read/written by code that was compiled against only
// this base -- inheritance, not just similarly-shaped structs, is what
// makes that well-defined.
struct ScriptContext {
    // The runaway-script watchdog: decremented once per loop iteration
    // executed by any compiled CEL code using this context (a check
    // inserted at every while/for loop back-edge by IR-gen). When it
    // reaches zero the running script faults with CEL9001 instead of
    // hanging the caller. Not a security sandbox -- just a guard against
    // an accidental infinite loop; see docs/SCRIPTING_ABI.md.
    int64_t loopBudget = 10'000'000;
    bool faulted = false;
    std::string faultMessage;
};

} // namespace ce::lang::jit
