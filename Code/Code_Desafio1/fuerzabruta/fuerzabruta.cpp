#include <iostream>
#include <cstring>
#include <cstddef>

// Declaraciones de funciones externas
extern unsigned char* descomprimir_RLE(unsigned char **bloques, size_t *tamanios, size_t num_bloques, size_t &salida_len_out);
extern unsigned char* descomprimir_LZ78(unsigned char **bloques, size_t *tamanios, size_t num_bloques, size_t &salida_len_out);
extern bool abrir_y_leer_en_bloques(const char *ruta, unsigned char ***bloques_out, size_t **tamanios_out, size_t &num_bloques_out, size_t &total_bytes_out);
extern void liberar_bloques(unsigned char **bloques, size_t *tamanios, size_t num_bloques);
extern unsigned char** desencriptar_xor(unsigned char **bloques, size_t *tamanios, size_t num_bloques, unsigned char clave);
extern unsigned char** desencriptar_rotacion(unsigned char **bloques, size_t *tamanios, size_t num_bloques, int n);
extern void liberar_bloques_desencriptados(unsigned char **bloques, size_t num_bloques);

// Función para comparar si un buffer contiene una pista específica
static bool contiene_pista(const unsigned char *buffer, size_t buffer_len,
                           const unsigned char *pista, size_t pista_len) {
    if (!buffer || !pista || pista_len == 0) return false;
    if (buffer_len < pista_len) return false;

    for (size_t i = 0; i <= buffer_len - pista_len; i++) {
        bool coincide = true;
        for (size_t j = 0; j < pista_len; j++) {
            if (buffer[i + j] != pista[j]) {
                coincide = false;
                break;
            }
        }
        if (coincide) return true;
    }
    return false;
}

// Función para cargar un archivo de texto como buffer de bytes
static bool cargar_archivo_texto(const char *ruta, unsigned char *&buffer, size_t &tamano) {
    unsigned char **bloques = nullptr;
    size_t *tamanios = nullptr;
    size_t num_bloques = 0;
    size_t total_bytes = 0;

    if (!abrir_y_leer_en_bloques(ruta, &bloques, &tamanios, num_bloques, total_bytes)) {
        return false;
    }

    // Consolidar todos los bloques en un solo buffer
    buffer = new unsigned char[total_bytes + 1];
    if (!buffer) {
        liberar_bloques(bloques, tamanios, num_bloques);
        return false;
    }

    size_t offset = 0;
    for (size_t i = 0; i < num_bloques; i++) {
        for (size_t j = 0; j < tamanios[i]; j++) {
            buffer[offset++] = bloques[i][j];
        }
    }
    buffer[total_bytes] = '\0';
    tamano = total_bytes;

    liberar_bloques(bloques, tamanios, num_bloques);
    return true;
}

// Función principal de fuerza bruta
void fuerza_bruta() {
    std::cout << "=== FUERZA BRUTA - PROBANDO TODAS LAS COMBINACIONES ===" << std::endl;

    // Rutas de los archivos
    const char *ruta_encriptado = "C:\\Users\\mat23\\OneDrive\\Documentos\\repositorio desafio uno\\Desaf-o-1---Inform-tica-2\\Code\\Code_Desafio1\\Encriptado1.txt";
    const char *ruta_pista = "C:\\Users\\mat23\\OneDrive\\Documentos\\repositorio desafio uno\\Desaf-o-1---Inform-tica-2\\Code\\Code_Desafio1\\pista1.txt";

    std::cout << "Archivo encriptado: " << ruta_encriptado << std::endl;
    std::cout << "Archivo pista: " << ruta_pista << std::endl;

    // Cargar archivo encriptado
    unsigned char **bloques_encriptados = nullptr;
    size_t *tamanios_encriptados = nullptr;
    size_t num_bloques_encriptados = 0;
    size_t total_bytes_encriptados = 0;

    if (!abrir_y_leer_en_bloques(ruta_encriptado, &bloques_encriptados,
                                 &tamanios_encriptados, num_bloques_encriptados,
                                 total_bytes_encriptados)) {
        std::cout << "Error: No se pudo cargar el archivo encriptado" << std::endl;
        return;
    }

    // Cargar pista
    unsigned char *pista_buffer = nullptr;
    size_t pista_tamano = 0;
    if (!cargar_archivo_texto(ruta_pista, pista_buffer, pista_tamano)) {
        std::cout << "Error: No se pudo cargar la pista" << std::endl;
        liberar_bloques(bloques_encriptados, tamanios_encriptados, num_bloques_encriptados);
        return;
    }

    std::cout << "Tamaño archivo encriptado: " << total_bytes_encriptados << " bytes" << std::endl;
    std::cout << "Tamaño pista: " << pista_tamano << " bytes" << std::endl;
    std::cout << "Pista buscada: ";
    for (size_t i = 0; i < pista_tamano; i++) {
        std::cout << pista_buffer[i];
    }
    std::cout << std::endl;
    std::cout << "Combinaciones a probar: " << (2 * 254 * 7 * 2) << std::endl;
    std::cout << "==========================================" << std::endl;

    int combinaciones_probadas = 0;
    int combinaciones_exitosas = 0;

    // Probar todas las combinaciones
    for (int orden = 0; orden < 2; orden++) {
        std::cout << "\n--- Probando orden: " << (orden == 0 ? "XOR->ROTACION" : "ROTACION->XOR") << " ---" << std::endl;

        for (unsigned char clave_xor = 1; clave_xor < 255; clave_xor++) {
            for (int rotacion = 1; rotacion < 8; rotacion++) {
                for (int metodo_compresion = 0; metodo_compresion < 2; metodo_compresion++) {
                    combinaciones_probadas++;

                    if (combinaciones_probadas % 1000 == 0) {
                        std::cout << "Probadas: " << combinaciones_probadas << " combinaciones..." << std::endl;
                    }

                    unsigned char **bloques_intermedios = nullptr;
                    size_t salida_len = 0;
                    unsigned char *resultado_descomprimido = nullptr;
                    bool exito = false;

                    try {
                        // Aplicar desencriptación según el orden
                        if (orden == 0) {
                            // XOR primero, luego rotación
                            bloques_intermedios = desencriptar_xor(bloques_encriptados,
                                                                   tamanios_encriptados,
                                                                   num_bloques_encriptados,
                                                                   clave_xor);
                            if (!bloques_intermedios) continue;

                            unsigned char **bloques_desencriptados =
                                desencriptar_rotacion(bloques_intermedios,
                                                      tamanios_encriptados,
                                                      num_bloques_encriptados,
                                                      rotacion);
                            liberar_bloques_desencriptados(bloques_intermedios, num_bloques_encriptados);

                            if (!bloques_desencriptados) continue;

                            // Aplicar descompresión
                            if (metodo_compresion == 0) {
                                resultado_descomprimido = descomprimir_RLE(bloques_desencriptados,
                                                                           tamanios_encriptados,
                                                                           num_bloques_encriptados,
                                                                           salida_len);
                            } else {
                                resultado_descomprimido = descomprimir_LZ78(bloques_desencriptados,
                                                                            tamanios_encriptados,
                                                                            num_bloques_encriptados,
                                                                            salida_len);
                            }

                            liberar_bloques_desencriptados(bloques_desencriptados, num_bloques_encriptados);

                        } else {
                            // Rotación primero, luego XOR
                            bloques_intermedios = desencriptar_rotacion(bloques_encriptados,
                                                                        tamanios_encriptados,
                                                                        num_bloques_encriptados,
                                                                        rotacion);
                            if (!bloques_intermedios) continue;

                            unsigned char **bloques_desencriptados =
                                desencriptar_xor(bloques_intermedios,
                                                 tamanios_encriptados,
                                                 num_bloques_encriptados,
                                                 clave_xor);
                            liberar_bloques_desencriptados(bloques_intermedios, num_bloques_encriptados);

                            if (!bloques_desencriptados) continue;

                            // Aplicar descompresión
                            if (metodo_compresion == 0) {
                                resultado_descomprimido = descomprimir_RLE(bloques_desencriptados,
                                                                           tamanios_encriptados,
                                                                           num_bloques_encriptados,
                                                                           salida_len);
                            } else {
                                resultado_descomprimido = descomprimir_LZ78(bloques_desencriptados,
                                                                            tamanios_encriptados,
                                                                            num_bloques_encriptados,
                                                                            salida_len);
                            }

                            liberar_bloques_desencriptados(bloques_desencriptados, num_bloques_encriptados);
                        }

                        // Verificar si la descompresión fue exitosa
                        if (resultado_descomprimido && salida_len > 0) {
                            // Verificar si contiene la pista
                            bool contiene = contiene_pista(resultado_descomprimido, salida_len,
                                                           pista_buffer, pista_tamano);

                            if (contiene) {
                                combinaciones_exitosas++;
                                std::cout << "\n*** COMBINACION EXITOSA #" << combinaciones_exitosas << " ***" << std::endl;
                                std::cout << "Orden: " << (orden == 0 ? "XOR->ROTACION" : "ROTACION->XOR") << std::endl;
                                std::cout << "Clave XOR: " << (int)clave_xor << std::endl;
                                std::cout << "Rotacion: " << rotacion << " bits" << std::endl;
                                std::cout << "Compresion: " << (metodo_compresion == 0 ? "RLE" : "LZ78") << std::endl;
                                std::cout << "Tamaño: " << salida_len << " bytes" << std::endl;

                                // Mostrar primeros caracteres del resultado
                                std::cout << "Primeros 100 caracteres: ";
                                for (size_t i = 0; i < salida_len && i < 100; i++) {
                                    std::cout << resultado_descomprimido[i];
                                }
                                std::cout << std::endl;

                                // Guardar resultado en archivo
                                char nombre_archivo[100];
                                snprintf(nombre_archivo, sizeof(nombre_archivo),
                                         "resultado_%d_%d_%d_%s.txt",
                                         orden, (int)clave_xor, rotacion,
                                         metodo_compresion == 0 ? "RLE" : "LZ78");

                                FILE *archivo = fopen(nombre_archivo, "wb");
                                if (archivo) {
                                    fwrite(resultado_descomprimido, 1, salida_len, archivo);
                                    fclose(archivo);
                                    std::cout << "Guardado en: " << nombre_archivo << std::endl;
                                }

                                std::cout << "------------------------------------------" << std::endl;
                                exito = true;
                            }

                            // Mostrar TODAS las combinaciones
                            std::cout << "Combinacion " << combinaciones_probadas << ": ";
                            std::cout << "Orden=" << (orden == 0 ? "XOR->ROT" : "ROT->XOR");
                            std::cout << ", XOR=" << (int)clave_xor;
                            std::cout << ", Rot=" << rotacion;
                            std::cout << ", Comp=" << (metodo_compresion == 0 ? "RLE" : "LZ78");
                            std::cout << ", Tamaño=" << salida_len;
                            std::cout << ", Pista=" << (contiene ? "SI" : "NO");
                            std::cout << std::endl;

                            if (!exito && salida_len > 0) {
                                std::cout << "  Preview: ";
                                for (size_t i = 0; i < salida_len && i < 30; i++) {
                                    if (resultado_descomprimido[i] >= 32 && resultado_descomprimido[i] <= 126) {
                                        std::cout << resultado_descomprimido[i];
                                    } else {
                                        std::cout << "?";
                                    }
                                }
                                std::cout << std::endl;
                            }
                        }

                        if (resultado_descomprimido) {
                            delete[] resultado_descomprimido;
                        }

                    } catch (...) {
                        if (bloques_intermedios) {
                            liberar_bloques_desencriptados(bloques_intermedios, num_bloques_encriptados);
                        }
                        if (resultado_descomprimido) {
                            delete[] resultado_descomprimido;
                        }

                        std::cout << "Combinacion " << combinaciones_probadas << ": ERROR en procesamiento" << std::endl;
                    }
                }
            }
        }
    }

    std::cout << "\n==========================================" << std::endl;
    std::cout << "=== RESULTADOS FINALES ===" << std::endl;
    std::cout << "Combinaciones probadas: " << combinaciones_probadas << std::endl;
    std::cout << "Combinaciones exitosas: " << combinaciones_exitosas << std::endl;
    std::cout << "==========================================" << std::endl;

    // Limpieza final
    delete[] pista_buffer;
    liberar_bloques(bloques_encriptados, tamanios_encriptados, num_bloques_encriptados);
}


