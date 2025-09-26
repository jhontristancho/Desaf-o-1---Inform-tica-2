// comparacion.cpp
// búsqueda binary-safe C-style

#include <cstddef>
#include <cstring>

bool comparar_con_pista(const unsigned char* salida, size_t salida_len,
                        const unsigned char* pista, size_t pista_len) {
    if (!salida || !pista) return false;
    if (pista_len == 0) return true;
    if (salida_len < pista_len) return false;

    for (size_t i = 0; i + pista_len <= salida_len; ++i) {
        size_t j = 0;
        for (; j < pista_len; ++j) {
            if (salida[i + j] != pista[j]) break;
        }
        if (j == pista_len) return true;
    }
    return false;
}
