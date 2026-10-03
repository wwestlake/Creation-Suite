#pragma once

#include <creation/litesemrag/Cards.h>
#include <creation/assets/ProjectManifest.h>

namespace creation::litesemrag
{
// The user's cards, kept in the VFS like every other setting - never a file on the OS
// (docs/architecture/Suite-Shared-Project-Model.md):
//
//   suite    suite entry   cards/suite.json
//   app      suite entry   cards/app/<app>.json      (<app> is the app's storage token: "texture", "station"...)
//   project  project entry Cards/cards.json           inside the project, so it travels with it
//
// Shipped cards are not stored here: they are built into the suite from source.
class CardStore final
{
public:
    // Where cards are, for one scope: the app for app cards, the project id for project cards.
    struct Place
    {
        CardScope scope = CardScope::suite;
        creation::assets::SuiteAppDomain app = creation::assets::SuiteAppDomain::unknown;
        juce::String projectId;
    };

    static Place suite();
    static Place forApp(creation::assets::SuiteAppDomain app);
    static Place forProject(const juce::String& projectId);

    // Nothing saved yet is no error: an empty set. errorMessage is set when the VFS cannot be reached or the stored
    // cards cannot be read.
    static juce::Array<Card> load(const Place& place, juce::String& errorMessage);
    // Saves the whole set; refuses a set with an invalid card (errorMessage says which and why).
    static bool save(const Place& place, const juce::Array<Card>& cards, juce::String& errorMessage);

    // One card added, or replaced by id; one card removed.
    static bool upsert(const Place& place, const Card& card, juce::String& errorMessage);
    static bool remove(const Place& place, const juce::String& cardId, juce::String& errorMessage);
};
} // namespace creation::litesemrag
