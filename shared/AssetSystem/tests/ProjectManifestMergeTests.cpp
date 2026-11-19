#include <creation/assets/ProjectManifest.h>
#include <iostream>
#include <stdexcept>
using namespace creation::assets;
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main()
{
    try
    {
        ProjectManifest base; base.projectId="test-project";base.projectName="Shared project";base.appDomain=SuiteAppDomain::station;
        AssetDescriptor audio;audio.id="audio";audio.versionId="audio@1";audio.logicalPath="Assets/Source/tone.wav";
        audio.details.set("channels","2");audio.details.set("sampleRate","48000");
        base.assetCatalog.assets.add(audio);
        auto station=base,movie=base;juce::String error;ProjectManifest merged;
        auto video=audio;video.id="video";video.versionId="video@1";video.logicalPath="Media/video.mkv";
        movie.assetCatalog.assets.add(video);
        station.projectName="Renamed in Station";
        check(mergeProjectManifest(base,station,movie,merged,error),"Disjoint metadata/catalog changes should merge");
        check(merged.projectName==station.projectName && merged.assetCatalog.assets.size()==2,"Peer assets or project metadata were lost");
        auto stale=base;
        check(mergeProjectManifest(base,stale,merged,station,error),"Unchanged stale session should refresh");
        check(station.assetCatalog.findByVersionId("video@1")!=nullptr,"Stale save removed peer media");
        auto removal=base;removal.assetCatalog.assets.clear();
        check(mergeProjectManifest(base,removal,movie,merged,error),"Local removal should preserve remote additions");
        check(merged.assetCatalog.assets.size()==1 && merged.assetCatalog.assets[0].id=="video","Removal resurrected an asset or removed peer addition");
        auto edit=base;edit.assetCatalog.assets.getReference(0).displayName="Edited";
        auto reordered=base;
        auto& details=reordered.assetCatalog.assets.getReference(0).details;
        details.clear();details.set("sampleRate","48000");details.set("channels","2");
        check(mergeProjectManifest(base,edit,reordered,merged,error),"JSON property ordering must not create an asset conflict");
        const auto untouched=serializeManifest(merged);
        check(!mergeProjectManifest(base,removal,edit,merged,error),"Delete/update conflict must reject");
        check(serializeManifest(merged)==untouched,"Rejected merge modified output");
        auto version=audio;version.versionId="audio@2";version.logicalPath="Assets/Derived/tone@2.wav";
        auto newVersion=base;newVersion.assetCatalog.assets.add(version);
        check(mergeProjectManifest(base,edit,newVersion,merged,error),"Independent asset versions should merge");
        check(merged.assetCatalog.findByVersionId("audio@1") && merged.assetCatalog.findByVersionId("audio@2"),"Exact versions were lost");
        auto rename=base;rename.projectName="Other name";
        check(!mergeProjectManifest(base,station,rename,merged,error),"Conflicting project names must reject");
        rename=base;rename.projectId="other-project";
        check(!mergeProjectManifest(base,base,rename,merged,error),"Different project identities must reject");
        std::cout<<"PASS: shared catalog merge, stale save, deletion, conflict atomicity, exact versions and identity\n";
        return 0;
    }
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
