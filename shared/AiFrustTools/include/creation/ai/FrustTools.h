#pragma once

#include <creation/ai/Tool.h>
#include <creation/frust/ScriptRunner.h>

#include <functional>
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

// `frust_lookup`: search the application's help (the FRust language, its API, how the tools work) for a question, and
// return the matching excerpts. The application supplies the search; the assistant calls this when it is unsure how to
// write something, instead of guessing.
std::shared_ptr<Tool> makeFrustLookupTool(std::function<std::string(const std::string& query)> search);

// `frate_registry`: what pods (FRust packages) the Frate registry has. The registry is a web service (by default
// https://lagdaemon.com/djehuti/api/frate); this asks its public, read-only list call and reports each pod's name,
// latest version, license, description and exports, so the agent knows what exists instead of guessing. The application supplies
// `httpGet` (the URL in, the response body out, empty on any failure), which keeps this free of networking code and testable.
// What the registry returns is written by the pods' authors, so it is cleaned and reported only as data.
std::shared_ptr<Tool> makeFrateRegistryTool(std::function<std::string(const std::string& url)> httpGet,
                                            std::string baseUrl = "https://lagdaemon.com/djehuti/api/frate");

// The text of a script result as the model should read it.
std::string describeScriptResult(const creation::frust::ScriptResult& result);
}
