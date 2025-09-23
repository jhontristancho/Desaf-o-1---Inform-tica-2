#include <iostream>
#include <fstream>
using std::cout;
using std::cerr;
using std::cin;
using std::endl;
const size_t TAM_BLOQUE = 8192UL; // 8 KiB por bloque, 8192 bytes tambien
const size_t CAPACIDAD_INICIAL_LISTAS = 8UL; // // capacidad inicial para arrays de punteros, no usaremos el mismo de bloques porque requiere de menos espacio de memoria
// basicamente lo que se realizara es darle el nombre del archivo y el programa lo lee y lo guarda el un arreglo dinamico.

bool abrir_y_leer_en_bloques(const char *ruta,
                             unsigned char ***bloques_out,//puntero de arreglos
                             size_t **tamanios_out,
                             size_t &num_bloques_out,
                             size_t &total_bytes_out) {//este es el total de bytes leidos
    if (!ruta) return false;
    *bloques_out = nullptr;
    *tamanios_out = nullptr;
    num_bloques_out = 0;
    total_bytes_out = 0;
    std::ifstream archivo;
    archivo.open(ruta, std::ios::in | std::ios::binary);
    if (!archivo.is_open()) {
        // hay que asegurarnos que la ruta que nos de el usuario sea correcta o exista
        return false;
    }
    // iniciiarlizar los arreglos dinamicos, usar el tipo de dato size_t es mejor porque nos da mas garantias con los arreglos
    size_t capacidad_listas = CAPACIDAD_INICIAL_LISTAS;
    unsigned char **bloques = new unsigned char*[capacidad_listas]; //arreglos dinamicos, con new
    size_t *tamanios = new size_t[capacidad_listas];
    if (!bloques || !tamanios) {
        if (bloques) delete[] bloques;
        if (tamanios) delete[] tamanios;
        archivo.close();
        return false;
    }
    size_t num_bloques = 0;
    size_t total_leidos = 0;
    while (true) {
        // reservar bloque fijo
        unsigned char *bloque = new unsigned char[TAM_BLOQUE];
        if (!bloque) {
            // limpieza en caso de fallo de new, eso evita las fugas de memoria
            for (size_t i = 0; i < num_bloques; ++i) delete[] bloques[i];
            delete[] bloques;
            delete[] tamanios;
            archivo.close();
            return false;
        }
        // leer hasta TAM_BLOQUE bytes
        archivo.read(reinterpret_cast<char*>(bloque), static_cast<std::streamsize>(TAM_BLOQUE));
        std::streamsize leidos = archivo.gcount();
        if (leidos <= 0) {
            // si no lee nada, limpia y rompe
            delete[] bloque;
            break;
        }
        size_t leidos_sz = static_cast<size_t>(leidos);
        // esto es en algun caso donde necesitemos mas espacio para listar los bloques, lo doblamos
        if (num_bloques >= capacidad_listas) {
            size_t nueva_cap = capacidad_listas * 2;
            unsigned char **nuevos_bloques = new unsigned char*[nueva_cap];
            size_t *nuevos_tamanios = new size_t[nueva_cap];
            if (!nuevos_bloques || !nuevos_tamanios) {
                // limpieza si falla la ampliación
                delete[] bloque;
                for (size_t i = 0; i < num_bloques; ++i) delete[] bloques[i];
                delete[] bloques;
                delete[] tamanios;
                if (nuevos_bloques) delete[] nuevos_bloques;
                if (nuevos_tamanios) delete[] nuevos_tamanios;
                archivo.close();
                return false;
            }
            // copiar punteros y tamaños a las nuevas listas
            for (size_t i = 0; i < num_bloques; ++i) {
                nuevos_bloques[i] = bloques[i];
                nuevos_tamanios[i] = tamanios[i];
            }
            delete[] bloques;
            delete[] tamanios;
            bloques = nuevos_bloques;
            tamanios = nuevos_tamanios;
            capacidad_listas = nueva_cap;
        }
        // guardar el bloque y su tamaño real, lo de su tamaño es por ahora para hcer pruebas a ver cuanto espacio estan ocupando
        bloques[num_bloques] = bloque;
        tamanios[num_bloques] = leidos_sz;
        ++num_bloques;
        total_leidos += leidos_sz;
    }
    archivo.close();
    *bloques_out = bloques;
    *tamanios_out = tamanios;
    num_bloques_out = num_bloques;
    total_bytes_out = total_leidos;
    return true;
}

// Libera todas las estructuras asignadas por abrir_y_leer_en_bloques
void liberar_bloques(unsigned char **bloques, size_t *tamanios, size_t num_bloques) {
    if (bloques) {
        for (size_t i = 0; i < num_bloques; ++i) {
            if (bloques[i]) delete[] bloques[i];
        }
        delete[] bloques;
    }
    if (tamanios) delete[] tamanios;
}
