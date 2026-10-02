#pragma once
#include "../library.h"
#include <map>
#include <cerrno>
#include <cstring>
struct FakeFS : tu::FileSystem {
    std::map<std::string,std::vector<tu::DirEntry>> dirs;
    std::map<std::string,std::vector<uint8_t>> files;
    int save_error=0;
    int list(const std::string& path,std::vector<tu::DirEntry>& out) override {
        out.clear(); auto i=dirs.find(path); if (i == dirs.end()) return -ENOENT;
        out=i->second; return 0;
    }
    int read(const std::string& path,size_t limit,std::vector<uint8_t>& out) override {
        out.clear(); auto i=files.find(path); if (i == files.end()) return -ENOENT;
        if (i->second.size() > limit) return -EFBIG;
        out=i->second; return 0;
    }
    bool exists(const std::string& path) override { return files.count(path) || dirs.count(path); }
    int save(const std::string& path,const std::string& value) override {
        if (save_error) return save_error;
        files[path]=std::vector<uint8_t>(value.begin(),value.end()); return 0;
    }
};
static inline void put16(std::vector<uint8_t>& b,size_t at,uint16_t n) {
    b[at]=uint8_t(n); b[at+1]=uint8_t(n>>8);
}
static inline void put32(std::vector<uint8_t>& b,size_t at,uint32_t n) {
    for (size_t i=0; i<4; ++i) b[at+i]=uint8_t(n>>(8*i));
}
static inline std::vector<uint8_t> sfo(const std::string& id,const std::string& title) {
    std::vector<uint8_t> b(68+16+title.size()+1,0);
    put32(b,0,0x46535000); put32(b,4,0x101); put32(b,8,52); put32(b,12,68); put32(b,16,2);
    memcpy(b.data()+52,"TITLE_ID\0TITLE\0",15);
    put16(b,20,0); put16(b,22,0x204); put32(b,24,id.size()+1); put32(b,28,16); put32(b,32,0);
    memcpy(b.data()+68,id.c_str(),id.size()+1);
    put16(b,36,9); put16(b,38,0x204); put32(b,40,title.size()+1); put32(b,44,title.size()+1); put32(b,48,16);
    memcpy(b.data()+84,title.c_str(),title.size()+1); return b;
}
