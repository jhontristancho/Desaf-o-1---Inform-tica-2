#include <iostream>
#include <cstring>
#include <stddef.h>   // size_t
const size_t OUT_BUF_INICIAL = 8192UL;
const size_t MAX_SALIDA = 300UL * 1024UL * 1024UL; // 300 MiB
const size_t DICT_CAP_INICIAL = 256UL;
const size_t TAM_MAX = (size_t)(-1);          // valores maximos
const unsigned long MAX_ULONG = (unsigned long)(-1);

static bool es_espacio(unsigned char c) {
    return (c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == '\v' || c == '\f');
}


static void asignar_metodo(char metodo_out[16], const char *s) {
    if (!metodo_out) return;
    int i = 0;
    while (i < 15 && s[i] != '\0') {
        metodo_out[i] = s[i];
        ++i;
    }
    // completar con ceros
    for (int j = i; j < 16; ++j) metodo_out[j] = 0;
}

static bool obtener_byte_con_cursor(unsigned char **bloques,
                                    size_t *tamanios,
                                    size_t num_bloques,
                                    size_t &idx_bloque,
                                    size_t &offset,
                                    unsigned char &out_byte) {
    while (idx_bloque < num_bloques && offset >= tamanios[idx_bloque]) {
        ++idx_bloque;
        offset = 0;
    }
    if (idx_bloque >= num_bloques) return false;
    out_byte = bloques[idx_bloque][offset];
    ++offset;
    return true;
}

static bool retroceder_un_byte(unsigned char ** /*bloques*/,
                               size_t *tamanios,
                               size_t num_bloques,
                               size_t &idx_bloque,
                               size_t &offset) {
    if (idx_bloque >= num_bloques && num_bloques > 0) {
        idx_bloque = num_bloques - 1;
        offset = (tamanios[idx_bloque] == 0) ? 0 : tamanios[idx_bloque];
    }
    if (idx_bloque == 0 && offset == 0) return false;
    if (offset > 0) {
        --offset;
        return true;
    } else {
        if (idx_bloque == 0) return false;
        --idx_bloque;
        offset = (tamanios[idx_bloque] == 0) ? 0 : (tamanios[idx_bloque] - 1);
        return true;
    }
}
static unsigned char* crear_buffer(size_t capacidad) {
    if (capacidad == 0) capacidad = 1;
    unsigned char *p = new unsigned char[capacidad];
    return p;
}
static bool asegurar_capacidad(unsigned char *&buf, size_t &cap, size_t requerido) {
    if (requerido <= cap) return true;
    size_t nueva = (cap == 0) ? 1 : cap;
    // doblar cuidando overflow sin usar SIZE_MAX macro
    while (nueva < requerido) {
        if (nueva > (TAM_MAX / 2)) { // evitar overflow
            nueva = requerido;
            break;
        } else {
            nueva *= 2;
        }
    }
    unsigned char *nuevo = new unsigned char[nueva];
    if (!nuevo) return false;
    for (size_t i = 0; i < cap; ++i) nuevo[i] = buf[i];
    delete[] buf;
    buf = nuevo;
    cap = nueva;
    return true;
}

unsigned char* descomprimir_RLE(unsigned char **bloques,
                                size_t *tamanios,
                                size_t num_bloques,
                                size_t &salida_len_out) {
    salida_len_out = 0;
    if (!bloques || !tamanios || num_bloques == 0) return nullptr;

    size_t idx = 0, off = 0;
    unsigned char *outbuf = crear_buffer(OUT_BUF_INICIAL);
    if (!outbuf) return nullptr;
    size_t cap_out = OUT_BUF_INICIAL;
    size_t out_len = 0;

    unsigned long acc = 0;
    bool leyendo_num = false;
    bool any_read = false;
    unsigned char b;

    while (true) {
        if (!obtener_byte_con_cursor(bloques, tamanios, num_bloques, idx, off, b)) break;
        any_read = true;
        if (b >= '0' && b <= '9') {
            leyendo_num = true;
            if (acc > (MAX_ULONG / 10UL)) { delete[] outbuf; return nullptr; }
            acc = acc * 10UL + (unsigned long)(b - '0');
            if ((size_t)acc > MAX_SALIDA) { delete[] outbuf; return nullptr; }
        } else {
            if (!leyendo_num) { delete[] outbuf; return nullptr; }
            size_t count = (size_t) acc;
            if (out_len + count > MAX_SALIDA) { delete[] outbuf; return nullptr; }
            size_t requerido = out_len + count;
            if (!asegurar_capacidad(outbuf, cap_out, requerido)) { delete[] outbuf; return nullptr; }
            for (size_t k = 0; k < count; ++k) outbuf[out_len + k] = b;
            out_len += count;
            acc = 0;
            leyendo_num = false;
        }
    }

    if (!any_read) { delete[] outbuf; return nullptr; }
    if (leyendo_num) { delete[] outbuf; return nullptr; } // número sin símbolo

    salida_len_out = out_len;
    return outbuf;
}

unsigned char* descomprimir_LZ78(unsigned char **bloques,
                                 size_t *tamanios,
                                 size_t num_bloques,
                                 size_t &salida_len_out) {
    salida_len_out = 0;
    if (!bloques || !tamanios || num_bloques == 0) return nullptr;

    size_t dict_cap = DICT_CAP_INICIAL;
    unsigned char **dict = new unsigned char*[dict_cap];
    size_t *dict_lens = new size_t[dict_cap];
    if (!dict || !dict_lens) {
        if (dict) delete[] dict;
        if (dict_lens) delete[] dict_lens;
        return nullptr;
    }
    size_t dict_size = 0;

    unsigned char *outbuf = crear_buffer(OUT_BUF_INICIAL);
    if (!outbuf) { delete[] dict; delete[] dict_lens; return nullptr; }
    size_t cap_out = OUT_BUF_INICIAL;
    size_t out_len = 0;

    size_t idx = 0, off = 0;
    unsigned char ch;
    bool any_token = false;

    while (true) {
        while (true) {
            if (!obtener_byte_con_cursor(bloques, tamanios, num_bloques, idx, off, ch)) break;
            if (!es_espacio(ch)) {
                if (!retroceder_un_byte(bloques, tamanios, num_bloques, idx, off)) {
                    for (size_t i = 0; i < dict_size; ++i) delete[] dict[i];
                    delete[] dict; delete[] dict_lens; delete[] outbuf;
                    return nullptr;
                }
                break;
            }
        }

        unsigned long acc = 0;
        bool any_digit = false;
        while (true) {
            if (!obtener_byte_con_cursor(bloques, tamanios, num_bloques, idx, off, ch)) break;
            if (ch >= '0' && ch <= '9') {
                any_digit = true;
                if (acc > (MAX_ULONG / 10UL)) {
                    for (size_t i = 0; i < dict_size; ++i) delete[] dict[i];
                    delete[] dict; delete[] dict_lens; delete[] outbuf;
                    return nullptr;
                }
                acc = acc * 10UL + (unsigned long)(ch - '0');
            } else {
                if (!retroceder_un_byte(bloques, tamanios, num_bloques, idx, off)) {
                    for (size_t i = 0; i < dict_size; ++i) delete[] dict[i];
                    delete[] dict; delete[] dict_lens; delete[] outbuf;
                    return nullptr;
                }
                break;
            }
        }

        if (!any_digit) {
            break;
        }
        unsigned long index = acc;

        bool found_char = false;
        while (true) {
            if (!obtener_byte_con_cursor(bloques, tamanios, num_bloques, idx, off, ch)) {
                for (size_t i = 0; i < dict_size; ++i) delete[] dict[i];
                delete[] dict; delete[] dict_lens; delete[] outbuf;
                return nullptr;
            }
            if (!es_espacio(ch)) { found_char = true; break; }
        }
        if (!found_char) {
            for (size_t i = 0; i < dict_size; ++i) delete[] dict[i];
            delete[] dict; delete[] dict_lens; delete[] outbuf;
            return nullptr;
        }

        any_token = true;
        unsigned char car = ch;

        unsigned char *nueva = nullptr;
        size_t nueva_len = 0;
        if (index == 0) {
            nueva_len = 1;
            nueva = new unsigned char[1];
            if (!nueva) {
                for (size_t i = 0; i < dict_size; ++i) delete[] dict[i];
                delete[] dict; delete[] dict_lens; delete[] outbuf;
                return nullptr;
            }
            nueva[0] = car;
        } else {
            if (index > dict_size) {
                for (size_t i = 0; i < dict_size; ++i) delete[] dict[i];
                delete[] dict; delete[] dict_lens; delete[] outbuf;
                return nullptr;
            }
            size_t pref_len = dict_lens[index - 1];
            nueva_len = pref_len + 1;
            nueva = new unsigned char[nueva_len];
            if (!nueva) {
                for (size_t i = 0; i < dict_size; ++i) delete[] dict[i];
                delete[] dict; delete[] dict_lens; delete[] outbuf;
                return nullptr;
            }
            for (size_t k = 0; k < pref_len; ++k) nueva[k] = dict[index - 1][k];
            nueva[pref_len] = car;
        }

        if (out_len + nueva_len > MAX_SALIDA) {
            delete[] nueva;
            for (size_t i = 0; i < dict_size; ++i) delete[] dict[i];
            delete[] dict; delete[] dict_lens; delete[] outbuf;
            return nullptr;
        }
        if (!asegurar_capacidad(outbuf, cap_out, out_len + nueva_len)) {
            delete[] nueva;
            for (size_t i = 0; i < dict_size; ++i) delete[] dict[i];
            delete[] dict; delete[] dict_lens; delete[] outbuf;
            return nullptr;
        }
        for (size_t k = 0; k < nueva_len; ++k) outbuf[out_len + k] = nueva[k];
        out_len += nueva_len;

        // añadir al diccionario
        if (dict_size >= dict_cap) {
            size_t nueva_cap = dict_cap * 2;
            if (nueva_cap <= dict_cap) {
                delete[] nueva;
                for (size_t i = 0; i < dict_size; ++i) delete[] dict[i];
                delete[] dict; delete[] dict_lens; delete[] outbuf;
                return nullptr;
            }
            unsigned char **nd = new unsigned char*[nueva_cap];
            size_t *nl = new size_t[nueva_cap];
            if (!nd || !nl) {
                delete[] nueva;
                if (nd) delete[] nd;
                if (nl) delete[] nl;
                for (size_t i = 0; i < dict_size; ++i) delete[] dict[i];
                delete[] dict; delete[] dict_lens; delete[] outbuf;
                return nullptr;
            }
            for (size_t i = 0; i < dict_size; ++i) { nd[i] = dict[i]; nl[i] = dict_lens[i]; }
            delete[] dict; delete[] dict_lens;
            dict = nd; dict_lens = nl; dict_cap = nueva_cap;
        }
        dict[dict_size] = nueva;
        dict_lens[dict_size] = nueva_len;
        ++dict_size;
    } // fin loop

    if (!any_token) {
        delete[] outbuf;
        for (size_t i = 0; i < dict_size; ++i) delete[] dict[i];
        delete[] dict; delete[] dict_lens;
        return nullptr;
    }

    salida_len_out = out_len;
    for (size_t i = 0; i < dict_size; ++i) delete[] dict[i];
    delete[] dict; delete[] dict_lens;
    return outbuf;
}

// ------------------ descomprimir_auto ------------------

unsigned char* descomprimir_auto(unsigned char **bloques,
                                 size_t *tamanios,
                                 size_t num_bloques,
                                 size_t &salida_len,
                                 char metodo_out[16]) {
    salida_len = 0;
    if (metodo_out) {
        for (int i = 0; i < 16; ++i) metodo_out[i] = 0;
    }

    unsigned char *res_rle = descomprimir_RLE(bloques, tamanios, num_bloques, salida_len);
    if (res_rle) {
        if (metodo_out) asignar_metodo(metodo_out, "RLE");
        return res_rle;
    }

    unsigned char *res_lz = descomprimir_LZ78(bloques, tamanios, num_bloques, salida_len);
    if (res_lz) {
        if (metodo_out) asignar_metodo(metodo_out, "LZ78");
        return res_lz;
    }

    if (metodo_out) asignar_metodo(metodo_out, "NINGUNO");
    return nullptr;
}
