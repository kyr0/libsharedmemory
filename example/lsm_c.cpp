// C wrapper implementation — compiled as C++ but exports C symbols.

#include "lsm_c.h"
#include <libsharedmemory/libsharedmemory.hpp>

struct lsm_memory {
    lsm::Memory impl;
    lsm_memory(const std::string& n, size_t s, bool p) : impl(n, s, p) {}
};

extern "C" {

lsm_memory* lsm_create(const char* name, size_t size, int persistent)
{
    auto* m = new(std::nothrow) lsm_memory(std::string(name), size, persistent != 0);
    if (!m) return nullptr;
    if (m->impl.create() != lsm::Error::OK) { delete m; return nullptr; }
    return m;
}

lsm_memory* lsm_open(const char* name, size_t size, int persistent)
{
    auto* m = new(std::nothrow) lsm_memory(std::string(name), size, persistent != 0);
    if (!m) return nullptr;
    if (m->impl.open() != lsm::Error::OK) { delete m; return nullptr; }
    return m;
}

void* lsm_data(lsm_memory* mem) { return mem ? mem->impl.data() : nullptr; }
size_t lsm_size(lsm_memory* mem) { return mem ? mem->impl.size() : 0; }

void lsm_close(lsm_memory* mem) { if (mem) mem->impl.close(); }
void lsm_destroy(lsm_memory* mem) { if (mem) mem->impl.destroy(); }
void lsm_free(lsm_memory* mem) { delete mem; }

} // extern "C"
