#!/usr/bin/env python3

import serial # Libreria para leer el puerto serie
import time # Libreria para retardos
import sys # Libreria para interactuar con el SO y poder leer datos

# --- CONFIGURACION ---
PUERTO = '/dev/ttyUSB0'  # Puerto serial donde esta conectada la ESP32
BAUDIOS = 115200 # Velocidad de la comunicacion

# Archivos de salida
FILE_SINGLE = 'single_core.dat' # Archivo para los datos de la ejecucion en un solo nucleo
FILE_DUAL = 'dual_core.dat' # Archivo para los datos de la ejecucion en los 2 nucleos

try: # Intenta esto
    ser = serial.Serial(PUERTO, BAUDIOS, timeout=1) # Abre el canal de comunicación con el puerto serie, másimo tiempo de espera 1s
    print(f"Conectado a {PUERTO}. Esperando datos del ESP32...") # Mensaje en la terminal
    print("Presiona RESET (EN) en tu ESP32 para iniciar...") # Mensaje en la terminal

    file_handle = None # Variable vacía para abrir cualquier documento
    grabando = False # Bandera de control, TRUE -> se guardan los datos, FALSE -> Se ignoran los datos

    while True: # Bucle infinito
        try:
            linea_bytes = ser.readline() # Lee los datos del puerto serial crudos hasta encontrar un enter
            linea = linea_bytes.decode('utf-8', errors='ignore').strip() # Transforma los datos leídos en texto legible. Ignora basura, limpia el inicio y el fin de cada linea
        except:
            continue

        if not linea:
            continue # Si la linea esta vacia regresa al inicio del while

        # Mostrar logs del sistema (info verde) en pantalla para ver el progreso
        if "I (EXP" in linea or "I (COMP" in linea or "RESULTADO" in linea:
            print(f"[ESP32]: {linea}")

        # --- LÓGICA DE SINGLE CORE ---
        if "###INICIA_INDIVIDUAL###" in linea: # Detecta la marca de inicio de los datos de un solo nucleo
            print(f"\n>>> Detectado inicio de datos de UN NUCLEO. Grabando en {FILE_SINGLE}...") # Mensaje en la terminal
            file_handle = open(FILE_SINGLE, 'w') # Crea o abre el archivo de datos de la ejecución con un solo nucleo
            grabando = True # Cambia el valor de bandera de control para guardar los datos
            continue # Regresa al inicio del while para no guardar la etiqueta
        
        if "###FIN_INDIVIDUAL###" in linea: # Detecta la marca de fin de datos de la ejecucion con un sólo núcleo
            print(f">>> Fin datos de datos de UN NUCLEO. Archivo cerrado.") # Mensaje en la terminal
            if file_handle: file_handle.close() # Guarda y cierra el archivo creado
            grabando = False # Cambia el valor de la bandera de control
            continue # Regresa al inicio del while

        # --- LÓGICA DE DUAL CORE ---
        if "###INICIO_AMBOS###" in linea: # Detecta la marca de inicio de los datos de la ejecución con 2 nucleos
            print(f"\n>>> Detectado inicio de datos de AMBOS NUCLEOS. Grabando en {FILE_DUAL}...") # Mensaje en la terminal
            file_handle = open(FILE_DUAL, 'w') # Crea o abre el archivo de datos de la ejecución con dos nucleos
            grabando = True # Cambia el valor de bandera de control para guardar los datos
            continue # Regresa al inicio del while para no guardar la etiqueta
            
        if "###FIN_AMBOS###" in linea: # Detecta la marca de fin de datos de la ejecucion con los dos núcleos
            print(f">>> Fin datos de datos de AMBOS NUCLEOS. Archivo cerrado.") # Mensaje en la terminal
            if file_handle: file_handle.close() # Guarda y cierra el archivo creado
            grabando = False # Cambia el valor de la bandera de control
            print("\n¡PROCESO TERMINADO CON ÉXITO!")
            break # SALE DEL CICLO INFINITO Y TERMINA EL PROGRAMA

        # --- GUARDADO ---
        if grabando and file_handle: # Si la bandera de control esta en TRUE y hay un archivo abierto 
            file_handle.write(linea + '\n') # Se escribe el dato en el archivo y hace un salto de línea

    ser.close() # Cierra la comunicación con el puerto serial

except KeyboardInterrupt:
    print("\nCancelado por usuario.") # Mensaje en la terminal
except Exception as e:
    print(f"\nError: {e}") # Muestra el error en caso de algún imprevisto