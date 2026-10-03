#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(sys.argv[1])

def replace_once(path, old, new):
    p = root / path
    s = p.read_text()
    if old not in s:
        raise SystemExit(f'patch anchor not found in {path}: {old[:120]}')
    p.write_text(s.replace(old, new, 1))

# Covers: only accept an icon candidate when its bytes are readable and the PNG
# header is valid; otherwise continue to the appmeta fallbacks. Allow larger
# PS4 icon files while still keeping a strict upper bound.
replace_once('library.cpp',
    'return be(8) == 13 && be(16) > 0 && be(20) > 0 && be(16) <= 2048 && be(20) <= 2048;',
    'return be(8) == 13 && be(16) > 0 && be(20) > 0 && be(16) <= 4096 && be(20) <= 4096;')

replace_once('library.cpp', '''        if (g.icon.empty()) {
            for (const char* suffix : {"/icon0.png", "/sce_sys/icon0.png"}) {
                if (fs.exists(base+suffix)) { g.icon = base+suffix; break; }
            }
        }''', '''        if (g.icon.empty()) {
            for (const char* suffix : {"/icon0.png", "/sce_sys/icon0.png"}) {
                std::string candidate=base+suffix;
                std::vector<uint8_t> icon_bytes;
                if (!fs.read(candidate,12*1024*1024,icon_bytes) && valid_png(icon_bytes)) {
                    g.icon=candidate;
                    break;
                }
            }
        }''')

replace_once('native_fs.cpp',
    'if (!valid_path(path) || limit > 4*1024*1024) return -EINVAL;',
    'if (!valid_path(path) || limit > 16*1024*1024) return -EINVAL;')

replace_once('main.cpp',
    'fs.read(game.icon,4*1024*1024,bytes)',
    'fs.read(game.icon,12*1024*1024,bytes)')

# Trophy Hijack: the homebrew itself normally has no nptitle.dat/npbind.dat.
# Missing originals are therefore valid. In that case install temporary game
# metadata and remove it afterward instead of aborting before the Trophy API.
replace_once('unlock.cpp', '''static int write_file(const std::string& path,const std::vector<uint8_t>& data) {
    int fd=sceKernelOpen(path.c_str(),O_WRONLY|O_CREAT|O_TRUNC,0644); if(fd<0) return native_error(fd);
    size_t pos=0; int status=0;
    while(pos<data.size()){int64_t n=sceKernelWrite(fd,data.data()+pos,data.size()-pos);int err=native_error(n);if(err==-EINTR)continue;if(err){status=err;break;}if(n<=0||uint64_t(n)>data.size()-pos){status=-EIO;break;}pos+=size_t(n);} if(!status) status=native_error(sceKernelFsync(fd)); int c=native_error(sceKernelClose(fd)); if(!status) status=c; return status;
}''', '''static int write_file(const std::string& path,const std::vector<uint8_t>& data) {
    int fd=sceKernelOpen(path.c_str(),O_WRONLY|O_CREAT|O_TRUNC,0644); if(fd<0) return native_error(fd);
    size_t pos=0; int status=0;
    while(pos<data.size()){int64_t n=sceKernelWrite(fd,data.data()+pos,data.size()-pos);int err=native_error(n);if(err==-EINTR)continue;if(err){status=err;break;}if(n<=0||uint64_t(n)>data.size()-pos){status=-EIO;break;}pos+=size_t(n);} if(!status) status=native_error(sceKernelFsync(fd)); int c=native_error(sceKernelClose(fd)); if(!status) status=c; return status;
}
static int remove_file(const std::string& path) {
    int st=native_error(sceKernelUnlink(path.c_str()));
    return st == -ENOENT ? 0 : st;
}''')

replace_once('unlock.cpp', '''    const std::string host_root="/system_data/priv/appmeta/BREW13533",host_nptitle=host_root+"/nptitle.dat",host_npbind=host_root+"/npbind.dat";
    std::vector<uint8_t>original_nptitle,original_npbind; st=read_file(host_nptitle,1024*1024,original_nptitle);if(st){result.status=st;result.detail="Falha ao guardar nptitle.dat original do app.";return result;} st=read_file(host_npbind,1024*1024,original_npbind);if(st){result.status=st;result.detail="Falha ao guardar npbind.dat original do app.";return result;}
    bool touched_nptitle=false,touched_npbind=false; int api=0; int32_t context=-1,handle=-1,plat=-1,user=0;
    st=write_file(host_npbind,game_npbind);if(st){result.status=st;result.detail="Falha ao aplicar npbind.dat temporario.";return result;}touched_npbind=true;
    st=write_file(host_nptitle,game_nptitle);if(st){write_file(host_npbind,original_npbind);result.status=st;result.restored=true;result.detail="Falha ao aplicar nptitle.dat temporario.";return result;}touched_nptitle=true;''', '''    const std::string host_root="/system_data/priv/appmeta/BREW13533",host_nptitle=host_root+"/nptitle.dat",host_npbind=host_root+"/npbind.dat";
    int mk=native_error(sceKernelMkdir(host_root.c_str(),0755));
    if(mk && mk!=-EEXIST){result.status=mk;result.detail="Falha ao preparar appmeta temporario do Trophy Hijack.";return result;}
    std::vector<uint8_t>original_nptitle,original_npbind;
    bool had_original_nptitle=false,had_original_npbind=false;
    st=read_file(host_nptitle,1024*1024,original_nptitle);
    if(!st) had_original_nptitle=true;
    else if(st!=-ENOENT){result.status=st;result.detail="Falha ao guardar nptitle.dat original do app.";return result;}
    st=read_file(host_npbind,1024*1024,original_npbind);
    if(!st) had_original_npbind=true;
    else if(st!=-ENOENT){result.status=st;result.detail="Falha ao guardar npbind.dat original do app.";return result;}
    bool touched_nptitle=false,touched_npbind=false; int api=0; int32_t context=-1,handle=-1,plat=-1,user=0;
    st=write_file(host_npbind,game_npbind);if(st){result.status=st;result.detail="Falha ao aplicar npbind.dat temporario.";return result;}touched_npbind=true;
    st=write_file(host_nptitle,game_nptitle);if(st){
        int cleanup=had_original_npbind?write_file(host_npbind,original_npbind):remove_file(host_npbind);
        result.status=st;result.restored=(cleanup==0);result.detail="Falha ao aplicar nptitle.dat temporario.";return result;
    }touched_nptitle=true;''')

replace_once('unlock.cpp', '''    {int r1=touched_nptitle?write_file(host_nptitle,original_nptitle):0;int r2=touched_npbind?write_file(host_npbind,original_npbind):0;result.restored=(r1==0&&r2==0);if(!result.restored){if(result.status==0)result.status=r1?r1:r2;result.detail+=" | restauracao dos metadados falhou";}}''', '''    {int r1=touched_nptitle?(had_original_nptitle?write_file(host_nptitle,original_nptitle):remove_file(host_nptitle)):0;int r2=touched_npbind?(had_original_npbind?write_file(host_npbind,original_npbind):remove_file(host_npbind)):0;result.restored=(r1==0&&r2==0);if(!result.restored){if(result.status==0)result.status=r1?r1:r2;result.detail+=" | restauracao dos metadados falhou";}}''')

print('[OK] V13.11 cover + Trophy Hijack fixes applied')
