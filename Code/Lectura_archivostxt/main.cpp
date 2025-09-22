#include <iostream>
#include <fstream>

using std::cout;
using std::cerr;
using std::endl;

const unsigned long TAM_BLOQUE_DEF = 8192UL; // 8 KiB por bloque

bool leer_archivo_en_bloques(const char *ruta,
                             unsigned char ***bloques_out,
                             unsigned long **tamanios_out,
                             unsigned long &num_bloques_out,
                             unsigned long &total_bytes_out,
                             unsigned long tam_bloque = TAM_BLOQUE_DEF) {
    if (!ruta) return false;
    *bloques_out = nullptr;
    *tamanios_out = nullptr;
    num_bloques_out = 0UL;
    total_bytes_out = 0UL;

    // Abrir archivo en binario
    std::ifstream archivo(ruta, std::ios::in | std::ios::binary);
    if (!archivo.is_open()) {
        cerr << "Error: no se pudo abrir el archivo: " << ruta << endl;
        return false;
    }

    // Arrays dinámicos para punteros y tamaños (empezamos con pequeña capacidad y doblamos)
    unsigned long capacidad_listas = 8UL;
    unsigned char **bloques = new unsigned char*[capacidad_listas];
    unsigned long *tamanios = new unsigned long[capacidad_listas];
    if (!bloques || !tamanios) {
        cerr << "Error: asignacion inicial fallida\n";
        if (bloques) delete[] bloques;
        if (tamanios) delete[] tamanios;
        archivo.close();
        return false;
    }

    unsigned long num_bloques = 0UL;
    unsigned long total_leidos = 0UL;

    while (archivo.good()) {
        // Crear bloque
        unsigned char *bloque = new unsigned char[tam_bloque];
        if (!bloque) {
            cerr << "Error: asignacion de bloque fallida\n";
            // limpiar lo ya asignado
            for (unsigned long i = 0; i < num_bloques; ++i) delete[] bloques[i];
            delete[] bloques;
            delete[] tamanios;
            archivo.close();
            return false;
        }

        // Leer hasta tam_bloque bytes
        archivo.read(reinterpret_cast<char*>(bloque), tam_bloque);
        std::streamsize leidos = archivo.gcount();
        if (leidos <= 0) {
            // EOF o no se leyó nada: liberar el bloque recién creado y salir
            delete[] bloque;
            break;
        }

        unsigned long leidos_ul = static_cast<unsigned long>(leidos);

        // Asegurar espacio en arrays de punteros/tamaños
        if (num_bloques >= capacidad_listas) {
            unsigned long nueva_cap = capacidad_listas * 2UL;
            unsigned char **nuevos_bloques = new unsigned char*[nueva_cap];
            unsigned long *nuevos_tamanios = new unsigned long[nueva_cap];
            if (!nuevos_bloques || !nuevos_tamanios) {
                cerr << "Error: no se pudo ampliar listas de bloques\n";
                // limpieza
                delete[] bloque;
                for (unsigned long i = 0; i < num_bloques; ++i) delete[] bloques[i];
                delete[] bloques;
                delete[] tamanios;
                if (nuevos_bloques) delete[] nuevos_bloques;
                if (nuevos_tamanios) delete[] nuevos_tamanios;
                archivo.close();
                return false;
            }
            // copiar los punteros y tamaños a las nuevas listas
            for (unsigned long i = 0; i < num_bloques; ++i) {
                nuevos_bloques[i] = bloques[i];
                nuevos_tamanios[i] = tamanios[i];
            }
            delete[] bloques;
            delete[] tamanios;
            bloques = nuevos_bloques;
            tamanios = nuevos_tamanios;
            capacidad_listas = nueva_cap;
        }

        // Guardar bloque y su tamaño real
        bloques[num_bloques] = bloque;
        tamanios[num_bloques] = leidos_ul;
        ++num_bloques;
        total_leidos += leidos_ul;
    }

    archivo.close();

    *bloques_out = bloques;
    *tamanios_out = tamanios;
    num_bloques_out = num_bloques;
    total_bytes_out = total_leidos;
    return true;
}

// Libera la memoria asignada por leer_archivo_en_bloques
void liberar_bloques(unsigned char **bloques, unsigned long *tamanios, unsigned long num_bloques) {
    if (bloques) {
        for (unsigned long i = 0; i < num_bloques; ++i) {
            if (bloques[i]) delete[] bloques[i];
        }
        delete[] bloques;
    }
    if (tamanios) delete[] tamanios;
}

// Imprime los primeros 'max_mostrar' bytes en hexadecimal (recorriendo bloques)
void imprimir_primeros_bytes(unsigned char **bloques, unsigned long *tamanios, unsigned long num_bloques, unsigned long max_mostrar) {
    const char *hex = "0123456789ABCDEF";
    unsigned long mostrados = 0UL;
    for (unsigned long b = 0; b < num_bloques && mostrados < max_mostrar; ++b) {
        unsigned char *bloque = bloques[b];
        unsigned long tam = tamanios[b];
        for (unsigned long i = 0; i < tam && mostrados < max_mostrar; ++i) {
            unsigned int v = static_cast<unsigned int>(bloque[i]);
            char h1 = hex[(v >> 4) & 0xF];
            char h2 = hex[v & 0xF];
            cout << h1 << h2 << ' ';
            ++mostrados;
            if (mostrados % 16 == 0) cout << '\n';
        }
    }
    if (mostrados % 16 != 0) cout << '\n';
}

// Lectura aleatoria O(1) si los bloques son de tamaño fijo tam_bloque
// Devuelve -1 si fuera de rango
int leer_byte_posicion(unsigned char **bloques, unsigned long *tamanios, unsigned long num_bloques, unsigned long tam_bloque, unsigned long posicion) {
    if (num_bloques == 0) return -1;
    unsigned long indice = posicion / tam_bloque;
    unsigned long offset = posicion % tam_bloque;
    if (indice >= num_bloques) return -1;
    if (offset >= tamanios[indice]) return -1;
    return static_cast<int>(bloques[indice][offset]);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        cerr << "Uso: " << argv[0] << " ruta_del_archivo.txt" << endl;
        return 1;
    }

    const char *ruta = argv[1];

    unsigned char **bloques = nullptr;
    unsigned long *tamanios = nullptr;
    unsigned long num_bloques = 0UL;
    unsigned long total_bytes = 0UL;

    bool ok = leer_archivo_en_bloques(ruta, &bloques, &tamanios, num_bloques, total_bytes, TAM_BLOQUE_DEF);
    if (!ok) {
        cerr << "Fallo al leer el archivo en bloques.\n";
        return 2;
    }

    cout << "Lectura por bloques completa." << endl;
    cout << "Bloques leídos: " << num_bloques << endl;
    cout << "Bytes totales: " << total_bytes << endl;

    cout << "\nPrimeros bytes (hex):\n";
    imprimir_primeros_bytes(bloques, tamanios, num_bloques, 128UL);

    // Ejemplo de lectura aleatoria:
    if (total_bytes > 0) {
        unsigned long pos = (total_bytes > 10UL) ? 10UL : 0UL;
        int val = leer_byte_posicion(bloques, tamanios, num_bloques, TAM_BLOQUE_DEF, pos);
        if (val >= 0) cout << "Byte en posición " << pos << " = " << val << " (decimal)\n";
    }

    // Liberar toda la memoria
    liberar_bloques(bloques, tamanios, num_bloques);
    return 0;
}
