// fuerza_bruta.cpp  (VERSIÓN: SOLO XOR, listado de claves imprimibles)
// - Prueba claves K = 0..255 aplicando solo desencriptar_xor
// - Llama a descomprimir_auto sobre el resultado
// - Imprime por consola las claves que generan salida "parcialmente imprimible"
// - Compara con la pista en varias normalizaciones y guarda Resultado_Caso... si hay match
// - Libera los bloques originales al final (contrato). No crea archivos intermedios.
//
// Compilar junto a tus otros .cpp:
// g++ main.cpp fuerza_bruta.cpp comparacion.cpp lectoryalmacenatxt.cpp desencriptacion.cpp Descomprimir.cpp -o brute_cases

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>

// ---------- Firmas externas (ajusta si tus funciones tienen otros nombres) ----------
unsigned char** desencriptar_xor(unsigned char **bloques, size_t *tamanios, size_t num_bloques, unsigned char clave);
void liberar_bloques_desencriptados(unsigned char **bloques, size_t num_bloques);
unsigned char* descomprimir_auto(unsigned char **bloques,
                                 size_t *tamanios,
                                 size_t num_bloques,
                                 size_t &salida_len,
                                 char metodo_out[16]);
void liberar_bloques(unsigned char **bloques, size_t *tamanios, size_t num_bloques);

// -----------------------------------------------------------------------------
// Helpers locales (no colisionan con comparacion.cpp)
// -----------------------------------------------------------------------------
static bool contains_sub_local(const unsigned char* buf, size_t buf_len, const unsigned char* pat, size_t pat_len) {
    if (!buf || !pat) return false;
    if (pat_len == 0) return true;
    if (buf_len < pat_len) return false;
    for (size_t i = 0; i + pat_len <= buf_len; ++i) {
        size_t j = 0;
        for (; j < pat_len; ++j) if (buf[i+j] != pat[j]) break;
        if (j == pat_len) return true;
    }
    return false;
}

static unsigned char* to_lower_copy_local(const unsigned char* buf, size_t len) {
    unsigned char* out = (unsigned char*) malloc(len ? len : 1);
    if (!out) return NULL;
    for (size_t i = 0; i < len; ++i) out[i] = (unsigned char) tolower(buf[i]);
    return out;
}
static unsigned char* strip_newlines_copy_local(const unsigned char* buf, size_t len, size_t* out_len) {
    unsigned char* tmp = (unsigned char*) malloc(len ? len : 1);
    if (!tmp) { *out_len = 0; return NULL; }
    size_t p = 0;
    for (size_t i = 0; i < len; ++i) {
        if (buf[i] == '\n' || buf[i] == '\r') continue;
        tmp[p++] = buf[i];
    }
    *out_len = p;
    unsigned char* out = (unsigned char*) realloc(tmp, p ? p : 1);
    if (out) return out;
    return tmp;
}
static unsigned char* strip_spaces_copy_local(const unsigned char* buf, size_t len, size_t* out_len) {
    unsigned char* tmp = (unsigned char*) malloc(len ? len : 1);
    if (!tmp) { *out_len = 0; return NULL; }
    size_t p = 0;
    bool last_space = false;
    for (size_t i = 0; i < len; ++i) {
        unsigned char c = buf[i];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            if (!last_space) { tmp[p++] = ' '; last_space = true; }
        } else {
            tmp[p++] = c;
            last_space = false;
        }
    }
    // trim
    size_t start = 0; while (start < p && tmp[start] == ' ') start++;
    size_t end = p; while (end > start && tmp[end-1] == ' ') end--;
    size_t outp = (end > start) ? (end - start) : 0;
    unsigned char* out = (unsigned char*) malloc(outp ? outp : 1);
    if (!out) { free(tmp); *out_len = 0; return NULL; }
    if (outp) memcpy(out, tmp + start, outp);
    free(tmp);
    *out_len = outp;
    return out;
}

// preview printer
static void print_preview_local(const unsigned char* buf, size_t len, size_t maxbytes) {
    size_t dump = len < maxbytes ? len : maxbytes;
    for (size_t i = 0; i < dump; ++i) {
        unsigned char c = buf[i];
        if (c >= 32 && c <= 126) putchar(c);
        else printf("\\x%02X", c);
    }
    if (len > dump) printf("...");
    printf("\n");
}

// decide si una salida es "parcialmente imprimible"
// strategy: examina primeros N bytes y cuenta imprimibles; devuelve true si >= threshold
static bool is_partially_printable(const unsigned char* buf, size_t len, size_t check_bytes = 48, size_t threshold = 12) {
    size_t n = (len < check_bytes) ? len : check_bytes;
    size_t cnt = 0;
    for (size_t i = 0; i < n; ++i) {
        unsigned char c = buf[i];
        if (c >= 32 && c <= 126) cnt++;
    }
    return cnt >= threshold;
}

// -----------------------------------------------------------------------------
// ejecutar_fuerza_bruta: SOLO XOR (K=0..255).
// Contrato: libera bloques (llama liberar_bloques al final).
// -----------------------------------------------------------------------------
extern "C" bool ejecutar_fuerza_bruta(unsigned char **bloques,
                                      size_t *tamanios,
                                      size_t num_bloques,
                                      const unsigned char *pista_buf,
                                      size_t pista_len,
                                      int caso) {
    if (!bloques || !tamanios || num_bloques == 0) return false;
    if (!pista_buf || pista_len == 0) return false;

    bool encontrado = false;
    int intentos = 0;

    // Solo XOR: no rotaciones
    for (int K = 0; K <= 255 && !encontrado; ++K) {
        ++intentos;
        if ((intentos & 0x3FF) == 0) {
            printf("Progreso (solo XOR): K=%d intentos=%d\n", K, intentos);
            fflush(stdout);
        }

        // aplicar XOR
        unsigned char **bloques_xor = desencriptar_xor(bloques, tamanios, num_bloques, (unsigned char)K);
        if (!bloques_xor) continue;

        // intentar descomprimir el resultado
        size_t salida_len = 0;
        char metodo_out[16] = {0};
        unsigned char *salida = descomprimir_auto(bloques_xor, tamanios, num_bloques, salida_len, metodo_out);

        // liberar bloques_xor (contrato con la función)
        liberar_bloques_desencriptados(bloques_xor, num_bloques);

        if (!salida || salida_len == 0) {
            if (salida) free(salida);
            continue;
        }

        // Si salida tiene parte imprimible, reportarlo (ayuda rápida)
        if (is_partially_printable(salida, salida_len)) {
            printf("[CANDIDATE] K=%03d metodo=%s preview: ", K, metodo_out);
            print_preview_local(salida, salida_len, 120);
        }

        // Comparaciones: exacta
        if (contains_sub_local(salida, salida_len, pista_buf, pista_len)) {
            char outname[256];
            snprintf(outname, sizeof(outname), "Resultado_Caso%d_SoloXOR_K%02X_%s_exact.txt", caso, K, metodo_out);
            FILE *fo = fopen(outname, "wb");
            if (fo) { fwrite(salida, 1, salida_len, fo); fclose(fo); }
            printf("[ENCONTRADO exact] Caso %d -> Solo XOR K=0x%02X Metodo=%s Archivo=%s\n",
                   caso, K, metodo_out, outname);
            encontrado = true;
        }
        if (encontrado) {
            // liberar salida (si descomprimir_auto usó malloc/free)
            free(salida);
            break;
        }

        // lowercase comparison
        unsigned char *s_low = to_lower_copy_local(salida, salida_len);
        unsigned char *p_low = to_lower_copy_local(pista_buf, pista_len);
        if (s_low && p_low) {
            if (contains_sub_local(s_low, salida_len, p_low, pista_len)) {
                char outname[256];
                snprintf(outname, sizeof(outname), "Resultado_Caso%d_SoloXOR_K%02X_%s_lower.txt", caso, K, metodo_out);
                FILE *fo = fopen(outname, "wb");
                if (fo) { fwrite(salida, 1, salida_len, fo); fclose(fo); }
                printf("[ENCONTRADO lower] Caso %d -> Solo XOR K=0x%02X Metodo=%s Archivo=%s\n",
                       caso, K, metodo_out, outname);
                encontrado = true;
            }
        }
        if (s_low) free(s_low);
        if (p_low) free(p_low);
        if (encontrado) { free(salida); break; }

        // strip newlines
        size_t s_sn_len = 0, p_sn_len = 0;
        unsigned char *s_sn = strip_newlines_copy_local(salida, salida_len, &s_sn_len);
        unsigned char *p_sn = strip_newlines_copy_local(pista_buf, pista_len, &p_sn_len);
        if (s_sn && p_sn) {
            if (contains_sub_local(s_sn, s_sn_len, p_sn, p_sn_len)) {
                char outname[256];
                snprintf(outname, sizeof(outname), "Resultado_Caso%d_SoloXOR_K%02X_%s_stripNL.txt", caso, K, metodo_out);
                FILE *fo = fopen(outname, "wb");
                if (fo) { fwrite(salida, 1, salida_len, fo); fclose(fo); }
                printf("[ENCONTRADO stripNL] Caso %d -> Solo XOR K=0x%02X Metodo=%s Archivo=%s\n",
                       caso, K, metodo_out, outname);
                encontrado = true;
            }
        }
        if (s_sn) free(s_sn);
        if (p_sn) free(p_sn);
        if (encontrado) { free(salida); break; }

        // strip & collapse spaces
        size_t s_sp_len = 0, p_sp_len = 0;
        unsigned char *s_sp = strip_spaces_copy_local(salida, salida_len, &s_sp_len);
        unsigned char *p_sp = strip_spaces_copy_local(pista_buf, pista_len, &p_sp_len);
        if (s_sp && p_sp) {
            if (contains_sub_local(s_sp, s_sp_len, p_sp, p_sp_len)) {
                char outname[256];
                snprintf(outname, sizeof(outname), "Resultado_Caso%d_SoloXOR_K%02X_%s_stripSpace.txt", caso, K, metodo_out);
                FILE *fo = fopen(outname, "wb");
                if (fo) { fwrite(salida, 1, salida_len, fo); fclose(fo); }
                printf("[ENCONTRADO stripSpace] Caso %d -> Solo XOR K=0x%02X Metodo=%s Archivo=%s\n",
                       caso, K, metodo_out, outname);
                encontrado = true;
            }
        }
        if (s_sp) free(s_sp);
        if (p_sp) free(p_sp);
        if (encontrado) { free(salida); break; }

        // no match: liberar salida
        free(salida);
    } // for K

    // liberar bloques originales (contrato)
    liberar_bloques(bloques, tamanios, num_bloques);

    printf("Fuerza bruta (Solo XOR) finalizada. intentos=%d encontrado=%d\n", intentos, encontrado ? 1 : 0);
    return encontrado;
}


