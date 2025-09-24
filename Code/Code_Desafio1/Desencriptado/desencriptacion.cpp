#include "desencriptacion.h"
#include <cstddef>

//utiliza los mismos parametros que resive de el leer archivo mas la respectiva clave del xor
unsigned char** desencriptar_xor(unsigned char **bloques, size_t *tamanios, size_t num_bloques, unsigned char clave){
    //creamos un arreglo de punteros con el mismo tamaño que el original
    unsigned char **resultado = new unsigned char*[num_bloques];
    //reservamos la memoria para cada bloque con el mismo tamaño que la original
    for (size_t i = 0; i < num_bloques; i++) {
        resultado[i] = new unsigned char[tamanios[i]];
        //aplicamos xor entre el byte encriptado y la clave
        for (size_t j = 0; j < tamanios[i]; j++) {
            resultado[i][j] = bloques[i][j] ^ clave;
        }
    }
    return resultado;
}
//como parametros utilizamos los mismos criterios que el anterior
unsigned char** desencriptar_rotacion(unsigned char **bloques, size_t *tamanios, size_t num_bloques, int n) {
    unsigned char **resultado = new unsigned char*[num_bloques];
    for (size_t i = 0; i < num_bloques; i++) {
        resultado[i] = new unsigned char[tamanios[i]];
        for (size_t j = 0; j < tamanios[i]; j++) {
            unsigned char byte = bloques[i][j];
            //Tomamos el byte encriptado del bloque original.
            resultado[i][j] = (byte >> n) | (byte << (8 - n));
            //aplicamos la respectiva rotacion
        }
    }
    return resultado;
}

void liberar_bloques_desencriptados(unsigned char **bloques, size_t num_bloques) {
    if (bloques) {
        for (size_t i = 0; i < num_bloques; i++) {
            if (bloques[i]) delete[] bloques[i];  // Liberar cada bloque
        }
        delete[] bloques;  // Liberar el array de punteros
    }
}

