#pragma once

#include "VfsProjectStore.h"

#include <httplib.h>

#include <functional>

// The chunked-upload routes, shared by the service and its test so the test exercises the real handlers.
//   PUT    /project/entry/chunk?projectId=&path=&offset=&total=   body = the piece
//   DELETE /project/entry/chunk?projectId=&path=                  abandon a half-finished upload
// onEntryChanged fires once, when the last piece lands.
inline void registerVfsChunkUploadRoutes(httplib::Server& http, VfsProjectStore& store, juce::CriticalSection& storeLock,
                                         std::function<void(const juce::String& projectAndPath)> onEntryChanged)
{
    http.Put("/project/entry/chunk", [&store, &storeLock, onEntryChanged](const httplib::Request& req, httplib::Response& res)
    {
        if (! req.has_param("projectId") || ! req.has_param("path") || ! req.has_param("offset") || ! req.has_param("total"))
        {
            res.status = 400;
            return;
        }

        const auto projectId = juce::String(req.get_param_value("projectId"));
        const auto path = juce::String(req.get_param_value("path"));
        const auto offset = juce::String(req.get_param_value("offset")).getLargeIntValue();
        const auto total = juce::String(req.get_param_value("total")).getLargeIntValue();

        bool completed = false;
        juce::String errorMessage;
        {
            const juce::ScopedLock lock(storeLock);
            if (! store.writeEntryChunk(projectId, path, offset, total, req.body.data(), req.body.size(), completed, errorMessage))
            {
                res.status = 409;
                res.set_content(errorMessage.toStdString(), "text/plain");
                return;
            }
        }

        if (completed && onEntryChanged)
            onEntryChanged(projectId + ":" + path);

        res.set_content(completed ? "{\"status\":\"complete\"}" : "{\"status\":\"ok\"}", "application/json");
    });

    http.Delete("/project/entry/chunk", [&store, &storeLock](const httplib::Request& req, httplib::Response& res)
    {
        if (! req.has_param("projectId") || ! req.has_param("path"))
        {
            res.status = 400;
            return;
        }

        const juce::ScopedLock lock(storeLock);
        store.discardEntryUpload(juce::String(req.get_param_value("projectId")), juce::String(req.get_param_value("path")));
        res.set_content("{\"status\":\"ok\"}", "application/json");
    });
}
