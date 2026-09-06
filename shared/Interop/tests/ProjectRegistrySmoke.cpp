#include <creation/assets/ProjectSession.h>
#include <creation/interop/ProjectRegistry.h>
#include <creation/suite/SuiteStoragePaths.h>

#include <iostream>
#include <stdexcept>

namespace
{
void fail(const std::string& message)
{
    std::cerr << message << std::endl;
    throw std::runtime_error(message);
}
}

int main()
{
    try
    {
        creation::suite::SuiteSettings settings;
        auto tempRoot = juce::File::getSpecialLocation(juce::File::tempDirectory)
                            .getChildFile("creation-suite-project-registry-smoke");
        tempRoot.deleteRecursively();
        tempRoot.createDirectory();

        settings.suiteVfsRoot = tempRoot.getChildFile("SuiteRoot").getFullPathName();

        juce::String errorMessage;

        creation::assets::ProjectSession stationSession;
        if (! creation::assets::ProjectSession::createNew(settings,
                                                          creation::assets::SuiteAppDomain::station,
                                                          "Station Project",
                                                          "0.1.0",
                                                          "0.1.0",
                                                          stationSession,
                                                          errorMessage))
            fail("Failed creating station container: " + errorMessage.toStdString());
        if (! stationSession.commit(errorMessage))
            fail("Failed committing station container: " + errorMessage.toStdString());
        stationSession.close();

        creation::assets::ProjectSession movieSession;
        if (! creation::assets::ProjectSession::createNew(settings,
                                                          creation::assets::SuiteAppDomain::movie,
                                                          "Movie Project",
                                                          "0.1.0",
                                                          "0.1.0",
                                                          movieSession,
                                                          errorMessage))
            fail("Failed creating movie container: " + errorMessage.toStdString());
        if (! movieSession.commit(errorMessage))
            fail("Failed committing movie container: " + errorMessage.toStdString());
        movieSession.close();

        auto projects = creation::interop::ProjectRegistry::discoverProjects(settings, errorMessage);
        if (projects.size() != 2)
            fail("Project registry did not discover both test containers.");

        bool foundStation = false;
        bool foundMovie = false;
        for (const auto& project : projects)
        {
            if (project.manifest.projectName == "Station Project")
                foundStation = true;
            if (project.manifest.projectName == "Movie Project")
                foundMovie = true;
        }

        if (! foundStation || ! foundMovie)
            fail("Project registry missed one or more manifests.");

        // appDomain is manifest metadata, never a query filter (there is no such thing as an
        // app owning a project) -- confirm queryProjects still returns everything, and that the
        // originating domain survived round-trip on the manifest itself.
        creation::interop::ProjectQuery query;
        auto allQueriedProjects = creation::interop::ProjectRegistry::queryProjects(settings, query, errorMessage);
        if (allQueriedProjects.size() != 2)
            fail("Project registry query unexpectedly filtered by app domain.");

        bool movieManifestHasMovieDomain = false;
        for (const auto& project : allQueriedProjects)
            if (project.manifest.projectName == "Movie Project")
                movieManifestHasMovieDomain = project.manifest.appDomain == creation::assets::SuiteAppDomain::movie;
        if (! movieManifestHasMovieDomain)
            fail("Movie project's manifest lost its originating app domain.");

        tempRoot.deleteRecursively();
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "ProjectRegistrySmoke failure: " << exception.what() << std::endl;
        return 1;
    }
}
