// nvs/FavStore.cpp — Almacén de favoritos MIDI en NVS  (AITEC 2026-06-30)
// NVS namespace "favs". Clave por índice: "0", "1", "2"...
// "n" guarda el número total de slots (puede haber huecos vacíos entre medios).
#include "FavStore.h"
#include <Preferences.h>
#include <string.h>

static Preferences s_prefs;
static int         s_count = 0;   // cache del total (máximo índice+1)

bool favInit() {
    bool ok = s_prefs.begin("favs", false);
    s_count = (int)s_prefs.getInt("n", 0);
    return ok;
}

int favCount() { return s_count; }

bool favLoad(int idx, FavEntry& out) {
    char key[8];
    snprintf(key, sizeof(key), "%d", idx);
    size_t sz = s_prefs.getBytesLength(key);
    if (sz != sizeof(FavEntry)) return false;
    s_prefs.getBytes(key, &out, sizeof(FavEntry));
    return true;
}

bool favSave(int idx, const FavEntry& e) {
    char key[8];
    snprintf(key, sizeof(key), "%d", idx);
    bool ok = (s_prefs.putBytes(key, &e, sizeof(FavEntry)) == sizeof(FavEntry));
    if (ok && idx >= s_count) {
        s_count = idx + 1;
        s_prefs.putInt("n", s_count);
    }
    return ok;
}

void favDelete(int idx) {
    char key[8];
    snprintf(key, sizeof(key), "%d", idx);
    s_prefs.remove(key);
}
