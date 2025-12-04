#include <stdio.h>
#include <math.h> 
#include <stdlib.h> 
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h" 
#include "freertos/semphr.h"
#include "esp_timer.h" 
#include "esp_task_wdt.h"

#define Nt 2501 // Numero de muestras de la ventana triangular 
#define N 10000 // Numero de muestras de la señal seno con ruido
#define fo 2 // Numero de ciclos de la señal seno
#define As 2.0 // Amplitud del seno
#define Ar 2.0 // Amplitud del ruido, SNR = 0
#define pi 3.14159 // Definicion de pi
#define tamanoResultadoConvolucion ((N + Nt) - 1) // Calculo del tamaño del vector resultante

#define inicioNC0 0
#define finNC0 (tamanoResultadoConvolucion/2)

static const char *TAG = "CONVOL"; // Variable para información a color

float senoRuido[N]; // Arreglo señal seno con ruido --> 4bytes * 15000 = 60000 bytes / 1024 --> 59 kB de memoria RAM
float Triangular[Nt]; // Arreglo ventana triangular --> 4bytes * 1000 = 4000 bytes / 1024 --> 4 kB de memoria RAM
float Y[tamanoResultadoConvolucion]; // Arreglo resultado un solo nucleo --> 4bytes * 15999 --> 63996 bytes / 1024 --> 62.5 kB 
float YD[tamanoResultadoConvolucion]; // Arreglo resultado un solo nucleo --> 4bytes * 15999 --> 63996 bytes / 1024 --> 62.5 kB
// ----------------------------------------------------------------------------------------------> Total = 188 kB de 280 kB totales

SemaphoreHandle_t semaforoFinNucleo1 = NULL; // Semaforo para sincronizar

void Create_Triangular(){

    for(int n = 0 ; n < Nt ; n++){
        
        Triangular[n] = 1.0f - fabsf((2.0f * n) / (Nt - 1) - 1.0f);
    }
}

void Create_Seno(){
    
    float senal, ruido;
    
    for(int i = 0 ; i < N ; i++){
        
        senal = As*sin(2*pi*fo*i/N);
        ruido = Ar * (2.0 * ((float)rand() / (float)RAND_MAX) - 1.0);
        senoRuido[i] = senal + ruido;
    }
}

void convolucion(int inicio, int fin, float *resultado){

    for (int i = inicio; i < fin; i++){ // Itera sobre cada punto del resultado
        
        float suma = 0.0f; // Acumulador temporal para cada elemento Y[i]

        for (int j = 0; j < Nt; j++){
            
            int indiceEntrada = i - j;

            if (indiceEntrada >= 0 && indiceEntrada < N) { // Verificacion para que el indice no sea menor a 0 o mayor al tamaño del seno

                suma += senoRuido[indiceEntrada] * Triangular[j];
            }
        }

        resultado[i] = suma;
    }
}

void imprimir_buffer(float *buffer, const char *marcaInicio, const char *marcaFin, int longitud) {
    
    printf("%s\n", marcaInicio);

    for (int i = 0; i < longitud; i++){
        printf("%d %.4f\n", i, buffer[i]);
        if (i % 100 == 0) {
            fflush(stdout);
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(200)); // Retardo para evitar errores de transmición
    printf("%s\n", marcaFin);
}

void tareaNucleo1(void *arg){

    esp_task_wdt_add(NULL);     // Para asegurar que el watchDog este monitoreando la tarea
    esp_task_wdt_delete(NULL);  // Desactivamos el watchDog para poder ejecutar una convolucion grande

    int inicioNC1 = (tamanoResultadoConvolucion/2);
    int finNC1 = tamanoResultadoConvolucion;

    convolucion(inicioNC1, finNC1, YD);

    xSemaphoreGive(semaforoFinNucleo1); //(Lo pone en 1)

    while(1){

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void){

    esp_task_wdt_delete(NULL); // Desactivar el Watchdog del Core actual para permitir tareas largas 

    semaforoFinNucleo1 = xSemaphoreCreateBinary(); // Crea un semaforo para sincronizar (inicia en 0)

    ESP_LOGI(TAG, "Generando señales y limpiando memoria");
    Create_Seno(); // Crea la señal seno ruidosa
    Create_Triangular(); // Crea la ventana triangular
    // Inicializar salida en 0
    for (int i = 0; i < tamanoResultadoConvolucion; i++){

        Y[i] = 0; // Inicializar salida de un nucleo en 0 para que no haya basura
        YD[i] = 0; // Inicializar salida de dos nucleos en 0 para que no haya basura
    }

    // ---------- EJECUCION DE UN SOLO NUCLEO -------------
    ESP_LOGI(TAG, "Ejecutando la convolucion sólo en el nucleo %d", xPortGetCoreID()); 

    int64_t start_time = esp_timer_get_time(); // Comienza la medición del tiempo de la convolucion en el nucleo 0
    
    convolucion(0, tamanoResultadoConvolucion, Y);

    int64_t end_time = esp_timer_get_time(); // Finaliza la medición del tiempo de la convolucion en el nucleo 0
     
    int64_t tiempo1N = end_time - start_time; // Obtiene el tiempo que tardó

    //ESP_LOGI(TAG, "Calcular la convolucion sólo en el nucleo %d toma %.3f s", xPortGetCoreID(), (float)tiempo1N/1000000);

    // ---------- EJECUCION EN DOS NUCLEOS -------------
    ESP_LOGI(TAG, "Ejecutando la convolucion en ambos nucleos");

    int64_t start_time2 = esp_timer_get_time(); // Comienza la medición del tiempo de la convolucion en ambos nucleos

    xTaskCreatePinnedToCore(tareaNucleo1, "tareaNucleo1", 4096, NULL, 1, NULL,1); // Crea la tarea del nucleo1 para que empiece a calcular su mitad

    convolucion(inicioNC0, finNC0, YD);

    xSemaphoreTake(semaforoFinNucleo1, portMAX_DELAY); // Espera a que el nucleo 1 lo ponga en 1

    int64_t end_time2 = esp_timer_get_time(); // Finaliza la medición del tiempo de la convolucion en ambos nucleos
     
    int64_t tiempo2N = end_time2 - start_time2; // Obtiene el tiempo que tardó

    //ESP_LOGI(TAG, "Calcular la convolucion en ambos nucleos toma %.3f s", (float)tiempo2N/1000000);

    vTaskDelay(pdMS_TO_TICKS(20)); // Retraso antes de escribir los datos

    // -------------------- Transmición de datos -------------------------

    ESP_LOGI(TAG, "Transmitiendo datos de la ejecución en un solo nucleo");

    imprimir_buffer(Y, "###INICIA_INDIVIDUAL###", "###FIN_INDIVIDUAL###", tamanoResultadoConvolucion);

    vTaskDelay(pdMS_TO_TICKS(20)); // Retraso antes de escribir los datos DE NUEVO

    imprimir_buffer(YD, "###INICIO_AMBOS###", "###FIN_AMBOS###", tamanoResultadoConvolucion);

    vTaskDelay(pdMS_TO_TICKS(20)); // Retraso antes de escribir los datos DE NUEVO

    imprimir_buffer(senoRuido, "###INICIO_SENO###", "###FIN_SENO###", N);

    vTaskDelay(pdMS_TO_TICKS(20)); // Retraso antes de escribir los datos DE NUEVO

    imprimir_buffer(Triangular, "###INICIO_VENTANA###", "###FIN_VENTANA###", Nt);

    ESP_LOGI(TAG, "Transmision completa");

    ESP_LOGI(TAG, "Calcular la convolucion sólo en el nucleo %d toma %.3f s", xPortGetCoreID(), (float)tiempo1N/1000000);
    ESP_LOGI(TAG, "Calcular la convolucion en ambos nucleos toma %.3f s", (float)tiempo2N/1000000);

    vSemaphoreDelete(semaforoFinNucleo1); // Se elimina el semaforo
    
    while(1){ vTaskDelay(pdMS_TO_TICKS(1000)); }
}