#pragma once

#include <creation/ai/Tool.h>
#include <creation/frust/ScriptRunner.h>

#include <memory>
#include <string>

// The tools that let an AI work as a coder in FRust: write a script, have it checked, run it against the
// application's API, and read the diagnostics or the output. They sit between the agent engine (which knows
// nothing about FRust) and the script runner (which knows nothing about AI).
namespace creation::ai
{
// `run_frust`: compile and run a script. It must define `pub fn run() -> String`. Compile errors come back as
// file:line:column messages the model can act on; a run reports what the script logged and returned.
std::shared_ptr<Tool> makeRunFrustTool(creation::frust::ScriptRunner& runner);

// `check_frust`: compile only. Cheap, changes nothing.
std::shared_ptr<Tool> makeCheckFrustTool(creation::frust::ScriptRunner& runner);

// `frust_api_reference`: the application's API declarations and the premade modules a script can `use`.
std::shared_ptr<Tool> makeFrustApiReferenceTool(const creation::frust::ScriptApi& api);

// The text of a script result as the model should read it.
std::string describeScriptResult(const creation::frust::ScriptResult& result);
}
