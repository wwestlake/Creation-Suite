#pragma once

#include <creation/services/SuiteVfsServiceClient.h>

#include <juce_core/juce_core.h>

#include <deque>

namespace creation::assets
{
// Reads one VFS entry as a plain juce::InputStream, in pieces, straight from the VFS service. Nothing is copied out to
// a file: this is how a big asset (a 2 GB video) is played or decoded without ever leaving the container. Seeking is
// cheap - the stream keeps a few recently used pieces in memory and asks the service for the piece that is needed.
//
// One stream is not thread safe (like any juce::InputStream): whoever uses it from several threads must lock around it.
class VfsEntryInputStream final : public juce::InputStream
{
public:
    VfsEntryInputStream(creation::services::SuiteVfsServiceClient client, juce::String projectId, juce::String logicalPath);

    // False if the entry could not be found or the service could not be reached.
    bool isValid() const noexcept { return totalSize >= 0; }

    juce::int64 getTotalLength() override { return totalSize; }
    bool isExhausted() override { return totalSize < 0 || position >= totalSize; }
    int read(void* destination, int maxBytesToRead) override;
    juce::int64 getPosition() override { return position; }
    bool setPosition(juce::int64 newPosition) override;

private:
    struct Piece
    {
        juce::int64 start = 0;
        juce::MemoryBlock data;
    };

    const Piece* pieceFor(juce::int64 offset);

    static constexpr juce::int64 kPieceBytes = 4 * 1024 * 1024;
    static constexpr size_t kMaxPieces = 6;

    creation::services::SuiteVfsServiceClient client;
    juce::String projectId, logicalPath;
    juce::int64 totalSize = -1;
    juce::int64 position = 0;
    std::deque<Piece> pieces; // most recently used at the front

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VfsEntryInputStream)
};

// Opens a stream on its own connection to the VFS service (finding or starting the service if needed). Null if the
// entry does not exist or the service cannot be reached. Safe to call from any thread.
std::unique_ptr<juce::InputStream> openVfsEntryStream(const juce::String& projectId, const juce::String& logicalPath);
}
