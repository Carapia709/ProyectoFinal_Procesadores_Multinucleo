#!/usr/bin/env python3

import serial
import time
import sys

# --- CONFIGURACIÓN ---
# En Linux suele ser /dev/ttyUSB0 o /dev/ttyACM0
# Si no sabes cuál es, ejecuta en la terminal: ls /dev/ttyUSB*
PUERTO = '/dev/ttyUSB0' 
BAUDIOS = 115200
ARCHIVO_SALIDA = 'convol.dat'

try:
    # Intentamos abrir el puerto
    ser = serial.Serial(PUERTO, BAUDIOS, timeout=1)
    print(f"Escuchando en {PUERTO} a {BAUDIOS} baudios...")
    print("Presiona RESET (EN) en tu ESP32 para iniciar...")
    
    f = open(ARCHIVO_SALIDA, 'w')
    grabando = False
    
    while True:
        try:
            linea_bytes = ser.readline()
            # Decodificar bytes a string ignorando errores de caracteres extraños
            linea = linea_bytes.decode('utf-8', errors='ignore').strip()
        except Exception as e:
            # Si falla la lectura, continuamos al siguiente ciclo
            continue 

        # Detección de marcas de inicio y fin
        if "###START_DATA###" in linea:
            grabando = True
            print(">>> Iniciando captura de datos...")
            continue
            
        if "###END_DATA###" in linea:
            print(">>> Fin de captura. Archivo guardado correctamente.")
            break
            
        # Guardar datos si estamos en modo grabación
        if grabando and linea:
            # print(linea) # Descomenta si quieres ver los números pasar en la terminal
            f.write(linea + '\n')

    f.close()
    ser.close()

except serial.SerialException:
    print(f"\nERROR: No se pudo abrir el puerto {PUERTO}.")
    print("Asegúrate de que:")
    print("1. El ESP32 está conectado.")
    print("2. Tienes permisos (prueba: sudo chmod 666 /dev/ttyUSB0)")
    print("3. No tienes el monitor serie de IDF abierto en otra ventana.")

except KeyboardInterrupt:
    print("\nCancelado por el usuario.")
    if 'f' in locals(): f.close()
    if 'ser' in locals(): ser.close()