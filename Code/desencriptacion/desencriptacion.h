#ifndef DESENCRIPTACION_H
#define DESENCRIPTACION_H

#include <cstddef>

// Funciones libres (sin clase)
unsigned char** desencriptar_xor(unsigned char **bloques, size_t *tamanios, size_t num_bloques, unsigned char clave);
unsigned char** desencriptar_rotacion(unsigned char **bloques, size_t *tamanios, size_t num_bloques, int n);
void liberar_bloques_desencriptados(unsigned char **bloques, size_t num_bloques);

#endif // DESENCRIPTACION_H
