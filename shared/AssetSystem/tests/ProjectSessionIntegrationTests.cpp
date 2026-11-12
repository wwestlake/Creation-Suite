#include <creation/assets/ProjectSession.h>
#include <creation/services/SuiteVfsServiceClient.h>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace creation::assets;
static void check(bool value,const juce::String& message){if(!value)throw std::runtime_error(message.toStdString());}
int main()
{
    creation::services::SuiteVfsServiceClient client;
    juce::String id;
    try
    {
        check(client.discover(),"Service unavailable");juce::String error;
        ProjectSession station,movie;
        check(ProjectSession::createNew({},SuiteAppDomain::station,"Order 1 session check "+juce::Uuid().toString(),"1","1",station,error),error);
        id=station.getProjectId();check(ProjectSession::open({},id,movie,error),error);
        const juce::MemoryBlock audio("AUDIO",5),video("VIDEO",5),sessionData("STATION",7),sequenceData("MOVIE",5);
        check(station.writeEntry("session.xml",sessionData),"Station state write failed");
        check(movie.writeEntry("Project/movie-state.json",sequenceData),"Movie state write failed");
        auto add=[](ProjectSession& session,const char* name,const juce::MemoryBlock& bytes)
        {
            AssetDescriptor a;a.id=name;a.versionId=a.id+"@1";a.logicalPath="Assets/Source/"+a.id;
            check(session.writeEntry(a.logicalPath,bytes),"Asset write failed");session.upsertAssetDescriptor(a);
        };
        add(station,"audio",audio);add(movie,"video",video);
        check(station.commit(error),error);check(movie.commit(error),error);
        check(station.refreshManifest(error),error);
        check(station.getManifest().assetCatalog.assets.size()==2,"Stale Movie save lost Station's catalog addition");
        ProjectManifest stale;bool supported=false;
        check(client.readManifest(id,stale,&supported) && supported,"Service lacks conditional manifest saves");
        auto changed=stale;changed.projectName="Concurrent rename";
        ProjectManifest saved;
        check(client.compareAndWriteManifest(id,changed,stale.revision,saved)==creation::services::SuiteVfsServiceClient::ManifestWriteResult::saved,"Conditional save failed");
        check(client.compareAndWriteManifest(id,stale,stale.revision,changed)==creation::services::SuiteVfsServiceClient::ManifestWriteResult::conflict,"Stale revision was accepted");
        check(client.compareAndWriteManifest(id,stale,-1,changed)==creation::services::SuiteVfsServiceClient::ManifestWriteResult::failed,"Missing revision was accepted by the client");
        check(station.commit(error),error);
        check(station.getManifest().projectName=="Concurrent rename","Stale app overwrote peer metadata");
        juce::MemoryBlock data;bool found=true;
        check(station.readOptionalEntry("not-created.json",data,found,error) && !found,"Missing app-specific state should be optional");
        check(station.readOptionalEntry("session.xml",data,found,error) && found && data==sessionData,"Movie changed Station state");
        check(station.readEntry("Project/movie-state.json",data) && data==sequenceData,"Station changed Movie state");
        for(int i=0;i<3;++i)
        {
            ProjectSession next;check(ProjectSession::open({},id,next,error),error);check(next.commit(error),error);
            check(next.getManifest().projectId==id && next.getManifest().assetCatalog.assets.size()==2,"Cross-app cycle changed identity/assets");
        }
        check(client.deleteProject(id,error),error);id={};
        std::cout<<"PASS: live shared sessions, stale catalog save, CAS rejection, peer metadata, optional state and repeated persistence\n";return 0;
    }
    catch(const std::exception& e)
    {
        if(id.isNotEmpty()){juce::String error;client.deleteProject(id,error);}
        std::cerr<<e.what()<<'\n';return 1;
    }
}
