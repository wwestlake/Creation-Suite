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
        settings.projectContainersRoot = tempRoot.getChildFile("Containers").getFullPathName();

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

        creation::interop::ProjectQuery query;
        query.appDomain = creation::assets::SuiteAppDomain::movie;
        auto movieProjects = creation::interop::ProjectRegistry::queryProjects(settings, query, errorMessage);
        if (movieProjects.size() != 1 || movieProjects[0].manifest.projectName != "Movie Project")
            fail("Project registry domain query returned the wrong result.");

        tempRoot.deleteRecursively();
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "ProjectRegistrySmoke failure: " << exception.what() << std::endl;
        return 1;
    }
}
