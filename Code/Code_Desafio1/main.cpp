// main.cpp
// Fuerza bruta enfocada en Encriptado2 (valores del README aplicados).
// Forzará: caso = 2, n = 3, K = 0x5A
// Produce: salida.txt, previews_normales.txt, encontrados_snippet.txt
// No usa std::string.

#include <locale.h>
#include <cstdio>
#include <cstdarg>
#include <cstddef>
#include <cctype>
#include <cstdlib>
#include <iostream>
using std::cout;
using std::cerr;
using std::cin;
using std::endl;

// ---------------------------------
// Configuración (fijada para Encriptado2 del README)
// ---------------------------------
static bool DEBUG_USAR_SOLO_OBJETIVO = false; // probar sólo la combinación objetivo
static const int DEBUG_OBJETIVO_N = 3;       // rotación indicada en README
static const int DEBUG_OBJETIVO_K = 0x5A;   // clave indicada en README

// además forzamos el caso objetivo (Encriptado2)
static bool DEBUG_USAR_SOLO_CASO = false;
static const int DEBUG_OBJETIVO_CASO = 2;
// ---------------------------------

// --- prototipos (implementados en tus .cpp) ---
bool abrir_y_leer_en_bloques(const char *ruta,
                             unsigned char ***bloques_out,
                             size_t **tamanios_out,
                             size_t &num_bloques_out,
                             size_t &total_bytes_out);

void liberar_bloques(unsigned char **bloques, size_t *tamanios, size_t num_bloques);

unsigned char** desencriptar_xor(unsigned char **bloques, size_t *tamanios, size_t num_bloques, unsigned char clave);
unsigned char** desencriptar_rotacion(unsigned char **bloques, size_t *tamanios, size_t num_bloques, int n);
void liberar_bloques_desencriptados(unsigned char **bloques, size_t num_bloques);

unsigned char* descomprimir_RLE(unsigned char **bloques, size_t *tamanios, size_t num_bloques, size_t &salida_len_out);
unsigned char* descomprimir_LZ78(unsigned char **bloques, size_t *tamanios, size_t num_bloques, size_t &salida_len_out);

// ------------------ logging global ------------------
static FILE *g_log = nullptr;
static FILE *g_previews = nullptr;
static FILE *g_encontrados = nullptr;

static void abrir_logs() {
    g_log = fopen("salida.txt", "wb");
    g_previews = fopen("previews_normales.txt", "a");
    g_encontrados = fopen("encontrados_snippet.txt", "a");
}

static void cerrar_logs() {
    if (g_log) { fclose(g_log); g_log = nullptr; }
    if (g_previews) { fclose(g_previews); g_previews = nullptr; }
    if (g_encontrados) { fclose(g_encontrados); g_encontrados = nullptr; }
}

static void log_printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);

    if (g_log) {
        va_list ap2;
        va_start(ap2, fmt);
        vfprintf(g_log, fmt, ap2);
        va_end(ap2);
        fflush(g_log);
    }
}

static bool guardar_binario(const char *ruta, const unsigned char *buf, size_t len) {
    FILE *f = fopen(ruta, "wb");
    if (!f) return false;
    size_t w = fwrite(buf, 1, len, f);
    fclose(f);
    return (w == len);
}

static void append_preview_text(const char *label, const unsigned char *buf, size_t len) {
    if (g_previews) {
        fprintf(g_previews, "%s\n", label);
        for (size_t i = 0; i < len; ++i) {
            unsigned char c = buf[i];
            if (isprint(c)) fputc((char)c, g_previews);
            else fprintf(g_previews, "\\x%02X", (unsigned int)c);
        }
        fputc('\n', g_previews);
        fflush(g_previews);
    }
    if (g_log) {
        fprintf(g_log, "%s\n", label);
        for (size_t i = 0; i < len; ++i) {
            unsigned char c = buf[i];
            if (isprint(c)) fputc((char)c, g_log);
            else fprintf(g_log, "\\x%02X", (unsigned int)c);
        }
        fputc('\n', g_log);
        fflush(g_log);
    }
}

static void append_encontrado(const char *msg) {
    if (g_encontrados) {
        fprintf(g_encontrados, "%s\n", msg);
        fflush(g_encontrados);
    }
    if (g_log) {
        fprintf(g_log, "%s\n", msg);
        fflush(g_log);
    }
}

// ------------------ previews ------------------
static void print_hex_preview_from_blocks_with_index(unsigned char **bloques, size_t *tamanios, size_t num_bloques, size_t max_bytes) {
    size_t printed = 0;
    size_t idx = 0, off = 0;
    size_t count_since_label = 0;
    while (idx < num_bloques && printed < max_bytes) {
        if (off >= tamanios[idx]) { ++idx; off = 0; continue; }
        if (count_since_label == 0) log_printf("[%zu:%zu] ", idx, off);
        unsigned char b = bloques[idx][off++];
        log_printf("%02X ", (unsigned int)b);
        ++printed;
        ++count_since_label;
        if (count_since_label >= 8) {
            count_since_label = 0;
            log_printf("  ");
        }
    }
    if (printed == 0) log_printf("(vacio)");
    log_printf("\n");
}

static void print_text_preview_to_logs(const unsigned char *buf, size_t len, size_t max_chars) {
    size_t show = (len < max_chars) ? len : max_chars;
    if (show == 0) { log_printf("(vacio)\n"); return; }
    for (size_t i = 0; i < show; ++i) {
        unsigned char c = buf[i];
        if (isprint(c)) log_printf("%c", (char)c);
        else log_printf("\\x%02X", (unsigned int)c);
    }
    if (len > show) log_printf("... (%zu bytes más)", len - show);
    log_printf("\n");
}

// ------------------ util ------------------
static unsigned char* concatenar_bloques(unsigned char **bloques, size_t *tamanios, size_t num_bloques, size_t &out_len) {
    out_len = 0;
    for (size_t i = 0; i < num_bloques; ++i) out_len += tamanios[i];
    if (out_len == 0) return nullptr;
    unsigned char *buf = new (std::nothrow) unsigned char[out_len];
    if (!buf) return nullptr;
    size_t pos = 0;
    for (size_t i = 0; i < num_bloques; ++i) {
        for (size_t j = 0; j < tamanios[i]; ++j) buf[pos++] = bloques[i][j];
    }
    return buf;
}

static long buscar_subsec(const unsigned char *hay, size_t hlen, const unsigned char *needle, size_t nlen) {
    if (!hay || !needle || nlen == 0 || hlen < nlen) return -1;
    for (size_t i = 0; i + nlen <= hlen; ++i) {
        bool match = true;
        for (size_t j = 0; j < nlen; ++j) {
            if (hay[i + j] != needle[j]) { match = false; break; }
        }
        if (match) return (long)i;
    }
    return -1;
}

// Extraer ASCII (normalizar) de buffer con padding
static unsigned char* extraer_ascii(const unsigned char *buf, size_t len, size_t &out_len) {
    if (!buf || len < 3) { out_len = 0; return nullptr; }
    out_len = len / 3;
    unsigned char *out = new unsigned char[out_len];
    size_t j = 0;
    for (size_t i = 2; i < len; i += 3) {
        out[j++] = buf[i];
        if (j >= out_len) break;
    }
    return out;
}

// ------------------ main ------------------
int main() {
    setlocale(LC_ALL, "");
    abrir_logs();
    log_printf("=== Fuerza bruta (RLE/LZ78) integracion - DEBUG (Encriptado2 target) ===\n");
    log_printf("Forzando: caso=%d, n=%d, K=0x%02X\n", DEBUG_OBJETIVO_CASO, DEBUG_OBJETIVO_N, DEBUG_OBJETIVO_K);

    log_printf("Ingrese la cantidad de casos a evaluar (n): ");

    int casos = 0;
    if (!(cin >> casos) || casos <= 0) {
        cerr << "Entrada invalida\n";
        cerrar_logs();
        return 1;
    }
    log_printf("%d\n", casos);

    for (int caso = 1; caso <= casos; ++caso) {
        // si activaste forzar SOLO CASO, saltamos los demás
        if (DEBUG_USAR_SOLO_CASO && caso != DEBUG_OBJETIVO_CASO) continue;

        char encriptado_path[128];
        char pista_path_try1[128];
        char pista_path_try2[128];
        snprintf(encriptado_path, sizeof(encriptado_path), "Encriptado%d.txt", caso);
        snprintf(pista_path_try1, sizeof(pista_path_try1), "pista%d.txt", caso);
        snprintf(pista_path_try2, sizeof(pista_path_try2), "pista%d", caso);

        log_printf("\n--- Caso %d ---\n", caso);

        unsigned char **enc_bloques = nullptr;
        size_t *enc_tamanios = nullptr;
        size_t enc_num = 0, enc_total = 0;
        if (!abrir_y_leer_en_bloques(encriptado_path, &enc_bloques, &enc_tamanios, enc_num, enc_total)) {
            log_printf("No se pudo abrir %s\n", encriptado_path);
            continue;
        }
        log_printf("Leidos %zu bytes en %zu bloques\n", enc_total, enc_num);

        // leer pista
        unsigned char **p_bloques = nullptr;
        size_t *p_tamanios = nullptr;
        size_t p_num = 0, p_total = 0;
        bool pista_ok = false;
        if (abrir_y_leer_en_bloques(pista_path_try1, &p_bloques, &p_tamanios, p_num, p_total)) pista_ok = true;
        else if (abrir_y_leer_en_bloques(pista_path_try2, &p_bloques, &p_tamanios, p_num, p_total)) pista_ok = true;

        if (!pista_ok) {
            log_printf("No se encontro archivo de pista (pista%d[.txt])\n", caso);
            liberar_bloques(enc_bloques, enc_tamanios, enc_num);
            continue;
        }
        log_printf("Pista leida: %zu bytes\n", p_total);

        size_t pista_len = 0;
        unsigned char *pista_buf = concatenar_bloques(p_bloques, p_tamanios, p_num, pista_len);
        log_printf("Preview pista (hasta 200 chars):\n");
        print_text_preview_to_logs(pista_buf, pista_len, 200);

        bool encontrado = false;

        // Si DEBUG_USAR_SOLO_OBJETIVO -> probamos solo la combinacion objetivo
        for (int n = 1; n <= 7 && !encontrado; ++n) {
            for (int K = 0; K <= 255 && !encontrado; ++K) {
                if (DEBUG_USAR_SOLO_OBJETIVO) {
                    if (!(n == DEBUG_OBJETIVO_N && K == DEBUG_OBJETIVO_K)) continue;
                }

                unsigned char clave = (unsigned char)K;
                unsigned char **xor_bloques = desencriptar_xor(enc_bloques, enc_tamanios, enc_num, clave);
                if (!xor_bloques) continue;

                log_printf("\n[n=%d K=0x%02X] Después de XOR (primeros 64 bytes, hex con índice):\n", n, K);
                print_hex_preview_from_blocks_with_index(xor_bloques, enc_tamanios, enc_num, 64);

                unsigned char **rot_bloques = desencriptar_rotacion(xor_bloques, enc_tamanios, enc_num, n);
                if (!rot_bloques) {
                    liberar_bloques_desencriptados(xor_bloques, enc_num);
                    continue;
                }

                size_t rot_len = 0;
                unsigned char *rot_concat = concatenar_bloques(rot_bloques, enc_tamanios, enc_num, rot_len);
                if (!rot_concat) {
                    liberar_bloques_desencriptados(xor_bloques, enc_num);
                    liberar_bloques_desencriptados(rot_bloques, enc_num);
                    continue;
                }

                // guardar binario rotado (último)
                guardar_binario("debug_rot_last.bin", rot_concat, rot_len);

                // normalizar a ascii
                size_t norm_len = 0;
                unsigned char *norm_buf = extraer_ascii(rot_concat, rot_len, norm_len);

                // guardar norm_buf en archivo legible por intento
                if (norm_buf && norm_len > 0) {
                    char fname_norm[128];
                    snprintf(fname_norm, sizeof(fname_norm), "debug_norm_caso%d_n%d_K%02X.txt", caso, n, K);
                    FILE *fn = fopen(fname_norm, "wb");
                    if (fn) {
                        fwrite(norm_buf, 1, norm_len, fn);
                        fclose(fn);
                    }
                }

                delete[] rot_concat;

                if (norm_buf && norm_len > 0) {
                    log_printf("[n=%d K=0x%02X] Preview normalizado:\n", n, K);
                    print_text_preview_to_logs(norm_buf, norm_len, 400);

                    // añadir a previews_normales.txt
                    char label[128];
                    snprintf(label, sizeof(label), "--- Caso %d n=%d K=0x%02X Preview normalizado ---", caso, n, K);
                    append_preview_text(label, norm_buf, norm_len);

                    // preparar 1-bloque para descompresores
                    unsigned char *bloques_tmp[1];
                    size_t tamanios_tmp[1];
                    bloques_tmp[0] = norm_buf;
                    tamanios_tmp[0] = norm_len;

                    // RLE (aunque README dice LZ78, probamos ambos)
                    size_t out_len_rle = 0;
                    unsigned char *rle_out = descomprimir_RLE(bloques_tmp, tamanios_tmp, 1, out_len_rle);
                    if (rle_out) {
                        log_printf("[n=%d K=0x%02X] Resultado intento RLE: %zu bytes. Preview (texto):\n", n, K, out_len_rle);
                        print_text_preview_to_logs(rle_out, out_len_rle, 400);

                        long pos = buscar_subsec(rle_out, out_len_rle, pista_buf, pista_len);
                        if (pos >= 0) {
                            char msg[256];
                            snprintf(msg, sizeof(msg), ">>> ENCONTRADO (RLE): caso=%d n=%d K=0x%02X posicion=%ld", caso, n, K, pos);
                            log_printf("%s\n", msg);
                            append_encontrado(msg);
                            char outpath[128];
                            snprintf(outpath, sizeof(outpath), "resultado_caso%d_RLE_n%d_K%02X.txt", caso, n, K);
                            FILE *f = fopen(outpath, "wb");
                            if (f) { fwrite(rle_out, 1, out_len_rle, f); fclose(f); log_printf("Escrito: %s\n", outpath); }
                            encontrado = true;
                        }
                        delete[] rle_out;
                    } else {
                        log_printf("[n=%d K=0x%02X] RLE -> no parseable o resultó vacío\n", n, K);
                    }

                    // LZ78
                    if (!encontrado) {
                        size_t out_len_lz = 0;
                        unsigned char *lz_out = descomprimir_LZ78(bloques_tmp, tamanios_tmp, 1, out_len_lz);
                        if (lz_out) {
                            log_printf("[n=%d K=0x%02X] Resultado intento LZ78: %zu bytes. Preview (texto):\n", n, K, out_len_lz);
                            print_text_preview_to_logs(lz_out, out_len_lz, 400);

                            long pos2 = buscar_subsec(lz_out, out_len_lz, pista_buf, pista_len);
                            if (pos2 >= 0) {
                                char msg2[256];
                                snprintf(msg2, sizeof(msg2), ">>> ENCONTRADO (LZ78): caso=%d n=%d K=0x%02X posicion=%ld", caso, n, K, pos2);
                                log_printf("%s\n", msg2);
                                append_encontrado(msg2);
                                char outpath2[128];
                                snprintf(outpath2, sizeof(outpath2), "resultado_caso%d_LZ78_n%d_K%02X.txt", caso, n, K);
                                FILE *f2 = fopen(outpath2, "wb");
                                if (f2) { fwrite(lz_out, 1, out_len_lz, f2); fclose(f2); log_printf("Escrito: %s\n", outpath2); }
                                encontrado = true;
                            }
                            delete[] lz_out;
                        } else {
                            log_printf("[n=%d K=0x%02X] LZ78 -> no parseable o resultó vacío\n", n, K);
                        }
                    }

                    delete[] norm_buf;
                } else {
                    log_printf("[n=%d K=0x%02X] Normalización produjo buffer vacío\n", n, K);
                    if (norm_buf) delete[] norm_buf;
                }

                liberar_bloques_desencriptados(xor_bloques, enc_num);
                liberar_bloques_desencriptados(rot_bloques, enc_num);

            } // K
        } // n

        if (!encontrado) log_printf("No se encontro la pista en este caso\n");

        // limpieza caso
        delete[] pista_buf;
        liberar_bloques(enc_bloques, enc_tamanios, enc_num);
        liberar_bloques(p_bloques, p_tamanios, p_num);
    } // caso

    log_printf("\nFin del proceso.\n");
    cerrar_logs();
    return 0;
}

