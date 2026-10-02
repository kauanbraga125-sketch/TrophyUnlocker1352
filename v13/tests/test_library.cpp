#include "fake_fs.h"
#include <cassert>
#include <cstdio>

static std::vector<uint8_t> dirent(const char* name,uint8_t type=4) {
    size_t len=strlen(name), size=(9+len+3)&~size_t(3);
    std::vector<uint8_t> bytes(size,0); put32(bytes,0,123);
    put16(bytes,4,size); bytes[6]=type; bytes[7]=len;
    memcpy(bytes.data()+8,name,len); return bytes;
}
static void wire_test() {
    std::vector<tu::DirEntry> entries;
    auto first=dirent("CUSA12345"), second=dirent("CUSA54321");
    first.insert(first.end(),second.begin(),second.end());
    assert(tu::parse_dirents(first.data(),first.size(),entries) == 0);
    assert(entries.size() == 2 && entries[1].name == "CUSA54321");
    entries.clear(); first[first.size()-second.size()+4]=0;
    assert(tu::parse_dirents(first.data(),first.size(),entries) < 0 && entries.empty());
    first=dirent("CUSA12345"); first[7]=255;
    assert(tu::parse_dirents(first.data(),first.size(),entries) < 0);
    first=dirent("CUSA12345"); first[17]='x';
    assert(tu::parse_dirents(first.data(),first.size(),entries) < 0);
    assert(!tu::valid_path("/user/app/../x") && !tu::valid_path("/user/app\n"));
    assert(tu::join_path("/user/app","CUSA12345") == "/user/app/CUSA12345");
    assert(tu::join_path("/user/app","../x").empty());
    assert(tu::parent_path("/user") == "/");
    puts("PASS native dirent boundaries and paths");
}
static void sfo_test() {
    auto bytes=sfo("CUSA12345","Ação e aventura"); std::string id,title;
    assert(tu::parse_sfo(bytes,id,title) && id == "CUSA12345" && title == "Ação e aventura");
    auto bad=bytes; put32(bad,8,0xfffffff0);
    assert(!tu::parse_sfo(bad,id,title) && id.empty() && title.empty());
    bad=bytes; put32(bad,48,0xfffffff0); assert(!tu::parse_sfo(bad,id,title));
    bad=bytes; put32(bad,28,1); assert(!tu::parse_sfo(bad,id,title));
    bad=bytes; bad[77]='A'; assert(!tu::parse_sfo(bad,id,title));
    // Deterministic malformed-file corpus under ASan/UBSan.
    uint32_t seed=0x1352;
    for (int i=0; i<5000; ++i) {
        seed=1664525*seed+1013904223;
        bad=bytes; bad[seed%bad.size()]=uint8_t(seed>>16);
        if (i%2) bad.resize(seed%bad.size());
        tu::parse_sfo(bad,id,title);
        std::vector<tu::DirEntry> out; tu::parse_dirents(bad.data(),bad.size(),out);
    }
    puts("PASS bounded SFO parsing and malformed inputs");
}
static void discovery_test() {
    FakeFS fs;
    fs.dirs["/user/app"]={{"CUSA12345",4},{"CUSA12345-extra",4},{"CUSA99999",8},{"BREW13533",4}};
    fs.dirs["/mnt/ext1/user/app"]={{"CUSA54321",10}};
    fs.dirs["/user/appmeta"]={{"CUSA12345",4},{"CUSA11111",4}};
    fs.files["/user/app/CUSA12345/app.pkg"]={1};
    fs.files["/user/appmeta/CUSA12345/param.sfo"]=sfo("CUSA12345","Primeiro jogo");
    fs.files["/mnt/ext1/user/app/CUSA54321/app.pkg"]={1};
    tu::Library lib(fs); lib.begin_scan();
    assert(lib.busy() && lib.games.empty());
    assert(lib.scan_step() && lib.games.size() == 1); // A single root per tick.
    while (lib.scan_step()) {}
    assert(lib.games.size() == 3 && lib.roots.size() == 6);
    assert(!lib.games[0].package && lib.games[0].metadata);
    assert(lib.games[1].package && lib.games[1].title == "Primeiro jogo");
    assert(lib.games[2].package && lib.games[2].path == "/mnt/ext1/user/app/CUSA54321");
    assert(lib.roots[1].status == -ENOENT);
    puts("PASS internal/external/appmeta discovery and deduplication");
}
static void manual_test() {
    FakeFS fs; tu::Library lib(fs); lib.scan();
    assert(lib.games.empty());
    int index=lib.add_id("CUSA54321"); assert(index == 0);
    assert(!lib.games[0].package && !lib.games[0].metadata && lib.games[0].manual);
    assert(lib.add_id("CUSA54321") == 0 && lib.games.size() == 1);
    assert(lib.add_id("../CUSA12") < 0);
    fs.dirs["/mnt/usb0/meu-jogo"]={};
    fs.files["/mnt/usb0/meu-jogo/sce_sys/param.sfo"]=sfo("CUSA12345","Pasta manual");
    index=lib.add_folder("/mnt/usb0/meu-jogo"); assert(index == 1);
    assert(lib.games[index].metadata && !lib.games[index].package);
    assert(lib.add_folder("/nao-existe") == -ENOENT);
    fs.dirs["/mnt/usb0/sem-sfo"]={}; assert(lib.add_folder("/mnt/usb0/sem-sfo") == -EINVAL);
    fs.dirs["/mnt/usb0/CUSA00001"]={};
    fs.files["/mnt/usb0/CUSA00001/param.sfo"]=sfo("CUSA00002","Divergencia");
    assert(lib.add_folder("/mnt/usb0/CUSA00001") == -EINVAL);
    // Explicit folder selection must win over an unverified automatic directory.
    fs.dirs["/user/app"]={{"CUSA12345",4}};
    lib.scan();
    index=lib.add_folder("/mnt/usb0/meu-jogo");
    assert(lib.games[index].path == "/mnt/usb0/meu-jogo");
    assert(lib.save_manual() == 0);
    tu::Library reopened(fs); assert(reopened.load_manual() == 0); reopened.scan();
    assert(reopened.games.size() == 2 && reopened.games[0].title == "Pasta manual");
    fs.files.erase("/mnt/usb0/meu-jogo/sce_sys/param.sfo");
    reopened.scan(); assert(!reopened.games[0].metadata); // No stale verification after USB removed.
    fs.save_error=-EACCES; assert(reopened.save_manual() == -EACCES);
    assert(reopened.games.size() == 2); // Still usable for this session.
    fs.files["/data/TrophyUnlocker1352/manual-games.txt"]={'x'};
    assert(reopened.load_manual() == -EINVAL); reopened.scan(); assert(reopened.games.size() == 2);
    puts("PASS manual CUSA/folder, persistence, denied writes and stale access");
}
static void png_test() {
    std::vector<uint8_t> b(33,0); const uint8_t sig[]={137,80,78,71,13,10,26,10};
    memcpy(b.data(),sig,8); b[11]=13; memcpy(b.data()+12,"IHDR",4); b[18]=2; b[22]=2;
    assert(tu::valid_png(b)); b[16]=1; assert(!tu::valid_png(b));
    puts("PASS cover dimensions before image decode");
}
int main() { wire_test(); sfo_test(); discovery_test(); manual_test(); png_test(); }
