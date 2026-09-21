#pragma once

#include <string>
#include <vector>

// Where an assistant keeps its things. The assistant never touches a disk: everything it stores goes through this,
// and an application backs it with the project's storage (its VFS). Paths are virtual, like "Assistants/Station/x".
namespace creation::assistant
{
class AssistantFiles
{
public:
    virtual ~AssistantFiles() = default;

    virtual bool exists(const std::string& path) = 0;
    virtual bool read(const std::string& path, std::string& text) = 0;
    // Writes replace the whole entry in one step.
    virtual bool write(const std::string& path, const std::string& text) = 0;
    virtual bool remove(const std::string& path) = 0;
    // Every entry whose path starts with `prefix`.
    virtual std::vector<std::string> list(const std::string& prefix) = 0;
};

// Where one application's assistant keeps what it stores, inside a project:
//
//   Assistants/<App>/conversations/<id>.json    the conversation ledger, one file per conversation
//   Assistants/<App>/archive/<id>.json          conversations put away
//   Assistants/<App>/plans/<id>.json            plans (see PlanStore)
//   Assistants/<App>/workspace/...              the scripts and tools the assistant has written for this project
//
// The folder is under the application because each application has its own assistant, and it lives in the
// project so it travels with the project.
struct AssistantArea
{
    std::string app;   // "Station"

    std::string root() const { return "Assistants/" + app + "/"; }
    std::string conversations() const { return root() + "conversations/"; }
    std::string archive() const { return root() + "archive/"; }
    std::string plans() const { return root() + "plans/"; }
    std::string workspace() const { return root() + "workspace/"; }
};
}
