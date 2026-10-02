#include "fake_fs.h"
#include <cstdlib>
namespace tu {
FileSystem& native_filesystem() {
    static FakeFS fs; static bool prepared=false;
    if (!prepared && !std::getenv("TU_PREVIEW_EMPTY")) {
        prepared=true;
        fs.dirs["/user/app"]={{"CUSA12345",4},{"CUSA54321",4},{"CUSA00111",4}};
        fs.dirs["/user/appmeta"]={{"CUSA12345",4},{"CUSA54321",4},{"CUSA00111",4}};
        for (const char* id : {"CUSA12345","CUSA54321","CUSA00111"}) fs.files[std::string("/user/app/")+id+"/app.pkg"]={1};
        fs.files["/user/appmeta/CUSA00111/param.sfo"]=sfo("CUSA00111","Jogo de teste: Ação e aventura");
        fs.files["/user/appmeta/CUSA12345/param.sfo"]=sfo("CUSA12345","Segundo jogo de teste");
        fs.files["/user/appmeta/CUSA54321/param.sfo"]=sfo("CUSA54321","Terceiro jogo de teste");
    }
    return fs;
}
AccessResult request_goldhen_access() { AccessResult r; r.attempted=true; r.sdk=0x100; return r; }
}
