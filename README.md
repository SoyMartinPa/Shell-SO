# Shell-SO

Implementación de una shell sencilla desarrollada en C para sistemas tipo Unix/Linux.

# Integrantes

- Carlos Salinas Pereira
- Daniel López Ramirez
- Ignacio Contardo Brito
- Martin Parra Polanco

## Requisitos

Para compilar y ejecutar el proyecto es necesario:

- GCC.
- Make.
- Un sistema operativo basado en Unix/Linux.

## Archivos principales

- `main.c`: contiene la función principal y el ciclo de ejecución de la shell.
- `funciones.c`: contiene la implementación de las funciones para ejecutar comandos, procesos, tuberías y redirecciones.
- `funciones.h`: contiene las declaraciones de las funciones utilizadas.
- `Makefile`: automatiza el proceso de compilación.

## Compilación

Clona el repositorio y accede a la carpeta del proyecto:

```bash
git clone https://github.com/SoyMartinPa/Shell-SO.git
cd Shell-SO
```

Luego, compila el programa utilizando `make`:

```bash
make
```

El comando anterior compila los archivos `main.c` y `funciones.c` y genera el ejecutable llamado `myshell`.

También puedes compilar el programa manualmente con GCC:

```bash
gcc main.c funciones.c -o myshell
```

## Ejecución

Después de compilar el proyecto, ejecuta la shell con:

```bash
./myshell
```

Al iniciar, se mostrará un prompt similar al siguiente:

```text
miShell:/ruta/actual$>
```

Desde este prompt puedes ejecutar comandos del sistema, por ejemplo:

```text
ls
pwd
echo Hola
```

## Comandos internos

La shell incluye los siguientes comandos internos:

### `cd`

Permite cambiar el directorio actual:

```text
cd nombre_del_directorio
```

Ejemplo:

```text
cd documentos
```

### `jobs`

Muestra los procesos que se están ejecutando en segundo plano:

```text
jobs
```

### `pmon`

Muestra información relacionada con los procesos:

```text
pmon
```

### `exit`

Finaliza la ejecución de la shell:

```text
exit
```

También puedes presionar `Ctrl+D` para salir.

## Tuberías

La shell permite conectar comandos mediante el operador `|`.

Ejemplo:

```text
ls | wc -l
```

Otro ejemplo:

```text
cat archivo.txt | grep palabra
```

## Procesos en segundo plano

Los comandos pueden ejecutarse en segundo plano utilizando `&`, si esta funcionalidad está disponible en la implementación:

```text
sleep 10 &
```

Después puedes consultar los procesos con:

```text
jobs
```

## Señales

El programa incluye manejo de señales para controlar los procesos, incluyendo:

- `SIGINT`, mediante `Ctrl+C`.
- `SIGQUIT`.
- `SIGCHLD`, para detectar la finalización de procesos hijos.

## Limpieza

El `Makefile` incluye un comando para limpiar los archivos generados:

```bash
make clean
```

Sin embargo, debido a que el ejecutable generado se llama `myshell`, puedes eliminarlo manualmente con:

```bash
rm -f myshell
```

## Ejemplo completo

```bash
git clone https://github.com/SoyMartinPa/Shell-SO.git
cd Shell-SO
make
./myshell
```