#include "unlock.h"
#include "library.h"
#include "filesystem.h"
#include <orbis/libkernel.h>
#include <orbis/Sysmodule.h>
#include <orbis/UserService.h>
#include <orbis/NpTrophy.h>
#include <fcntl.h>
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace tu {
namespace {
static int native_error(int64_t result) {
    uint32_t bits = uint32_t(result);
    if ((bits & 0xffff0000u) == 0x80020000u) return -int(bits & 0xffffu);
    return result < 0 ? int(result) : 0;
}
static int read_file(const std::string& path, size_t limit, std::vector<uint8_t>& out) {
    out.clear(); int fd=sceKernelOpen(path.c_str(),O_RDONLY,0); if(fd<0) return native_error(fd);
    uint8_t buf[8192]; int status=0;
    for (;;) { size_t want=std::min(sizeof(buf),limit+1-out.size()); int64_t n=sceKernelRead(fd,buf,want); int err=native_error(n); if(err==-EINTR) continue; if(err){status=err;break;} if(!n) break; out.insert(out.end(),buf,buf+n); if(out.size()>limit){status=-EFBIG;break;} }
    int c=native_error(sceKernelClose(fd)); if(!status) status=c; if(status) out.clear(); return status;
}
static int write_file(const std::string& path,const std::vector<uint8_t>& data) {
    int fd=sceKernelOpen(path.c_str(),O_WRONLY|O_CREAT|O_TRUNC,0644); if(fd<0) return native_error(fd);
    size_t pos=0; int status=0;
    while(pos<data.size()){int64_t n=sceKernelWrite(fd,data.data()+pos,data.size()-pos);int err=native_error(n);if(err==-EINTR)continue;if(err){status=err;break;}if(n<=0||uint64_t(n)>data.size()-pos){status=-EIO;break;}pos+=size_t(n);} if(!status) status=native_error(sceKernelFsync(fd)); int c=native_error(sceKernelClose(fd)); if(!status) status=c; return status;
}
static bool replace_title_id(std::vector<uint8_t>& bytes,const std::string& from){const std::string old_id=from+"_00",new_id="BREW13533_00";if(old_id.size()!=new_id.size())return false;auto it=std::search(bytes.begin(),bytes.end(),old_id.begin(),old_id.end());if(it==bytes.end())return false;std::copy(new_id.begin(),new_id.end(),it);return true;}
static bool locate_game_trophy_files(const Game& game,std::string& nptitle,std::string& npbind){const std::string roots[]={"/system_data/priv/appmeta/"+game.id,"/user/appmeta/"+game.id,"/user/appmeta/external/"+game.id,game.path+"/sce_sys"};for(const std::string& root:roots){if(root.empty())continue;std::vector<uint8_t>a,b;if(!read_file(root+"/nptitle.dat",1024*1024,a)&&!read_file(root+"/npbind.dat",1024*1024,b)){nptitle=root+"/nptitle.dat";npbind=root+"/npbind.dat";return true;}}return false;}
static std::string code(const char* where,int value){char buf[128];snprintf(buf,sizeof(buf),"%s: 0x%08x",where,unsigned(value));return buf;}
}
UnlockResult unlock_trophy_for_game(const Game& game,int trophy_id){
    UnlockResult result;
    if(!game.package||!is_cusa(game.id)||trophy_id<0||trophy_id>255){result.status=-EINVAL;result.detail="Jogo/trofeu invalido.";return result;}
    AccessResult access=request_goldhen_access(); if(!access.acknowledged()){result.status=-EACCES;result.detail="GoldHEN nao confirmou acesso necessario ao Trophy Hijack.";return result;}
    std::string game_nptitle_path,game_npbind_path; if(!locate_game_trophy_files(game,game_nptitle_path,game_npbind_path)){result.status=-ENOENT;result.detail="Nao encontrei nptitle.dat/npbind.dat do jogo instalado.";return result;}
    std::vector<uint8_t>game_nptitle,game_npbind; int st=read_file(game_nptitle_path,1024*1024,game_nptitle);if(st){result.status=st;result.detail="Falha ao ler nptitle.dat do jogo.";return result;} st=read_file(game_npbind_path,1024*1024,game_npbind);if(st){result.status=st;result.detail="Falha ao ler npbind.dat do jogo.";return result;} if(!replace_title_id(game_nptitle,game.id)){result.status=-EINVAL;result.detail="nptitle.dat nao contem o Title ID esperado do jogo.";return result;}
    const std::string host_root="/system_data/priv/appmeta/BREW13533",host_nptitle=host_root+"/nptitle.dat",host_npbind=host_root+"/npbind.dat";
    std::vector<uint8_t>original_nptitle,original_npbind; st=read_file(host_nptitle,1024*1024,original_nptitle);if(st){result.status=st;result.detail="Falha ao guardar nptitle.dat original do app.";return result;} st=read_file(host_npbind,1024*1024,original_npbind);if(st){result.status=st;result.detail="Falha ao guardar npbind.dat original do app.";return result;}
    bool touched_nptitle=false,touched_npbind=false; int api=0; int32_t context=-1,handle=-1,plat=-1,user=0;
    st=write_file(host_npbind,game_npbind);if(st){result.status=st;result.detail="Falha ao aplicar npbind.dat temporario.";return result;}touched_npbind=true;
    st=write_file(host_nptitle,game_nptitle);if(st){write_file(host_npbind,original_npbind);result.status=st;result.restored=true;result.detail="Falha ao aplicar nptitle.dat temporario.";return result;}touched_nptitle=true;
    api=sceUserServiceInitialize(nullptr);if(api!=0&&uint32_t(api)!=0x80960003u){result.detail=code("sceUserServiceInitialize",api);goto restore;}
    api=sceUserServiceGetInitialUser(&user);if(api<0){result.detail=code("sceUserServiceGetInitialUser",api);goto restore;}
    api=sceSysmoduleLoadModule(ORBIS_SYSMODULE_NP_TROPHY);if(api!=0&&uint32_t(api)!=0x8002D002u){result.detail=code("sceSysmoduleLoadModule(NP_TROPHY)",api);goto restore;}
    api=sceNpTrophyCreateContext(&context,user,0,0);if(api<0){result.detail=code("sceNpTrophyCreateContext",api);goto restore;}
    api=sceNpTrophyCreateHandle(&handle);if(api<0){result.detail=code("sceNpTrophyCreateHandle",api);goto restore;}
    api=sceNpTrophyRegisterContext(context,handle,0);if(api<0){result.detail=code("sceNpTrophyRegisterContext",api);goto restore;}
    api=sceNpTrophyUnlockTrophy(context,handle,trophy_id,&plat);result.api_status=api;if(api<0){result.detail=code("sceNpTrophyUnlockTrophy",api);goto restore;}
    result.status=0;result.detail="Trofeu desbloqueado pela API nativa do PS4.";
restore:
    if(handle>=0)sceNpTrophyDestroyHandle(handle);if(context>=0)sceNpTrophyDestroyContext(context);
    {int r1=touched_nptitle?write_file(host_nptitle,original_nptitle):0;int r2=touched_npbind?write_file(host_npbind,original_npbind):0;result.restored=(r1==0&&r2==0);if(!result.restored){if(result.status==0)result.status=r1?r1:r2;result.detail+=" | restauracao dos metadados falhou";}}
    if(result.status!=0&&api<0)result.status=api;return result;
}
}
