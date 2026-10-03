#include "library.h"
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>

namespace tu {
bool is_cusa(const std::string& s) {
    if (s.size() != 9 || s.compare(0, 4, "CUSA")) return false;
    for (size_t i = 4; i < 9; ++i) if (s[i] < '0' || s[i] > '9') return false;
    return true;
}
static uint16_t u16(const uint8_t* p) { return uint16_t(p[0]) | uint16_t(p[1])<<8; }
static uint32_t u32(const uint8_t* p) {
    return uint32_t(p[0]) | uint32_t(p[1])<<8 | uint32_t(p[2])<<16 | uint32_t(p[3])<<24;
}
bool parse_sfo(const std::vector<uint8_t>& b, std::string& id, std::string& title) {
    id.clear(); title.clear();
    if (b.size() < 20 || u32(b.data()) != 0x46535000) return false;
    uint32_t keys = u32(b.data()+8), data = u32(b.data()+12), count = u32(b.data()+16);
    if (count > 512 || 20ull+16ull*count > b.size() || keys < 20ull+16ull*count ||
        keys >= data || data > b.size()) return false;
    std::string got_id, got_title;
    for (uint32_t i = 0; i < count; ++i) {
        const uint8_t* e = b.data()+20+16*i;
        uint64_t key = uint64_t(keys)+u16(e), val = uint64_t(data)+u32(e+12);
        uint32_t len = u32(e+4), maxlen = u32(e+8);
        if (key >= data || val > b.size() || len > maxlen || maxlen > b.size()-val) return false;
        const char* k = reinterpret_cast<const char*>(b.data()+key);
        const void* end = memchr(k, 0, data-key);
        if (!end) return false;
        bool wanted_id = !strcmp(k, "TITLE_ID"), wanted_title = !strcmp(k, "TITLE");
        if (!wanted_id && !wanted_title) continue;
        if (u16(e+2) != 0x0204 || len < 1 || len > 512) return false;
        const char* value = reinterpret_cast<const char*>(b.data()+val);
        const char* stop = static_cast<const char*>(memchr(value, 0, len));
        if (!stop) return false;
        std::string text(value, size_t(stop-value));
        for (char& c : text) if (static_cast<unsigned char>(c) < 32) c = ' ';
        if (wanted_id) got_id = text; else got_title = text;
    }
    if (!got_id.empty() && !is_cusa(got_id)) return false;
    id = got_id; title = got_title;
    return !id.empty() || !title.empty();
}
bool valid_png(const std::vector<uint8_t>& b) {
    static const uint8_t sig[8] = {137,80,78,71,13,10,26,10};
    if (b.size() < 33 || memcmp(b.data(), sig, 8) || memcmp(b.data()+12, "IHDR", 4)) return false;
    auto be = [&b](int n) { return uint32_t(b[n])<<24 | uint32_t(b[n+1])<<16 | uint32_t(b[n+2])<<8 | b[n+3]; };
    return be(8) == 13 && be(16) > 0 && be(20) > 0 && be(16) <= 2048 && be(20) <= 2048;
}
std::string game_status(const Game& g) {
    if (g.package) return "Jogo instalado";
    if (g.metadata) return "Metadados encontrados";
    return "Nao confirmado como instalado";
}

void Library::enrich(Game& g) {
    // IMPORTANT: discovery is already complete before enrich(). These paths
    // only enrich an installed CUSA with title/icon/metadata; they never create
    // a new library entry.
    std::vector<std::string> bases;
    if (!g.path.empty()) bases.push_back(g.path);
    bases.push_back("/user/appmeta/"+g.id);
    bases.push_back("/user/appmeta/external/"+g.id);
    bases.push_back("/system_data/priv/appmeta/"+g.id);

    for (const std::string& base : bases) {
        const char* sfo_names[] = {"/param.sfo", "/sce_sys/param.sfo"};
        for (const char* suffix : sfo_names) {
            std::vector<uint8_t> bytes;
            if (fs.read(base+suffix, 65536, bytes)) continue;
            std::string id, title;
            if (parse_sfo(bytes, id, title) && (id.empty() || id == g.id)) {
                g.metadata = true;
                if (!title.empty()) g.title = title;
            }
        }
        if (g.icon.empty()) {
            for (const char* suffix : {"/icon0.png", "/sce_sys/icon0.png"}) {
                if (fs.exists(base+suffix)) { g.icon = base+suffix; break; }
            }
        }
    }
    if (g.title.empty()) g.title = g.id;
}

size_t Library::merge(const Game& value) {
    for (size_t i = 0; i < games.size(); ++i) if (games[i].id == value.id) {
        Game& g = games[i];
        if (g.path.empty() || value.package) g.path = value.path;
        if (g.icon.empty()) g.icon = value.icon;
        if (!value.title.empty() && value.title != value.id) g.title = value.title;
        g.package |= value.package; g.metadata |= value.metadata;
        return i;
    }
    if (games.size() >= 512) return size_t(-1);
    games.push_back(value); return games.size()-1;
}

void Library::begin_scan() {
    games.clear(); roots.clear();
    root_index = 0; enrich_index = 0; scanning = true;
}

bool Library::scan_step() {
    if (!scanning) return false;

    // V13.9: these are the ONLY discovery roots. appmeta and trophy_local.db
    // are deliberately excluded so stale/uninstalled trophy sets cannot appear.
    const char* paths[] = {
        "/user/app",
        "/mnt/ext0/user/app",
        "/mnt/ext1/user/app"
    };

    if (root_index < sizeof(paths)/sizeof(paths[0])) {
        const char* root = paths[root_index++];
        std::vector<DirEntry> entries;
        RootResult status = {root, fs.list(root, entries), 0};
        if (!status.status) for (const DirEntry& ent : entries) {
            if (!is_cusa(ent.name) || (ent.type != 4 && ent.type != 0 && ent.type != 10)) continue;
            ++status.matches;
            Game game;
            game.id = ent.name;
            game.path = join_path(root, ent.name);
            game.package = true;   // presence in an installed-app root is authoritative
            game.metadata = false;
            game.manual = false;
            merge(game);
        }
        roots.push_back(status);
        if (root_index == sizeof(paths)/sizeof(paths[0])) {
            std::sort(games.begin(), games.end(), [](const Game& a, const Game& b) { return a.id < b.id; });
        }
        return true;
    }

    if (enrich_index < games.size()) {
        enrich(games[enrich_index++]);
        return true;
    }
    scanning = false;
    return false;
}

void Library::scan() {
    begin_scan(); while (scan_step()) {}
}

// Manual lookup remains available to diagnostics/source compatibility, but it
// is intentionally NOT merged into the V13.9 main installed-games carousel.
int Library::add_id(const std::string& id) {
    if (!is_cusa(id)) return -EINVAL;
    for (size_t i=0; i<games.size(); ++i) if (games[i].id == id) return int(i);
    return -ENOENT;
}

int Library::add_folder(const std::string& path) {
    if (!valid_path(path)) return -EINVAL;
    for (size_t i=0; i<games.size(); ++i) if (games[i].path == path) return int(i);
    return -ENOENT;
}

int Library::load_manual() {
    manuals.clear();
    return 0;
}

int Library::save_manual() {
    return 0;
}

std::string Library::diagnostic(const AccessResult& a) const {
    char line[220];
    snprintf(line, sizeof(line), "Trophy Unlocker V13.9 Installed Carousel\nGoldHEN requested=%d sdk_raw=%lld sdk_CF=%llu\naccess_sent=%d access_raw=%lld access_CF=%llu\n", int(a.attempted), (long long)a.sdk, (unsigned long long)a.sdk_carry, int(a.jailbreak_attempted), (long long)a.jailbreak, (unsigned long long)a.jailbreak_carry);
    std::string result = line;
    for (const RootResult& root : roots) {
        snprintf(line, sizeof(line), "%s status=%d installed_CUSA=%d\n", root.path.c_str(), root.status, root.matches);
        result += line;
    }
    for (const Game& game : games) result += game.id+" | "+game_status(game)+" | "+game.path+"\n";
    return result;
}
}
