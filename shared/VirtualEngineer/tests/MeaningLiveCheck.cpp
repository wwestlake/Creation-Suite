// Not a test: asks the suite's real AI account (the one an app uses) for embeddings of a few sample cards and requests
// and prints how close each request comes to each card - what the meaning threshold in MeaningMatch is set from. It
// reads ai-settings.json and writes nothing.
//
//   creation_suite_meaning_live_check [app]      app is a storage token, texture by default

#include <creation/litesemrag/Embeddings.h>
#include <creation/services/SuiteAiEmbeddingClient.h>

#include <cstdio>
#include <iostream>

namespace ls = creation::litesemrag;
namespace services = creation::services;

int main(int argc, char** argv)
{
    const auto app = creation::assets::suiteAppDomainFromStorageToken(argc > 1 ? juce::String(argv[1]) : juce::String("texture"));
    juce::String error;
    const auto settings = services::SuiteAiSettingsStore().load(error);
    const auto runtime = services::SuiteAiSettingsResolver::resolveRuntimeSettingsForApp(settings, app);
    std::cout << "account " << runtime.accountId << ", provider " << runtime.providerId << ", embedding model "
              << services::SuiteAiEmbeddingClient::embeddingModelFor(runtime).name << "\n\n";

    auto card = [](const char* title, const char* text, juce::StringArray tokens) {
        ls::Card c;
        c.title = title;
        c.kind = "knowledge";
        c.text = text;
        c.tokens = tokens;
        return c;
    };
    const juce::Array<ls::Card> cards {
        card("Storage", "Everything a suite app keeps goes inside the VFS container; the only file on the OS is the root pointer.", { "vfs", "storage" }),
        card("Structs", "A Frust struct is a product type with named fields; Make builds one, Break takes it apart.", { "struct", "fields" }),
        card("Adding a node", "Add nodes to the graph from the palette; connect outputs to inputs of the same type.", { "node", "graph" }),
        card("Colour", "Colours in Texture graphs are linear RGBA floats; convert sRGB inputs before blending.", { "colour", "rgb" }),
        card("Compile errors", "When the graph fails to compile, read the error panel, fix the first error, then rebuild.", { "error", "compile" }),
    };
    const juce::StringArray requests {
        "Where do my saved files live?",
        "How do I hold my company details in one value?",
        "How do I hook two boxes together?",
        "The picture looks washed out after mixing two images",
        "It says something is wrong and won't run",
        "What's the weather tomorrow?",
        "Write me a poem about the sea",
    };

    juce::StringArray texts;
    for (const auto& c : cards)
        texts.add(ls::embeddingText(c));
    texts.addArray(requests);
    services::SuiteAiEmbeddingClient::Result result;
    if (! services::SuiteAiEmbeddingClient().embed(runtime, texts, result))
    {
        std::cerr << "embeddings failed: " << result.errorMessage << "\n";
        return 1;
    }

    std::printf("%-55s", "");
    for (const auto& c : cards)
        std::printf("%10.9s", c.title.toRawUTF8());
    std::printf("\n");
    for (int r = 0; r < requests.size(); ++r)
    {
        std::printf("%-55.55s", requests[r].toRawUTF8());
        for (int c = 0; c < cards.size(); ++c)
            std::printf("%10.3f", ls::similarity(result.vectors[static_cast<size_t>(cards.size() + r)], result.vectors[static_cast<size_t>(c)]));
        std::printf("\n");
    }
    return 0;
}
