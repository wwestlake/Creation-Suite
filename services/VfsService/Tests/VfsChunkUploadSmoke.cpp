// Headless check of chunked entry upload: the service's own store and routes behind a real HTTP server,
// driven by the real client. Uploads a file larger than the service's old 100 MB single-request limit.
#include <JuceHeader.h>

#include "../Source/VfsProjectStore.h"
#include "../Source/VfsChunkUploadRoutes.h"

#include <creation/services/SuiteVfsServiceClient.h>

#include <atomic>
#include <cstdio>
#include <thread>

static int failures = 0;
static void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (! ok)
        ++failures;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    // Scratch space beside the test executable (inside the build tree), never a fixed path and never the system drive.
    const auto root = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile("vfs-chunk-smoke-work");
    root.deleteRecursively();
    root.createDirectory();

    creation::suite::SuiteSettings settings;
    settings.suiteVfsRoot = root.getFullPathName();
    VfsProjectStore store(settings);

    juce::String projectId, err;
    creation::assets::ProjectManifest manifest;
    if (! store.createProject(creation::assets::SuiteAppDomain::station, "chunk-smoke", "0", "0", projectId, manifest, err))
    {
        std::printf("could not create project: %s\n", err.toRawUTF8());
        return 2;
    }

    juce::CriticalSection lock;
    std::atomic<int> changed { 0 };
    httplib::Server http;
    registerVfsChunkUploadRoutes(http, store, lock, [&](const juce::String&) { ++changed; });
    // The service's plain single-request route, for the small-entry case.
    http.Put("/project/entry", [&](const httplib::Request& req, httplib::Response& res)
    {
        juce::String e;
        const juce::MemoryBlock data(req.body.data(), req.body.size());
        const juce::ScopedLock l(lock);
        if (! store.writeEntry(juce::String(req.get_param_value("projectId")), juce::String(req.get_param_value("path")), data, e))
            res.status = 500;
    });
    const int port = http.bind_to_any_port("127.0.0.1");
    std::thread server([&] { http.listen_after_bind(); });

    creation::services::SuiteVfsServiceClient client;
    client.setHttpPortForTesting(port);

    // Source: 150 MB of a repeating, position-dependent pattern (over the old 100 MB limit).
    const auto source = root.getChildFile("source.bin");
    const juce::int64 size = 150ll * 1024 * 1024 + 12345;
    {
        juce::FileOutputStream out(source);
        juce::MemoryBlock block(1 << 20);
        for (juce::int64 written = 0; written < size;)
        {
            const auto n = (size_t) juce::jmin<juce::int64>((juce::int64) block.getSize(), size - written);
            auto* p = (unsigned char*) block.getData();
            for (size_t i = 0; i < n; ++i)
                p[i] = (unsigned char) (((written + (juce::int64) i) * 2654435761ull) >> 13);
            out.write(block.getData(), n);
            written += (juce::int64) n;
        }
    }

    double lastFraction = 0.0;
    int calls = 0;
    const auto ok = client.writeProjectEntryFromFile(projectId, "Assets/big.bin", source,
                                                     [&](double f) { lastFraction = f; ++calls; return true; });
    check(ok, "150 MB upload succeeds");
    if (! ok)
        std::printf("   reason: %s\n", client.getLastWriteError().toRawUTF8());
    check(calls >= 9 && lastFraction > 0.999, "progress reported per piece, ending at 100%");
    check(changed == 1, "change broadcast fires once, on completion");

    juce::File projectFolder;
    store.findProjectFolderById(projectId, projectFolder);
    const auto stored = projectFolder.getChildFile("Assets/big.bin");
    check(stored.getSize() == size, "stored size matches");
    check(stored.getSize() == size && stored.hasIdenticalContentTo(source), "stored bytes match the source");
    check(! projectFolder.getChildFile("Assets/big.bin.upload-part").exists(), "no part file left behind");

    // Download the 150 MB entry back in pieces.
    {
        const auto back = root.getChildFile("download/big-back.bin");
        int downloadCalls = 0;
        const auto okDown = client.readProjectEntryToFile(projectId, "Assets/big.bin", back, [&](double) { ++downloadCalls; return true; });
        check(okDown, "150 MB download succeeds");
        if (! okDown)
            std::printf("   reason: %s\n", client.getLastReadError().toRawUTF8());
        check(okDown && back.getSize() == size && back.hasIdenticalContentTo(source), "downloaded bytes match the source");
        check(downloadCalls >= 9, "download reports progress per piece");

        const auto missing = client.readProjectEntryToFile(projectId, "Assets/nope.bin", root.getChildFile("download/nope.bin"));
        check(! missing && ! root.getChildFile("download/nope.bin").exists(), "missing entry fails and leaves no file");

        int cancelDown = 0;
        const auto cancelledDown = client.readProjectEntryToFile(projectId, "Assets/big.bin", root.getChildFile("download/cancel.bin"),
                                                                 [&](double) { return ++cancelDown < 2; });
        check(! cancelledDown && ! root.getChildFile("download/cancel.bin").exists(), "cancelled download leaves no file");
    }

    // Cancel part way: the existing entry must survive, and no part file may remain.
    int cancelCalls = 0;
    const auto cancelled = client.writeProjectEntryFromFile(projectId, "Assets/big.bin", source,
                                                            [&](double) { return ++cancelCalls < 3; });
    check(! cancelled && client.lastWriteWasCancelled(), "cancel is reported as a cancel");
    check(stored.getSize() == size && stored.hasIdenticalContentTo(source), "cancel leaves the earlier entry untouched");
    check(! projectFolder.getChildFile("Assets/big.bin.upload-part").exists(), "cancel removes the part file");

    // Out of order piece is refused.
    {
        juce::String e; bool done = false;
        const char data[] = "abc";
        check(! store.writeEntryChunk(projectId, "Assets/x.bin", 5, 10, data, 3, done, e), "out-of-order piece refused");
    }

    // Small in-memory entry, and a large in-memory one that has to go through pieces.
    juce::MemoryBlock smallBlock("hello", 5);
    check(client.writeProjectEntry(projectId, "Assets/small.txt", smallBlock), "small entry still uses the single request");
    juce::MemoryBlock read;
    check(store.readEntry(projectId, "Assets/small.txt", read) && read == smallBlock, "small entry stored intact");

    juce::MemoryBlock large((size_t) (30 * 1024 * 1024));
    for (size_t i = 0; i < large.getSize(); ++i)
        ((unsigned char*) large.getData())[i] = (unsigned char) (i * 31);
    check(client.writeProjectEntry(projectId, "Assets/large.bin", large), "30 MB in-memory entry goes up in pieces");
    check(store.readEntry(projectId, "Assets/large.bin", read) && read == large, "30 MB entry stored intact");

    // Empty file.
    const auto empty = root.getChildFile("empty.bin");
    empty.replaceWithText("");
    check(client.writeProjectEntryFromFile(projectId, "Assets/empty.bin", empty), "empty file uploads");
    check(projectFolder.getChildFile("Assets/empty.bin").existsAsFile(), "empty entry exists");

    // An older project service (only the plain single-request routes): small files must still work, and a file too
    // big for one request must fail with words, not silently.
    {
        httplib::Server oldHttp;
        oldHttp.Put("/project/entry", [&](const httplib::Request& req, httplib::Response& res)
        {
            juce::String e;
            const juce::MemoryBlock data(req.body.data(), req.body.size());
            const juce::ScopedLock l(lock);
            if (! store.writeEntry(juce::String(req.get_param_value("projectId")), juce::String(req.get_param_value("path")), data, e))
                res.status = 500;
        });
        oldHttp.Get("/project/entry", [&](const httplib::Request& req, httplib::Response& res)
        {
            juce::MemoryBlock data;
            const juce::ScopedLock l(lock);
            if (! store.readEntry(juce::String(req.get_param_value("projectId")), juce::String(req.get_param_value("path")), data))
            {
                res.status = 404;
                return;
            }
            res.set_content(static_cast<const char*>(data.getData()), data.getSize(), "application/octet-stream");
        });
        const int oldPort = oldHttp.bind_to_any_port("127.0.0.1");
        std::thread oldServer([&] { oldHttp.listen_after_bind(); });

        creation::services::SuiteVfsServiceClient oldClient;
        oldClient.setHttpPortForTesting(oldPort);

        const auto midFile = root.getChildFile("mid.bin");
        {
            juce::MemoryBlock mid((size_t) (5 * 1024 * 1024));
            for (size_t i = 0; i < mid.getSize(); ++i)
                ((unsigned char*) mid.getData())[i] = (unsigned char) (i * 7);
            midFile.replaceWithData(mid.getData(), mid.getSize());
        }
        check(oldClient.writeProjectEntryFromFile(projectId, "Assets/mid.bin", midFile), "older service: a 5 MB file still uploads");
        const auto midBack = root.getChildFile("download/mid-back.bin");
        check(oldClient.readProjectEntryToFile(projectId, "Assets/mid.bin", midBack) && midBack.hasIdenticalContentTo(midFile),
              "older service: a 5 MB file still downloads (plain read fallback)");
        check(! oldClient.readProjectEntryToFile(projectId, "Assets/absent.bin", root.getChildFile("download/absent.bin")),
              "older service: a missing entry still fails");

        check(! oldClient.writeProjectEntryFromFile(projectId, "Assets/toobig.bin", source), "older service: a 150 MB file is refused");
        check(oldClient.getLastWriteError().containsIgnoreCase("out of date"), "older service: the refusal says the service is out of date");

        oldHttp.stop();
        oldServer.join();
    }

    http.stop();
    server.join();
    root.deleteRecursively();

    std::printf("%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
