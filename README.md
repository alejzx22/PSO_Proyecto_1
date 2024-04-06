# Implementación del Algoritmo de Huffman en C

La Codificación de Huffman es un algoritmo de compresión sin pérdida utilizado principalmente para documentos de texto. Este proyecto presenta una implementación en C del algoritmo de Huffman, así como programas adicionales para comprimir y descomprimir archivos de texto en un directorio dado.

## Funcionamiento del Algoritmo de Huffman

El algoritmo de Huffman es un método de compresión sin pérdida que reemplaza el código binario de cada letra por una codificación nueva de tamaño variable. Asigna códigos de menor longitud a letras más frecuentes y códigos más largos a letras menos frecuentes. Esto permite una compresión fácil de revertir y sin pérdida.

## Implementación

El proyecto consta de tres componentes principales:

1. **Implementación del Algoritmo de Huffman en C**: Se ha desarrollado una versión funcional del algoritmo de Huffman en C que considera signos de puntuación, acentos y otros símbolos especiales.

2. **Programa de Compresión**: Un programa que toma todos los archivos de texto en un directorio dado, los comprime utilizando el algoritmo de Huffman y genera un único archivo binario con el resultado de la compresión.

3. **Programa de Descompresión**: Un programa que descomprime el archivo binario generado por el programa de compresión y recupera el directorio original.

## Prueba del Algoritmo

El algoritmo implementado se probará utilizando la versión en texto plano UTF-8 de los Top 100 libros de los últimos 30 días del proyecto Gutenberg.

[Proyecto Gutenberg - Top 100 libros](https://www.gutenberg.org/browse/scores/top#books-last30)

## Versiones Implementadas

Se han implementado tres versiones de los programas de compresión y descompresión:

1. **Versión Serial**: Implementación secuencial de los programas.
2. **Versión Paralela utilizando fork()**: Se utilizan procesos hijo para realizar la compresión y descompresión en paralelo.
3. **Versión Concurrente utilizando pthread()**: Se utilizan hilos para realizar la compresión y descompresión de manera concurrente.

Todas las versiones deben proporcionar los mismos resultados y medir el tiempo de ejecución en nanosegundos.

## Instrucciones de Uso

1. Clonar el repositorio.
2. Compilar los programas utilizando el compilador de C.
3. Ejecutar los programas según la versión deseada.

## Requisitos

- Compilador de C compatible con el estándar C99.
- Sistema operativo compatible con las funciones utilizadas (fork(), pthread(), etc.).

## Autores
- Alejandro Alfaro
- Bianka Mora
- Bryan Sandí


