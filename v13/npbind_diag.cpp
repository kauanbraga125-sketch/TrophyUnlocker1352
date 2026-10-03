#include "npbind_diag.h"
#include "library.h"
#include "filesystem.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <set>

namespace tu {
namespace {
static uint16_t rd16be(const uint8_t* p){return (uint16_t(p[0])<<8)|p[1];}
static uint16_t rd16le(const uint8_t* p){return uint16_t(p[0])|(uint16_t(p[1])<<8);}
static uint64_t rd64be(const uint8_t* p){uint64_t v=0;for(int i=0;i<8;++i)v=(v<<8)|p[i];return v;}
static uint64_t rd64le(const uint8_t* p){uint64_t v=0;for(int i=7;i>=0;--i)v=(v<<8)|p[i];return v;}
static std::string hex_bytes(const uint8_t* p,size_t n){
    char b[4]; std::string out;
    for(size_t i=0;i<n;++i){snprintf(b,sizeof(b),"%02X",unsigned(p[i]));if(i)out+=' ';out+=b;}
    return out;
}
static bool npwr_string(const uint8_t* p,size_t n,std::string& out){
    if(n<12||memcmp(p,"NPWR",4))return false;
    std::string s(reinterpret_cast<const char*>(p),12);
    if(s[9]!='_')return false;
    for(size_t i=4;i<9;++i)if(s[i]<'0'||s[i]>'9')return false;
    for(size_t i=10;i<12;++i)if(s[i]<'0'||s[i]>'9')return false;
    out=s;return true;
}
static bool plausible(uint64_t file_size,uint64_t entry_size,uint64_t count,size_t actual){
    return file_size>=0x80 && file_size<=actual && entry_size>=0x20 && entry_size<=0x10000 && count<=256 && 0x80+entry_size*count<=actual;
}
}

NpbindDiagnostic inspect_npbind_for_game(const Game& game, FileSystem& fs){
    NpbindDiagnostic d;
    std::vector<std::string> roots={
        "/system_data/priv/appmeta/"+game.id,
        "/user/appmeta/"+game.id,
        "/user/appmeta/external/"+game.id
    };
    if(!game.path.empty()) roots.push_back(game.path+"/sce_sys");
    std::vector<uint8_t> bytes;
    for(const auto& root:roots){
        std::string p=root+"/npbind.dat";
        if(!fs.read(p,4*1024*1024,bytes)){d.path=p;break;}
    }
    if(d.path.empty()){d.status=-2;d.lines.push_back("npbind.dat nao encontrado nos caminhos conhecidos.");return d;}
    char line[256];
    snprintf(line,sizeof(line),"Arquivo: %s",d.path.c_str());d.lines.push_back(line);
    snprintf(line,sizeof(line),"Tamanho real: %zu bytes",bytes.size());d.lines.push_back(line);
    if(bytes.size()<0x80){d.status=-5;d.lines.push_back("Arquivo pequeno demais para cabecalho npbind.");return d;}
    snprintf(line,sizeof(line),"Magic: %02X %02X %02X %02X",bytes[0],bytes[1],bytes[2],bytes[3]);d.lines.push_back(line);

    uint64_t fs_be=rd64be(bytes.data()+8), es_be=rd64be(bytes.data()+0x10), ct_be=rd64be(bytes.data()+0x18);
    uint64_t fs_le=rd64le(bytes.data()+8), es_le=rd64le(bytes.data()+0x10), ct_le=rd64le(bytes.data()+0x18);
    bool be=plausible(fs_be,es_be,ct_be,bytes.size());
    bool le=plausible(fs_le,es_le,ct_le,bytes.size());
    bool use_be=be||!le;
    uint64_t file_size=use_be?fs_be:fs_le, entry_size=use_be?es_be:es_le, count=use_be?ct_be:ct_le;
    snprintf(line,sizeof(line),"Cabecalho %s: file=%llu entry=%llu count=%llu",use_be?"BE":"LE",(unsigned long long)file_size,(unsigned long long)entry_size,(unsigned long long)count);d.lines.push_back(line);

    std::set<std::string> ascii_npwr;
    for(size_t i=0;i+12<=bytes.size();++i){std::string s;if(npwr_string(bytes.data()+i,bytes.size()-i,s))ascii_npwr.insert(s);}
    if(ascii_npwr.empty()) d.lines.push_back("NPWR ASCII: nenhum localizado");
    else for(const auto& s:ascii_npwr)d.lines.push_back("NPWR ASCII: "+s);

    if(!(be||le)){d.status=-5;d.lines.push_back("Cabecalho nao passou na verificacao de tamanho; sem parse de entradas.");return d;}
    size_t max_entries=std::min<uint64_t>(count,16);
    for(size_t ei=0;ei<max_entries;++ei){
        size_t base=0x80+size_t(entry_size)*ei;
        if(base+entry_size>bytes.size())break;
        snprintf(line,sizeof(line),"Entrada %zu:",ei);d.lines.push_back(line);
        size_t pos=base;
        for(int block=0;block<4 && pos+4<=base+entry_size;++block){
            uint16_t type_be=rd16be(bytes.data()+pos), size_be=rd16be(bytes.data()+pos+2);
            uint16_t type_le=rd16le(bytes.data()+pos), size_le=rd16le(bytes.data()+pos+2);
            bool bbe=type_be>=0x10&&type_be<=0x13&&size_be<=entry_size-4;
            bool ble=type_le>=0x10&&type_le<=0x13&&size_le<=entry_size-4;
            uint16_t type=bbe?type_be:type_le;
            uint16_t sz=bbe?size_be:size_le;
            if(!(bbe||ble)||pos+4+sz>base+entry_size){
                snprintf(line,sizeof(line),"  bloco %d invalido @0x%zX raw=%s",block,pos,hex_bytes(bytes.data()+pos,std::min<size_t>(8,base+entry_size-pos)).c_str());d.lines.push_back(line);break;
            }
            const uint8_t* data=bytes.data()+pos+4;
            if(type==0x10){
                std::string id; if(npwr_string(data,sz,id)) snprintf(line,sizeof(line),"  NP Communication ID: %s",id.c_str());
                else snprintf(line,sizeof(line),"  NP Communication ID raw: %s",hex_bytes(data,std::min<size_t>(sz,16)).c_str());
            } else if(type==0x11){
                uint64_t vbe=0,vle=0;size_t take=std::min<size_t>(sz,8);
                for(size_t k=0;k<take;++k){vbe=(vbe<<8)|data[k];vle|=uint64_t(data[k])<<(8*k);} 
                snprintf(line,sizeof(line),"  NP Service Label: BE=%llu LE=%llu raw=%s",(unsigned long long)vbe,(unsigned long long)vle,hex_bytes(data,take).c_str());
            } else if(type==0x12){
                snprintf(line,sizeof(line),"  NP Comm Signature: presente (%u bytes)",unsigned(sz));
            } else {
                snprintf(line,sizeof(line),"  IV: presente (%u bytes)",unsigned(sz));
            }
            d.lines.push_back(line);
            pos+=4+sz;
        }
    }
    d.status=0;
    return d;
}
}
