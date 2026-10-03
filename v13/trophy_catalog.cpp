#include "trophies.h"
#include <sqlite3.h>
#include <cstdio>
#include <cstring>

#ifndef TU_HOST_TEST
#include <orbis/UserService.h>
#endif

namespace tu {
namespace {

static std::string coltext(sqlite3_stmt* stmt, int col) {
    const unsigned char* text = sqlite3_column_text(stmt, col);
    return text ? reinterpret_cast<const char*>(text) : "";
}

#ifndef TU_HOST_TEST
static int open_snapshot(const std::string& path,
                         sqlite3** out_db,
                         unsigned char** out_buffer,
                         std::string& detail) {
    *out_db = nullptr;
    *out_buffer = nullptr;
    FILE* fp = fopen(path.c_str(), "rb");
    if (!fp) { detail = "Nao foi possivel abrir trophy_local.db."; return SQLITE_CANTOPEN; }
    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); detail = "Falha ao medir trophy_local.db."; return SQLITE_IOERR; }
    long size = ftell(fp);
    if (size <= 100 || size > 64L*1024L*1024L) { fclose(fp); detail = "Tamanho inesperado do banco de trofeus."; return SQLITE_NOTADB; }
    rewind(fp);
    unsigned char* buffer = static_cast<unsigned char*>(sqlite3_malloc64(static_cast<sqlite3_uint64>(size)));
    if (!buffer) { fclose(fp); detail = "Sem memoria para copiar trophy_local.db."; return SQLITE_NOMEM; }
    size_t got = fread(buffer, 1, static_cast<size_t>(size), fp);
    fclose(fp);
    if (got != static_cast<size_t>(size) || size < 16 || memcmp(buffer, "SQLite format 3\000", 16) != 0) {
        sqlite3_free(buffer); detail = "Snapshot do banco de trofeus invalido."; return SQLITE_NOTADB;
    }
    sqlite3* db = nullptr;
    int rc = sqlite3_open(":memory:", &db);
    if (rc != SQLITE_OK) { sqlite3_free(buffer); if (db) sqlite3_close(db); detail = "Falha ao criar banco em memoria."; return rc; }
    rc = sqlite3_deserialize(db, "main", buffer,
                             static_cast<sqlite3_int64>(size),
                             static_cast<sqlite3_int64>(size),
                             SQLITE_DESERIALIZE_READONLY);
    if (rc != SQLITE_OK) { detail = sqlite3_errmsg(db); sqlite3_close(db); sqlite3_free(buffer); return rc; }
    *out_db = db;
    *out_buffer = buffer;
    return SQLITE_OK;
}
#endif

}

TrophyCatalogResult load_all_trophy_sets() {
    TrophyCatalogResult result;
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
    snprintf(dbpath, sizeof(dbpath), "/user/home/%08x/trophy/db/trophy_local.db", unsigned(result.user_id));
    result.database_path = dbpath;

    sqlite3* db = nullptr;
    unsigned char* backing = nullptr;
    std::string detail;
    int rc = open_snapshot(result.database_path, &db, &backing, detail);
    if (rc != SQLITE_OK) {
        result.sqlite_status = rc;
        result.status = -1;
        result.detail = detail;
        return result;
    }

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT id, trophy_title_id, title FROM tbl_trophy_title WHERE status = 0 ORDER BY title COLLATE NOCASE, trophy_title_id";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc == SQLITE_OK) {
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            if (result.sets.size() >= 2048) { rc = SQLITE_TOOBIG; break; }
            TrophySet set;
            set.database_id = sqlite3_column_int64(stmt, 0);
            set.np_communication_id = coltext(stmt, 1);
            set.title = coltext(stmt, 2);
            if (valid_np_communication_id(set.np_communication_id)) result.sets.push_back(set);
        }
        if (rc == SQLITE_DONE) rc = SQLITE_OK;
    }
    if (stmt) sqlite3_finalize(stmt);
    if (rc != SQLITE_OK) result.detail = sqlite3_errmsg(db);
    sqlite3_close(db);
    sqlite3_free(backing);

    result.sqlite_status = rc;
    result.status = (rc == SQLITE_OK && !result.sets.empty()) ? 0 : -1;
    if (!result.status) {
        char line[128];
        snprintf(line, sizeof(line), "%zu conjunto(s) de trofeus disponiveis para vinculo manual.", result.sets.size());
        result.detail = line;
    } else if (result.detail.empty()) {
        result.detail = "Nenhum conjunto de trofeus encontrado no usuario.";
    }
#else
    result.detail = "TU_HOST_TEST";
#endif
    return result;
}

}
