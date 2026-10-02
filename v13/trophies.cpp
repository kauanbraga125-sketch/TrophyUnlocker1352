#include "trophies.h"
#include "library.h"
#include "filesystem.h"
#include <sqlite3.h>
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>

#ifndef TU_HOST_TEST
#include <orbis/UserService.h>
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

TrophyLoadResult load_trophies_for_game(const Game& game, FileSystem& fs) {
    return load_for_game_internal(game,fs);
}

}
