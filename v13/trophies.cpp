#include "trophies.h"
#include "library.h"
#include "filesystem.h"
#include "goldhen_access.h"
#include <sqlite3.h>
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>

#ifndef TU_HOST_TEST
#include <orbis/UserService.h>
#include <orbis/libkernel.h>
#include <fcntl.h>
#endif

namespace tu {

static uint16_t rd16(const uint8_t* p) { return uint16_t(p[0]) | (uint16_t(p[1]) << 8); }
static uint32_t rd32(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

bool valid_np_communication_id(const std::string& value) {
    if (value.size() != 12 || value.compare(0, 4, "NPWR") || value[9] != '_') return false;
    for (size_t i = 4; i < 9; ++i) if (value[i] < '0' || value[i] > '9') return false;
    for (size_t i = 10; i < 12; ++i) if (value[i] < '0' || value[i] > '9') return false;
    return true;
}

const char* trophy_grade_name(int grade) {
    switch (grade) {
        case 1: return "Platina";
        case 2: return "Ouro";
        case 3: return "Prata";
        case 4: return "Bronze";
        default: return "Desconhecido";
    }
}

static bool parse_np_from_sfo(const std::vector<uint8_t>& b, std::string& np) {
    np.clear();
    if (b.size() < 20 || rd32(b.data()) != 0x46535000) return false;
    uint32_t keys = rd32(b.data()+8), data = rd32(b.data()+12), count = rd32(b.data()+16);
    if (count > 512 || 20ull + 16ull*count > b.size() || keys < 20ull + 16ull*count || keys >= data || data > b.size()) return false;
    for (uint32_t i=0; i<count; ++i) {
        const uint8_t* e = b.data()+20+16*i;
        uint64_t key = uint64_t(keys)+rd16(e), val = uint64_t(data)+rd32(e+12);
        uint32_t len = rd32(e+4), maxlen = rd32(e+8);
        if (key >= data || val > b.size() || len > maxlen || maxlen > b.size()-val) return false;
        const char* k = reinterpret_cast<const char*>(b.data()+key);
        const char* kend = static_cast<const char*>(memchr(k, 0, data-key));
        if (!kend) return false;
        if (strcmp(k, "NP_COMMUNICATION_ID")) continue;
        if (rd16(e+2) != 0x0204 || len < 1 || len > 64) return false;
        const char* value = reinterpret_cast<const char*>(b.data()+val);
        const char* stop = static_cast<const char*>(memchr(value, 0, len));
        if (!stop) return false;
        np.assign(value, size_t(stop-value));
        return valid_np_communication_id(np);
    }
    return false;
}

static std::string find_np_communication_id(FileSystem& fs, const Game& game) {
    std::vector<std::string> bases;
    if (!game.path.empty()) bases.push_back(game.path);
    bases.push_back("/user/app/"+game.id);
    bases.push_back("/mnt/ext0/user/app/"+game.id);
    bases.push_back("/mnt/ext1/user/app/"+game.id);
    bases.push_back("/user/appmeta/"+game.id);
    bases.push_back("/user/appmeta/external/"+game.id);
    bases.push_back("/system_data/priv/appmeta/"+game.id);
    for (const std::string& base : bases) {
        for (const char* suffix : {"/param.sfo", "/sce_sys/param.sfo"}) {
            std::vector<uint8_t> bytes;
            if (fs.read(base+suffix, 65536, bytes)) continue;
            std::string np;
            if (parse_np_from_sfo(bytes, np)) return np;
        }
    }
    return "";
}

static std::string coltext(sqlite3_stmt* stmt, int col) {
    const unsigned char* text = sqlite3_column_text(stmt, col);
    return text ? reinterpret_cast<const char*>(text) : "";
}

static int find_set_by_np(sqlite3* db, const std::string& np, TrophySet& out, std::string& detail) {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT id, trophy_title_id, title FROM tbl_trophy_title WHERE status = 0 AND trophy_title_id = ? LIMIT 1";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) { detail = sqlite3_errmsg(db); return rc; }
    sqlite3_bind_text(stmt, 1, np.c_str(), -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        out.database_id = sqlite3_column_int64(stmt, 0);
        out.np_communication_id = coltext(stmt, 1);
        out.title = coltext(stmt, 2);
        sqlite3_finalize(stmt);
        return SQLITE_OK;
    }
    sqlite3_finalize(stmt);
    detail = rc == SQLITE_DONE ? "Conjunto de trofeus ainda nao registrado neste usuario." : sqlite3_errmsg(db);
    return rc == SQLITE_DONE ? SQLITE_NOTFOUND : rc;
}

static int find_set_by_title(sqlite3* db, const std::string& title, TrophySet& out, std::string& detail) {
    if (title.empty()) return SQLITE_NOTFOUND;
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT id, trophy_title_id, title FROM tbl_trophy_title WHERE status = 0 AND title = ? LIMIT 2";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) { detail = sqlite3_errmsg(db); return rc; }
    sqlite3_bind_text(stmt, 1, title.c_str(), -1, SQLITE_TRANSIENT);
    int count = 0;
    TrophySet candidate;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        ++count;
        candidate.database_id = sqlite3_column_int64(stmt,0);
        candidate.np_communication_id = coltext(stmt,1);
        candidate.title = coltext(stmt,2);
    }
    sqlite3_finalize(stmt);
    if (count == 1) { out = candidate; return SQLITE_OK; }
    detail = count > 1 ? "Mais de um conjunto possui esse titulo; NPWR necessario." : "Jogo nao encontrado no banco local de trofeus.";
    return SQLITE_NOTFOUND;
}

static int load_flags(sqlite3* db, TrophySet& out, std::string& detail) {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT trophyid, groupid, title, description, grade, unlocked FROM tbl_trophy_flag WHERE title_id = ? ORDER BY trophyid";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) { detail = sqlite3_errmsg(db); return rc; }
    sqlite3_bind_int64(stmt, 1, out.database_id);
    out.trophies.clear();
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (out.trophies.size() >= 512) { sqlite3_finalize(stmt); detail="Lista de trofeus excede o limite de seguranca."; return SQLITE_TOOBIG; }
        Trophy t;
        t.id = sqlite3_column_int(stmt,0);
        t.group = sqlite3_column_int(stmt,1);
        t.title = coltext(stmt,2);
        t.description = coltext(stmt,3);
        t.grade = sqlite3_column_int(stmt,4);
        t.unlocked = sqlite3_column_int(stmt,5) != 0;
        out.trophies.push_back(t);
    }
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) { detail=sqlite3_errmsg(db); return rc; }
    if (out.trophies.empty()) { detail="Conjunto encontrado, mas sem trofeus registrados."; return SQLITE_NOTFOUND; }
    return SQLITE_OK;
}

static int query_db(sqlite3* db, const std::string& np, const std::string& fallback_title, TrophySet& out, std::string& detail) {
    out = TrophySet();
    int rc = valid_np_communication_id(np) ? find_set_by_np(db,np,out,detail) : SQLITE_NOTFOUND;
    if (rc != SQLITE_OK && !fallback_title.empty()) rc = find_set_by_title(db,fallback_title,out,detail);
    if (rc != SQLITE_OK) return rc;
    return load_flags(db,out,detail);
}

int query_trophy_database(const std::string& database_path,
                          const std::string& np_communication_id,
                          TrophySet& out,
                          std::string& detail) {
    sqlite3* db = nullptr;
    int rc = sqlite3_open_v2(database_path.c_str(), &db, SQLITE_OPEN_READONLY, nullptr);
    if (rc != SQLITE_OK) {
        detail = db ? sqlite3_errmsg(db) : "sqlite3_open_v2 falhou";
        if (db) sqlite3_close(db);
        return rc;
    }
    rc = query_db(db,np_communication_id,"",out,detail);
    sqlite3_close(db);
    return rc;
}

#ifndef TU_HOST_TEST
// The PS4 SQLite VFS is not reliable for querying trophy_local.db directly.
// Apollo solves the same problem by first copying the database into memory.
// We do the same thing here using SQLite's deserialize API. The original file
// is never opened for writing and the in-memory copy is never written back.
static int open_trophy_snapshot(const std::string& path,
                                sqlite3** out_db,
                                unsigned char** out_buffer,
                                long long& out_size,
                                std::string& detail) {
    *out_db = nullptr;
    *out_buffer = nullptr;
    out_size = 0;

    FILE* fp = fopen(path.c_str(), "rb");
    if (!fp) {
        detail = "Nao foi possivel abrir trophy_local.db para leitura.";
        return SQLITE_CANTOPEN;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        detail = "Falha ao medir trophy_local.db.";
        return SQLITE_IOERR;
    }
    long size = ftell(fp);
    if (size <= 100 || size > 64L*1024L*1024L) {
        fclose(fp);
        char line[128];
        snprintf(line,sizeof(line),"Tamanho inesperado do banco: %ld bytes.",size);
        detail = line;
        return SQLITE_NOTADB;
    }
    rewind(fp);

    unsigned char* buffer = static_cast<unsigned char*>(sqlite3_malloc64(static_cast<sqlite3_uint64>(size)));
    if (!buffer) {
        fclose(fp);
        detail = "Sem memoria para copiar trophy_local.db.";
        return SQLITE_NOMEM;
    }
    size_t got = fread(buffer, 1, static_cast<size_t>(size), fp);
    fclose(fp);
    if (got != static_cast<size_t>(size)) {
        sqlite3_free(buffer);
        char line[160];
        snprintf(line,sizeof(line),"Leitura incompleta do banco: %zu de %ld bytes.",got,size);
        detail = line;
        return SQLITE_IOERR_READ;
    }
    if (size < 16 || memcmp(buffer, "SQLite format 3\000", 16) != 0) {
        sqlite3_free(buffer);
        char line[160];
        snprintf(line,sizeof(line),"Cabecalho do banco nao e SQLite valido (%ld bytes lidos).",size);
        detail = line;
        return SQLITE_NOTADB;
    }

    sqlite3* db = nullptr;
    int rc = sqlite3_open(":memory:", &db);
    if (rc != SQLITE_OK) {
        sqlite3_free(buffer);
        detail = db ? sqlite3_errmsg(db) : "Falha ao criar banco em memoria.";
        if (db) sqlite3_close(db);
        return rc;
    }

    rc = sqlite3_deserialize(db, "main", buffer,
                             static_cast<sqlite3_int64>(size),
                             static_cast<sqlite3_int64>(size),
                             SQLITE_DESERIALIZE_READONLY);
    if (rc != SQLITE_OK) {
        detail = sqlite3_errmsg(db);
        sqlite3_close(db);
        sqlite3_free(buffer);
        return rc;
    }

    *out_db = db;
    *out_buffer = buffer;
    out_size = size;
    char line[160];
    snprintf(line,sizeof(line),"Snapshot SQLite OK: %ld bytes.",size);
    detail = line;
    return SQLITE_OK;
}

static void close_trophy_snapshot(sqlite3* db, unsigned char* buffer) {
    if (db) sqlite3_close(db);
    if (buffer) sqlite3_free(buffer);
}
#endif

static TrophyLoadResult load_for_game_internal(const Game& game, FileSystem& fs) {
    TrophyLoadResult result;
#ifndef TU_HOST_TEST
    int32_t user = 0;
    result.user_status = sceUserServiceGetInitialUser(&user);
    if (result.user_status < 0) {
        result.status = result.user_status;
        result.detail = "Nao foi possivel identificar o usuario ativo.";
        return result;
    }
    result.user_id = uint32_t(user);
    char dbpath[128];
    snprintf(dbpath,sizeof(dbpath),"/user/home/%08x/trophy/db/trophy_local.db",unsigned(result.user_id));
    result.database_path = dbpath;

    std::string np = find_np_communication_id(fs,game);
    sqlite3* db = nullptr;
    unsigned char* backing = nullptr;
    long long snapshot_size = 0;
    std::string snapshot_detail;
    int rc = open_trophy_snapshot(result.database_path,&db,&backing,snapshot_size,snapshot_detail);
    if (rc != SQLITE_OK) {
        result.sqlite_status = rc;
        result.status = -EACCES;
        result.detail = snapshot_detail;
        return result;
    }

    std::string query_detail;
    result.sqlite_status = query_db(db,np,game.title,result.set,query_detail);
    close_trophy_snapshot(db,backing);
    if (result.sqlite_status == SQLITE_OK) {
        result.status = 0;
        result.detail = snapshot_detail + (np.empty() ? " NPWR resolvido pelo titulo local." : " Trofeus carregados em modo somente leitura.");
    } else {
        result.status = -ENOENT;
        result.detail = snapshot_detail + " " + query_detail;
    }
#else
    result.detail = "TU_HOST_TEST";
#endif
    return result;
}

TrophyLoadResult load_trophies(const std::string& np_communication_id) {
    TrophyLoadResult result;
#ifndef TU_HOST_TEST
    int32_t user = 0;
    result.user_status = sceUserServiceGetInitialUser(&user);
    if (result.user_status < 0) {
        result.status=result.user_status;
        result.detail="Nao foi possivel identificar o usuario ativo.";
        return result;
    }
    result.user_id = uint32_t(user);
    char dbpath[128];
    snprintf(dbpath,sizeof(dbpath),"/user/home/%08x/trophy/db/trophy_local.db",unsigned(result.user_id));
    result.database_path=dbpath;

    sqlite3* db = nullptr;
    unsigned char* backing = nullptr;
    long long snapshot_size = 0;
    std::string snapshot_detail;
    int rc = open_trophy_snapshot(result.database_path,&db,&backing,snapshot_size,snapshot_detail);
    if (rc != SQLITE_OK) {
        result.sqlite_status=rc;
        result.status=-EACCES;
        result.detail=snapshot_detail;
        return result;
    }
    std::string query_detail;
    result.sqlite_status=query_db(db,np_communication_id,"",result.set,query_detail);
    close_trophy_snapshot(db,backing);
    result.status=result.sqlite_status == SQLITE_OK ? 0 : -ENOENT;
    result.detail=result.sqlite_status == SQLITE_OK ? snapshot_detail + " Trofeus carregados em modo somente leitura." : snapshot_detail + " " + query_detail;
#endif
    return result;
}


#ifndef TU_HOST_TEST
static int trophy_native_error(int64_t result) {
    uint32_t bits = uint32_t(result);
    if ((bits & 0xffff0000u) == 0x80020000u) return -int(bits & 0xffffu);
    return result < 0 ? int(result) : 0;
}

static int read_trophy_db_bytes(const std::string& path,
                                std::vector<uint8_t>& out,
                                std::string& detail) {
    out.clear();
    FILE* fp = fopen(path.c_str(), "rb");
    if (!fp) {
        detail = "Nao foi possivel abrir trophy_local.db para backup.";
        return errno ? -errno : -EIO;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        detail = "Falha ao medir trophy_local.db.";
        return -EIO;
    }
    long size = ftell(fp);
    if (size <= 100 || size > 64L*1024L*1024L) {
        fclose(fp);
        detail = "Tamanho inesperado de trophy_local.db.";
        return -EFBIG;
    }
    rewind(fp);
    out.resize(static_cast<size_t>(size));
    size_t got = fread(out.data(),1,out.size(),fp);
    fclose(fp);
    if (got != out.size()) {
        out.clear();
        detail = "Leitura incompleta de trophy_local.db.";
        return -EIO;
    }
    if (out.size() < 16 || memcmp(out.data(),"SQLite format 3\000",16) != 0) {
        out.clear();
        detail = "trophy_local.db nao possui cabecalho SQLite valido.";
        return -EINVAL;
    }
    return 0;
}

static int write_bytes_atomic(const std::string& path,
                              const uint8_t* data,
                              size_t size,
                              std::string& detail) {
    if (!data || !size) return -EINVAL;
    const std::string temp = path + ".tu-v1319.tmp";
    sceKernelUnlink(temp.c_str());
    int fd = sceKernelOpen(temp.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) {
        int st=trophy_native_error(fd);
        detail="Nao foi possivel criar arquivo temporario ("+std::to_string(st)+").";
        return st;
    }
    size_t pos=0;
    int status=0;
    while (pos<size) {
        int64_t n=sceKernelWrite(fd,data+pos,size-pos);
        int err=trophy_native_error(n);
        if (err == -EINTR) continue;
        if (err) { status=err; break; }
        if (n <= 0 || uint64_t(n) > size-pos) { status=-EIO; break; }
        pos += static_cast<size_t>(n);
    }
    if (!status) status=trophy_native_error(sceKernelFsync(fd));
    int closed=trophy_native_error(sceKernelClose(fd));
    if (!status) status=closed;
    if (!status) status=trophy_native_error(sceKernelRename(temp.c_str(),path.c_str()));
    if (status) {
        sceKernelUnlink(temp.c_str());
        detail="Falha ao gravar arquivo com seguranca ("+std::to_string(status)+").";
    }
    return status;
}

static bool file_exists_native(const std::string& path) {
    int fd=sceKernelOpen(path.c_str(),O_RDONLY,0);
    if (fd < 0) return false;
    sceKernelClose(fd);
    return true;
}

static std::string next_trophy_backup(uint32_t user_id,
                                      long long title_id,
                                      int trophy_id) {
    int mk=trophy_native_error(sceKernelMkdir("/data/TrophyUnlocker1352",0755));
    if (mk && mk != -EEXIST) return "";
    char path[256];
    for (int i=1;i<=99;++i) {
        snprintf(path,sizeof(path),
                 "/data/TrophyUnlocker1352/trophy_local_%08x_set%lld_t%03d_%02d.bak",
                 unsigned(user_id),title_id,trophy_id,i);
        if (!file_exists_native(path)) return path;
    }
    return "";
}

static int scalar_count(sqlite3* db,
                        const char* sql,
                        long long title_id,
                        int group_id,
                        int grade,
                        int& value) {
    sqlite3_stmt* stmt=nullptr;
    int rc=sqlite3_prepare_v2(db,sql,-1,&stmt,nullptr);
    if (rc != SQLITE_OK) return rc;
    int bind=1;
    sqlite3_bind_int64(stmt,bind++,title_id);
    if (strstr(sql,"groupid = ?")) sqlite3_bind_int(stmt,bind++,group_id);
    if (strstr(sql,"grade = ?")) sqlite3_bind_int(stmt,bind++,grade);
    rc=sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        value=sqlite3_column_int(stmt,0);
        rc=SQLITE_OK;
    }
    sqlite3_finalize(stmt);
    return rc;
}

static int recompute_trophy_aggregates(sqlite3* db,
                                       long long title_id,
                                       int group_id,
                                       std::string& detail) {
    int total_title=0, total_group=0;
    int title_grade[5]={0,0,0,0,0};
    int group_grade[5]={0,0,0,0,0};
    int rc=scalar_count(db,
        "SELECT COUNT(*) FROM tbl_trophy_flag WHERE title_id = ? AND unlocked <> 0",
        title_id,group_id,0,total_title);
    if (rc != SQLITE_OK) { detail=sqlite3_errmsg(db); return rc; }
    rc=scalar_count(db,
        "SELECT COUNT(*) FROM tbl_trophy_flag WHERE title_id = ? AND groupid = ? AND unlocked <> 0",
        title_id,group_id,0,total_group);
    if (rc != SQLITE_OK) { detail=sqlite3_errmsg(db); return rc; }
    for (int grade=1;grade<=4;++grade) {
        rc=scalar_count(db,
            "SELECT COUNT(*) FROM tbl_trophy_flag WHERE title_id = ? AND unlocked <> 0 AND grade = ?",
            title_id,group_id,grade,title_grade[grade]);
        if (rc != SQLITE_OK) { detail=sqlite3_errmsg(db); return rc; }
        rc=scalar_count(db,
            "SELECT COUNT(*) FROM tbl_trophy_flag WHERE title_id = ? AND groupid = ? AND unlocked <> 0 AND grade = ?",
            title_id,group_id,grade,group_grade[grade]);
        if (rc != SQLITE_OK) { detail=sqlite3_errmsg(db); return rc; }
    }

    sqlite3_stmt* title=nullptr;
    rc=sqlite3_prepare_v2(db,
        "UPDATE tbl_trophy_title SET "
        "progress=CASE WHEN trophy_num>0 THEN (?*100)/trophy_num ELSE 0 END,"
        "unlocked_trophy_num=?,"
        "unlocked_platinum_num=?,unlocked_gold_num=?,unlocked_silver_num=?,unlocked_bronze_num=? "
        "WHERE id=?",
        -1,&title,nullptr);
    if (rc != SQLITE_OK) { detail=sqlite3_errmsg(db); return rc; }
    sqlite3_bind_int(title,1,total_title);
    sqlite3_bind_int(title,2,total_title);
    sqlite3_bind_int(title,3,title_grade[1]);
    sqlite3_bind_int(title,4,title_grade[2]);
    sqlite3_bind_int(title,5,title_grade[3]);
    sqlite3_bind_int(title,6,title_grade[4]);
    sqlite3_bind_int64(title,7,title_id);
    rc=sqlite3_step(title);
    sqlite3_finalize(title);
    if (rc != SQLITE_DONE) { detail=sqlite3_errmsg(db); return rc; }

    sqlite3_stmt* group=nullptr;
    rc=sqlite3_prepare_v2(db,
        "UPDATE tbl_trophy_group SET "
        "progress=CASE WHEN trophy_num>0 THEN (?*100)/trophy_num ELSE 0 END,"
        "unlocked_trophy_num=?,"
        "unlocked_platinum_num=?,unlocked_gold_num=?,unlocked_silver_num=?,unlocked_bronze_num=? "
        "WHERE title_id=? AND groupid=?",
        -1,&group,nullptr);
    if (rc != SQLITE_OK) { detail=sqlite3_errmsg(db); return rc; }
    sqlite3_bind_int(group,1,total_group);
    sqlite3_bind_int(group,2,total_group);
    sqlite3_bind_int(group,3,group_grade[1]);
    sqlite3_bind_int(group,4,group_grade[2]);
    sqlite3_bind_int(group,5,group_grade[3]);
    sqlite3_bind_int(group,6,group_grade[4]);
    sqlite3_bind_int64(group,7,title_id);
    sqlite3_bind_int(group,8,group_id);
    rc=sqlite3_step(group);
    sqlite3_finalize(group);
    if (rc != SQLITE_DONE) { detail=sqlite3_errmsg(db); return rc; }
    return SQLITE_OK;
}
#endif

TrophyVisualRevertResult revert_visual_trophy(long long title_database_id, int trophy_id) {
    TrophyVisualRevertResult result;
#ifdef TU_HOST_TEST
    (void)title_database_id;
    (void)trophy_id;
    result.status=-ENOTSUP;
    result.detail="Reversao visual so existe no PS4.";
    return result;
#else
    if (title_database_id < 0 || trophy_id < 0 || trophy_id > 255) {
        result.status=-EINVAL;
        result.detail="Conjunto ou ID de trofeu invalido.";
        return result;
    }

    AccessResult access=request_goldhen_access();
    if (!access.acknowledged()) {
        result.status=-EACCES;
        result.detail="GoldHEN nao confirmou acesso para editar o estado visual local.";
        return result;
    }

    int32_t user=0;
    int user_status=sceUserServiceGetInitialUser(&user);
    if (user_status < 0) {
        result.status=user_status;
        result.detail="Nao foi possivel identificar o usuario ativo.";
        return result;
    }

    char dbpath[128];
    snprintf(dbpath,sizeof(dbpath),"/user/home/%08x/trophy/db/trophy_local.db",unsigned(user));

    std::vector<uint8_t> original;
    std::string io_detail;
    int st=read_trophy_db_bytes(dbpath,original,io_detail);
    if (st) {
        result.status=st;
        result.detail=io_detail;
        return result;
    }

    sqlite3* db=nullptr;
    int rc=sqlite3_open(":memory:",&db);
    if (rc != SQLITE_OK) {
        result.status=-EIO;
        result.detail=db ? sqlite3_errmsg(db) : "Falha ao criar banco em memoria.";
        if (db) sqlite3_close(db);
        return result;
    }

    unsigned char* work=static_cast<unsigned char*>(sqlite3_malloc64(original.size()));
    if (!work) {
        sqlite3_close(db);
        result.status=-ENOMEM;
        result.detail="Sem memoria para preparar a reversao visual.";
        return result;
    }
    memcpy(work,original.data(),original.size());
    rc=sqlite3_deserialize(db,"main",work,
                           static_cast<sqlite3_int64>(original.size()),
                           static_cast<sqlite3_int64>(original.size()),0);
    if (rc != SQLITE_OK) {
        result.status=-EIO;
        result.detail=sqlite3_errmsg(db);
        sqlite3_close(db);
        sqlite3_free(work);
        return result;
    }
    sqlite3_exec(db,"PRAGMA journal_mode=OFF; PRAGMA synchronous=OFF;",nullptr,nullptr,nullptr);

    int group_id=0;
    int unlocked=0;
    sqlite3_stmt* check=nullptr;
    rc=sqlite3_prepare_v2(db,
        "SELECT groupid, unlocked FROM tbl_trophy_flag WHERE title_id=? AND trophyid=? LIMIT 1",
        -1,&check,nullptr);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int64(check,1,title_database_id);
        sqlite3_bind_int(check,2,trophy_id);
        rc=sqlite3_step(check);
        if (rc == SQLITE_ROW) {
            group_id=sqlite3_column_int(check,0);
            unlocked=sqlite3_column_int(check,1);
            rc=SQLITE_OK;
        } else if (rc == SQLITE_DONE) {
            rc=SQLITE_NOTFOUND;
        }
    }
    sqlite3_finalize(check);
    if (rc != SQLITE_OK) {
        result.status=-ENOENT;
        result.detail="Trofeu selecionado nao foi encontrado no banco local.";
        sqlite3_close(db);
        sqlite3_free(work);
        return result;
    }
    if (!unlocked) {
        result.status=-EALREADY;
        result.detail="Esse trofeu ja esta marcado como bloqueado.";
        sqlite3_close(db);
        sqlite3_free(work);
        return result;
    }

    result.backup_path=next_trophy_backup(uint32_t(user),title_database_id,trophy_id);
    if (result.backup_path.empty()) {
        result.status=-EIO;
        result.detail="Nao foi possivel reservar um nome para o backup.";
        sqlite3_close(db);
        sqlite3_free(work);
        return result;
    }
    st=write_bytes_atomic(result.backup_path,original.data(),original.size(),io_detail);
    if (st) {
        result.status=st;
        result.detail="Backup obrigatorio falhou. Nenhuma alteracao foi feita. "+io_detail;
        sqlite3_close(db);
        sqlite3_free(work);
        return result;
    }

    char* err=nullptr;
    rc=sqlite3_exec(db,"BEGIN IMMEDIATE;",nullptr,nullptr,&err);
    if (rc != SQLITE_OK) {
        result.status=-EIO;
        result.detail=err ? err : sqlite3_errmsg(db);
        sqlite3_free(err);
        sqlite3_close(db);
        sqlite3_free(work);
        return result;
    }

    sqlite3_stmt* update=nullptr;
    rc=sqlite3_prepare_v2(db,
        "UPDATE tbl_trophy_flag SET "
        "visible=((~(hidden&1))&(hidden|1)),"
        "unlocked=0,"
        "time_unlocked='0001-01-01T00:00:00.00Z',"
        "time_unlocked_uc='0001-01-01T00:00:00.00Z' "
        "WHERE title_id=? AND trophyid=? AND unlocked<>0",
        -1,&update,nullptr);
    if (rc == SQLITE_OK) {
        sqlite3_bind_int64(update,1,title_database_id);
        sqlite3_bind_int(update,2,trophy_id);
        rc=sqlite3_step(update);
    }
    sqlite3_finalize(update);
    if (rc != SQLITE_DONE) {
        sqlite3_exec(db,"ROLLBACK;",nullptr,nullptr,nullptr);
        result.status=-EIO;
        result.detail="Falha ao limpar a marcacao visual: "+std::string(sqlite3_errmsg(db));
        sqlite3_close(db);
        sqlite3_free(work);
        return result;
    }

    std::string aggregate_detail;
    rc=recompute_trophy_aggregates(db,title_database_id,group_id,aggregate_detail);
    if (rc != SQLITE_OK) {
        sqlite3_exec(db,"ROLLBACK;",nullptr,nullptr,nullptr);
        result.status=-EIO;
        result.detail="Falha ao recalcular progresso: "+aggregate_detail;
        sqlite3_close(db);
        sqlite3_free(work);
        return result;
    }

    rc=sqlite3_exec(db,"COMMIT;",nullptr,nullptr,&err);
    if (rc != SQLITE_OK) {
        sqlite3_exec(db,"ROLLBACK;",nullptr,nullptr,nullptr);
        result.status=-EIO;
        result.detail=err ? err : sqlite3_errmsg(db);
        sqlite3_free(err);
        sqlite3_close(db);
        sqlite3_free(work);
        return result;
    }
    sqlite3_free(err);

    sqlite3_int64 serialized_size=0;
    unsigned char* serialized=sqlite3_serialize(db,"main",&serialized_size,0);
    if (!serialized || serialized_size <= 0 || serialized_size > 64LL*1024LL*1024LL) {
        if (serialized) sqlite3_free(serialized);
        result.status=-EIO;
        result.detail="Falha ao serializar o banco corrigido. O original continua intacto.";
        sqlite3_close(db);
        sqlite3_free(work);
        return result;
    }

    st=write_bytes_atomic(dbpath,serialized,static_cast<size_t>(serialized_size),io_detail);
    sqlite3_free(serialized);
    sqlite3_close(db);
    sqlite3_free(work);
    if (st) {
        result.status=st;
        result.detail="Nao foi possivel substituir trophy_local.db. Backup preservado em "+result.backup_path+". "+io_detail;
        return result;
    }

    result.status=0;
    result.detail="Marcacao visual revertida para BLOQUEADO. Backup: "+result.backup_path+
                  ". Agora use X para tentar o desbloqueio real.";
    return result;
#endif
}

TrophyLoadResult load_trophies_for_game(const Game& game, FileSystem& fs) {
    return load_for_game_internal(game,fs);
}

}
