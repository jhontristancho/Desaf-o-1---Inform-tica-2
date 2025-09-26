// main.cpp
// Lee EncriptadoX.txt (decodifica ZR->byte si aplica) y ejecuta fuerza bruta:
// variantes: XOR->ROT, ROT->XOR, Solo XOR, Solo ROT
// No escribe samples; imprime previews y escribe solo el Resultado final si encuentra la pista.
//
// Compilar (ejemplo):
// g++ main.cpp desencriptacion.cpp Descomprimir.cpp -o brute_cases
// (añade lectoryalmacenatxt.cpp si lo necesitas; el main hace su propia lectura/decodificación)

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>

// -------------------------
// Declaraciones de tus funciones (ajusta si tus firmas son distintas)
// -------------------------
unsigned char** desencriptar_xor(unsigned char **bloques, size_t *tamanios, size_t num_bloques, unsigned char clave);
unsigned char** desencriptar_rotacion(unsigned char **bloques, size_t *tamanios, size_t num_bloques, int n);
void liberar_bloques_desencriptados(unsigned char **bloques, size_t num_bloques);

// Descompresión automática (RLE / LZ78) - rellena metodo_out con "RLE" / "LZ78" etc.
// Devuelve puntero heap y longitud en salida_len.
// NOTA: en este código se libera con free(salida). Si tu Descomprimir.cpp usa new[], cambia free por delete[].
unsigned char* descomprimir_auto(unsigned char **bloques,
                                 size_t *tamanios,
                                 size_t num_bloques,
                                 size_t &salida_len,
                                 char metodo_out[16]);

// -------------------------
// Helpers: decodificar ZR->bytes
// -------------------------
static void decodeZR_to_buffer(const unsigned char* inbuf, size_t inlen, unsigned char** outbuf, size_t* outlen) {
    // input is bytes (text). We look for sequences 'Z' 'R' <c> and convert to <c>.
    // If not matching, copy byte as-is.
    unsigned char *tmp = (unsigned char*) malloc(inlen ? inlen : 1);
    if (!tmp) { *outbuf = NULL; *outlen = 0; return; }
    size_t p = 0;
    size_t i = 0;
    while (i < inlen) {
        if (i + 2 < inlen && inbuf[i] == 'Z' && inbuf[i+1] == 'R') {
            tmp[p++] = inbuf[i+2];
            i += 3;
        } else {
            tmp[p++] = inbuf[i];
            i += 1;
        }
    }
    // shrink
    unsigned char *out = (unsigned char*) realloc(tmp, p ? p : 1);
    if (out) {
        *outbuf = out;
    } else {
        *outbuf = tmp;
    }
    *outlen = p;
}

// -------------------------
// Comparación binary-safe
// -------------------------
static bool contains_sub(const unsigned char* buf, size_t buf_len, const unsigned char* pat, size_t pat_len) {
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

// Normalize helpers (heap copies; caller frees)
static unsigned char* to_lower_copy(const unsigned char* buf, size_t len) {
    unsigned char* out = (unsigned char*) malloc(len ? len : 1);
    if (!out) return NULL;
    for (size_t i=0;i<len;++i) out[i] = (unsigned char) tolower(buf[i]);
    return out;
}
static unsigned char* strip_newlines_copy(const unsigned char* buf, size_t len, size_t* out_len) {
    unsigned char* tmp = (unsigned char*) malloc(len ? len : 1);
    if (!tmp) { *out_len = 0; return NULL; }
    size_t p = 0;
    for (size_t i=0;i<len;++i) {
        if (buf[i] == '\n' || buf[i] == '\r') continue;
        tmp[p++] = buf[i];
    }
    *out_len = p;
    unsigned char* out = (unsigned char*) realloc(tmp, p ? p : 1);
    if (out) return out;
    return tmp;
}
static unsigned char* strip_spaces_copy(const unsigned char* buf, size_t len, size_t* out_len) {
    unsigned char* tmp = (unsigned char*) malloc(len ? len : 1);
    if (!tmp) { *out_len = 0; return NULL; }
    size_t p = 0;
    bool last_space = false;
    for (size_t i=0;i<len;++i) {
        unsigned char c = buf[i];
        if (c==' '||c=='\t'||c=='\r'||c=='\n') {
            if (!last_space) { tmp[p++] = ' '; last_space = true; }
        } else {
            tmp[p++] = c; last_space = false;
        }
    }
    // trim
    size_t start = 0; while (start < p && tmp[start]==' ') start++;
    size_t end = p; while (end > start && tmp[end-1]==' ') end--;
    size_t outp = (end>start) ? (end-start) : 0;
    unsigned char* out = (unsigned char*) malloc(outp ? outp : 1);
    if (!out) { free(tmp); *out_len = 0; return NULL; }
    if (outp) memcpy(out, tmp + start, outp);
    free(tmp);
    *out_len = outp;
    return out;
}

// preview printing
static void print_preview(const unsigned char* buf, size_t len) {
    size_t dump = len < 160 ? len : 160;
    for (size_t i=0;i<dump;++i) {
        unsigned char c = buf[i];
        if (c >= 32 && c <= 126) putchar(c);
        else printf("\\x%02X", c);
    }
    if (len > dump) printf("...");
    printf("\n");
}

// -------------------------
// Main brute force loop (integrado en main para simplicidad)
// -------------------------
int main() {
    printf("¿Cuántos casos quieres procesar? (ej: 1): ");
    int ncases = 0;
    if (scanf("%d", &ncases) != 1 || ncases <= 0) {
        fprintf(stderr, "Número inválido.\n");
        return 1;
    }

    // consumir newline
    int ch = getchar();
    (void)ch;

    for (int caso = 1; caso <= ncases; ++caso) {
        printf("\n=== Caso %d ===\n", caso);
        char fname[256];
        snprintf(fname, sizeof(fname), "Encriptado%d.txt", caso);

        // read file as binary text
        FILE *fi = fopen(fname, "rb");
        if (!fi) {
            // try without .txt
            snprintf(fname, sizeof(fname), "Encriptado%d", caso);
            fi = fopen(fname, "rb");
            if (!fi) {
                fprintf(stderr, "No se encontró %s ni Encriptado%d\n", fname, caso);
                continue;
            }
        }
        fseek(fi, 0, SEEK_END);
        long flen = ftell(fi);
        fseek(fi, 0, SEEK_SET);
        unsigned char *raw = (unsigned char*) malloc(flen > 0 ? flen : 1);
        if (!raw) { fclose(fi); fprintf(stderr,"Memoria insuficiente\n"); continue; }
        size_t got = fread(raw, 1, flen > 0 ? flen : 0, fi);
        fclose(fi);

        // decode ZR -> bytes
        unsigned char *decoded = NULL;
        size_t decoded_len = 0;
        decodeZR_to_buffer(raw, got, &decoded, &decoded_len);
        free(raw);

        if (!decoded || decoded_len == 0) {
            fprintf(stderr, "Decodificación vacía para %s\n", fname);
            if (decoded) free(decoded);
            continue;
        }

        // Construimos arrays de bloques (1 bloque)
        unsigned char **bloques = (unsigned char**) malloc(sizeof(unsigned char*));
        size_t *tamanios = (size_t*) malloc(sizeof(size_t));
        bloques[0] = decoded;
        tamanios[0] = decoded_len;
        size_t num_bloques = 1;

        // Leer pista desde pistaX.txt
        char pfname[256];
        snprintf(pfname, sizeof(pfname), "pista%d.txt", caso);
        FILE *pf = fopen(pfname, "rb");
        if (!pf) {
            snprintf(pfname, sizeof(pfname), "pista%d", caso);
            pf = fopen(pfname, "rb");
            if (!pf) {
                fprintf(stderr, "No se pudo abrir la pista %s ni pista%d\n", pfname, caso);
                // liberar bloques
                free(bloques); free(tamanios); free(decoded);
                continue;
            }
        }
        fseek(pf, 0, SEEK_END);
        long plen = ftell(pf);
        fseek(pf, 0, SEEK_SET);
        unsigned char *pbuf = (unsigned char*) malloc(plen > 0 ? plen : 1);
        size_t pgot = fread(pbuf, 1, plen > 0 ? plen : 0, pf);
        fclose(pf);
        size_t p_len = pgot;

        // Normalize pista: remove trailing newline (common)
        while (p_len > 0 && (pbuf[p_len-1] == '\n' || pbuf[p_len-1] == '\r')) p_len--;

        // Fuerza bruta: variantes 0..3
        bool encontrado = false;
        int intentos = 0;

        for (int variante = 0; variante < 4 && !encontrado; ++variante) {
            // variante 0: XOR->ROT
            // variante 1: ROT->XOR
            // variante 2: Solo XOR
            // variante 3: Solo ROT
            for (int n = 0; n <= 7 && !encontrado; ++n) {
                for (int K = 0; K <= 255 && !encontrado; ++K) {
                    ++intentos;
                    if ((intentos & 0x3FF) == 0) {
                        printf("Progreso: var=%d n=%d K=%d (intentos=%d)\n", variante, n, K, intentos);
                    }

                    unsigned char **s1 = NULL;
                    unsigned char **s2 = NULL;

                    if (variante == 0) {
                        s1 = desencriptar_xor(bloques, tamanios, num_bloques, (unsigned char)K);
                        if (!s1) continue;
                        s2 = desencriptar_rotacion(s1, tamanios, num_bloques, n);
                        liberar_bloques_desencriptados(s1, num_bloques);
                        if (!s2) continue;
                    } else if (variante == 1) {
                        s1 = desencriptar_rotacion(bloques, tamanios, num_bloques, n);
                        if (!s1) continue;
                        s2 = desencriptar_xor(s1, tamanios, num_bloques, (unsigned char)K);
                        liberar_bloques_desencriptados(s1, num_bloques);
                        if (!s2) continue;
                    } else if (variante == 2) {
                        s2 = desencriptar_xor(bloques, tamanios, num_bloques, (unsigned char)K);
                        if (!s2) continue;
                    } else {
                        s2 = desencriptar_rotacion(bloques, tamanios, num_bloques, n);
                        if (!s2) continue;
                    }

                    // intentar descomprimir
                    size_t salida_len = 0;
                    char metodo_out[16] = {0};
                    unsigned char *salida = descomprimir_auto(s2, tamanios, num_bloques, salida_len, metodo_out);

                    liberar_bloques_desencriptados(s2, num_bloques);

                    if (!salida || salida_len == 0) {
                        if (salida) free(salida);
                        continue;
                    }

                    // Print preview
                    const char *varstr = (variante==0)?"XOR->ROT":(variante==1)?"ROT->XOR":(variante==2)?"Solo XOR":"Solo ROT";
                    printf("var=%s n=%d K=%d metodo=%s preview: ", varstr, n, K, metodo_out);
                    print_preview(salida, salida_len);

                    // Comparaciones: exacta
                    if (contains_sub(salida, salida_len, pbuf, p_len)) {
                        // guardar resultado final
                        char outname[256];
                        snprintf(outname, sizeof(outname), "Resultado_Caso%d_%s_n%d_K%02X_%s.txt",
                                 caso, varstr, n, K, metodo_out);
                        FILE *fo = fopen(outname, "wb");
                        if (fo) { fwrite(salida,1,salida_len,fo); fclose(fo); }
                        printf("[ENCONTRADO exact] Caso %d -> %s n=%d K=0x%02X Metodo=%s Archivo=%s\n",
                               caso, varstr, n, K, metodo_out, outname);
                        encontrado = true;
                    }
                    if (encontrado) { free(salida); break; }

                    // lowercase
                    unsigned char *s_low = to_lower_copy(salida, salida_len);
                    unsigned char *p_low = to_lower_copy(pbuf, p_len);
                    if (s_low && p_low) {
                        if (contains_sub(s_low, salida_len, p_low, p_len)) {
                            char outname[256];
                            snprintf(outname, sizeof(outname), "Resultado_Caso%d_%s_n%d_K%02X_%s_lower.txt",
                                     caso, varstr, n, K, metodo_out);
                            FILE *fo = fopen(outname, "wb");
                            if (fo) { fwrite(salida,1,salida_len,fo); fclose(fo); }
                            printf("[ENCONTRADO lower] Caso %d -> %s n=%d K=0x%02X Metodo=%s Archivo=%s\n",
                                   caso, varstr, n, K, metodo_out, outname);
                            encontrado = true;
                        }
                    }
                    if (s_low) free(s_low);
                    if (p_low) free(p_low);
                    if (encontrado) { free(salida); break; }

                    // strip newlines
                    size_t s_sn_len=0, p_sn_len=0;
                    unsigned char *s_sn = strip_newlines_copy(salida, salida_len, &s_sn_len);
                    unsigned char *p_sn = strip_newlines_copy(pbuf, p_len, &p_sn_len);
                    if (s_sn && p_sn) {
                        if (contains_sub(s_sn, s_sn_len, p_sn, p_sn_len)) {
                            char outname[256];
                            snprintf(outname, sizeof(outname), "Resultado_Caso%d_%s_n%d_K%02X_%s_stripNL.txt",
                                     caso, varstr, n, K, metodo_out);
                            FILE *fo = fopen(outname, "wb");
                            if (fo) { fwrite(salida,1,salida_len,fo); fclose(fo); }
                            printf("[ENCONTRADO stripNL] Caso %d -> %s n=%d K=0x%02X Metodo=%s Archivo=%s\n",
                                   caso, varstr, n, K, metodo_out, outname);
                            encontrado = true;
                        }
                    }
                    if (s_sn) free(s_sn);
                    if (p_sn) free(p_sn);
                    if (encontrado) { free(salida); break; }

                    // strip & collapse spaces
                    size_t s_sp_len=0, p_sp_len=0;
                    unsigned char *s_sp = strip_spaces_copy(salida, salida_len, &s_sp_len);
                    unsigned char *p_sp = strip_spaces_copy(pbuf, p_len, &p_sp_len);
                    if (s_sp && p_sp) {
                        if (contains_sub(s_sp, s_sp_len, p_sp, p_sp_len)) {
                            char outname[256];
                            snprintf(outname, sizeof(outname), "Resultado_Caso%d_%s_n%d_K%02X_%s_stripSpace.txt",
                                     caso, varstr, n, K, metodo_out);
                            FILE *fo = fopen(outname, "wb");
                            if (fo) { fwrite(salida,1,salida_len,fo); fclose(fo); }
                            printf("[ENCONTRADO stripSpace] Caso %d -> %s n=%d K=0x%02X Metodo=%s Archivo=%s\n",
                                   caso, varstr, n, K, metodo_out, outname);
                            encontrado = true;
                        }
                    }
                    if (s_sp) free(s_sp);
                    if (p_sp) free(p_sp);
                    if (encontrado) { free(salida); break; }

                    // no match: liberar salida
                    free(salida);
                } // for K
            } // for n
        } // for variante

        // liberar estructuras locales
        free(bloques);
        free(tamanios);
        free(decoded);
        free(pbuf);

        if (!encontrado) {
            printf("No se encontró la pista para el caso %d.\n", caso);
        } else {
            printf("Pista encontrada para el caso %d (ver archivo Resultado_Caso...)\n", caso);
        }
    } // for casos

    printf("\nProceso finalizado.\n");
    return 0;
}

