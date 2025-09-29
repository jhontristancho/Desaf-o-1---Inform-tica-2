
// desencriptacion.cpp
#include <cstddef>
#include <new>  // std::nothrow

                             // XOR simple: un byte de entrada -> un byte de salida
                             unsigned char** desencriptar_xor(unsigned char **bloques, size_t *tamanios,
                                              size_t num_bloques, unsigned char clave) {
    if (!bloques || !tamanios || num_bloques == 0) return nullptr;

    unsigned char **resultado = new unsigned char*[num_bloques];
    for (size_t i = 0; i < num_bloques; i++) {
        resultado[i] = new unsigned char[tamanios[i]];
        for (size_t j = 0; j < tamanios[i]; j++) {
            unsigned char b = bloques[i][j];      // <-- garantizamos byte limpio
            resultado[i][j] = (unsigned char)(b ^ clave);
        }
    }
    return resultado;
}

// Rotación de bits a la derecha: un byte de entrada -> un byte de salida
static inline unsigned char rotar_derecha(unsigned char b, int n) {
    n &= 7; // aseguramos que n esté entre 0 y 7
    return (unsigned char)(((b >> n) | (b << (8 - n))) & 0xFF);
}

unsigned char** desencriptar_rotacion(unsigned char **bloques, size_t *tamanios,
                                      size_t num_bloques, int n) {
    if (!bloques || !tamanios || num_bloques == 0) return nullptr;

    unsigned char **resultado = new unsigned char*[num_bloques];
    for (size_t i = 0; i < num_bloques; i++) {
        resultado[i] = new unsigned char[tamanios[i]];
        for (size_t j = 0; j < tamanios[i]; j++) {
            unsigned char b = bloques[i][j];
            // rotación a la derecha de n bits
            unsigned char r = (unsigned char)((b >> n) | (b << (8 - n)));
            resultado[i][j] = r;  // ahora es siempre un byte válido
        }
    }
    return resultado;
}

// Liberar bloques generados por las desencriptaciones
void liberar_bloques_desencriptados(unsigned char **bloques, size_t num_bloques) {
    if (!bloques) return;
    for (size_t i = 0; i < num_bloques; i++) {
        delete[] bloques[i];
    }
    delete[] bloques;
}

