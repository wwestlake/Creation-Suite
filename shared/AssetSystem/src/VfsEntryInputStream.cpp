#include <creation/assets/VfsEntryInputStream.h>

namespace creation::assets
{
VfsEntryInputStream::VfsEntryInputStream(creation::services::SuiteVfsServiceClient clientToUse, juce::String projectIdToUse,
                                         juce::String logicalPathToUse)
    : client(std::move(clientToUse)), projectId(std::move(projectIdToUse)), logicalPath(std::move(logicalPathToUse))
{
    // The first piece both proves the entry exists and tells us how big it is.
    pieceFor(0);
}

bool VfsEntryInputStream::setPosition(juce::int64 newPosition)
{
    if (newPosition < 0)
        return false;

    position = totalSize >= 0 ? juce::jmin(newPosition, totalSize) : newPosition;
    return true;
}

const VfsEntryInputStream::Piece* VfsEntryInputStream::pieceFor(juce::int64 offset)
{
    for (auto it = pieces.begin(); it != pieces.end(); ++it)
    {
        if (offset >= it->start && offset < it->start + (juce::int64) it->data.getSize())
        {
            if (it != pieces.begin())
            {
                auto hit = std::move(*it);
                pieces.erase(it);
                pieces.push_front(std::move(hit));
            }
            return &pieces.front();
        }
    }

    const auto start = (offset / kPieceBytes) * kPieceBytes;
    Piece piece;
    piece.start = start;
    juce::int64 total = 0;
    if (! client.readProjectEntryRange(projectId, logicalPath, start, kPieceBytes, piece.data, total))
        return nullptr;

    totalSize = total;
    if (piece.data.getSize() == 0)
        return nullptr;

    pieces.push_front(std::move(piece));
    while (pieces.size() > kMaxPieces)
        pieces.pop_back();
    return &pieces.front();
}

int VfsEntryInputStream::read(void* destination, int maxBytesToRead)
{
    if (totalSize < 0 || maxBytesToRead <= 0)
        return 0;

    auto* out = static_cast<char*>(destination);
    int done = 0;
    while (done < maxBytesToRead && position < totalSize)
    {
        const auto* piece = pieceFor(position);
        if (piece == nullptr)
            break;

        const auto within = (size_t) (position - piece->start);
        const auto available = piece->data.getSize() - within;
        const auto count = (int) juce::jmin<size_t>(available, (size_t) (maxBytesToRead - done));
        std::memcpy(out + done, static_cast<const char*>(piece->data.getData()) + within, (size_t) count);
        done += count;
        position += count;
    }
    return done;
}

std::unique_ptr<juce::InputStream> openVfsEntryStream(const juce::String& projectId, const juce::String& logicalPath)
{
    creation::services::SuiteVfsServiceClient client;
    if (! client.discover())
        return nullptr;

    auto stream = std::make_unique<VfsEntryInputStream>(client, projectId, logicalPath);
    if (! stream->isValid())
        return nullptr;

    return stream;
}
}
