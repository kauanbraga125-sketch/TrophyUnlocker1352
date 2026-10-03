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
    puts("PASS native dirent boundaries and paths");
}
static void sfo_test() {
    auto bytes=sfo("CUSA12345","Ação e aventura"); std::string id,title;
    assert(tu::parse_sfo(bytes,id,title) && id == "CUSA12345" && title == "Ação e aventura");
    auto bad=bytes; put32(bad,8,0xfffffff0);
    assert(!tu::parse_sfo(bad,id,title) && id.empty() && title.empty());
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
static void installed_only_test() {
    FakeFS fs;
    fs.dirs["/user/app"]={{"CUSA12345",4},{"CUSA99999",8},{"BREW13533",4}};
    fs.dirs["/mnt/ext1/user/app"]={{"CUSA54321",10}};
    // This stale appmeta entry MUST NOT become a game in V13.9.
    fs.dirs["/user/appmeta"]={{"CUSA11111",4},{"CUSA12345",4}};
    fs.files["/user/appmeta/CUSA11111/param.sfo"]=sfo("CUSA11111","Desinstalado");
    fs.files["/user/appmeta/CUSA12345/param.sfo"]=sfo("CUSA12345","Primeiro jogo");
    fs.files["/user/appmeta/CUSA12345/icon0.png"]={137,80,78,71,13,10,26,10};

    tu::Library lib(fs); lib.begin_scan();
    while (lib.scan_step()) {}
    assert(!lib.busy());
    assert(lib.roots.size() == 3);
    assert(lib.games.size() == 2);
    assert(lib.games[0].id == "CUSA12345");
    assert(lib.games[0].package && lib.games[0].title == "Primeiro jogo");
    assert(lib.games[1].id == "CUSA54321");
    assert(lib.games[1].package && lib.games[1].path == "/mnt/ext1/user/app/CUSA54321");
    for (const auto& g: lib.games) assert(g.id != "CUSA11111");
    puts("PASS V13.9 installed-only discovery; stale appmeta excluded");
}
static void manual_exclusion_test() {
    FakeFS fs; tu::Library lib(fs); lib.scan();
    assert(lib.games.empty());
    assert(lib.add_id("CUSA54321") == -ENOENT);
    fs.dirs["/mnt/usb0/CUSA54321"]={};
    assert(lib.add_folder("/mnt/usb0/CUSA54321") == -ENOENT);
    assert(lib.load_manual() == 0);
    assert(lib.save_manual() == 0);
    assert(lib.games.empty());
    puts("PASS manual/uninstalled entries excluded from main library");
}
static void png_test() {
    std::vector<uint8_t> b(33,0); const uint8_t sig[]={137,80,78,71,13,10,26,10};
    memcpy(b.data(),sig,8); b[11]=13; memcpy(b.data()+12,"IHDR",4); b[18]=2; b[22]=2;
    assert(tu::valid_png(b)); b[16]=1; assert(!tu::valid_png(b));
    puts("PASS cover dimensions before image decode");
}
int main() { wire_test(); sfo_test(); installed_only_test(); manual_exclusion_test(); png_test(); }
