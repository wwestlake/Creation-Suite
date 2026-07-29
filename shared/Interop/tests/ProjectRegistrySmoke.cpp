#include <creation/assets/ProjectContainerIO.h>
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

        const auto stationManifest = creation::suite::createDefaultManifest("Station Project",
                                                                            creation::assets::SuiteAppDomain::station,
                                                                            "0.1.0",
                                                                            "0.1.0");
        const auto movieManifest = creation::suite::createDefaultManifest("Movie Project",
                                                                          creation::assets::SuiteAppDomain::movie,
                                                                          "0.1.0",
                                                                          "0.1.0");

        juce::String errorMessage;
        if (! creation::assets::ProjectContainerIO::writeContainer(
                creation::suite::getProjectContainerPath(settings, creation::assets::SuiteAppDomain::station, stationManifest.projectName),
                stationManifest,
                {},
                errorMessage))
            fail("Failed writing station container: " + errorMessage.toStdString());

        if (! creation::assets::ProjectContainerIO::writeContainer(
                creation::suite::getProjectContainerPath(settings, creation::assets::SuiteAppDomain::movie, movieManifest.projectName),
                movieManifest,
                {},
                errorMessage))
            fail("Failed writing movie container: " + errorMessage.toStdString());

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
