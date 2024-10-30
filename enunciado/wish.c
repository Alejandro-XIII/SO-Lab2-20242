#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_ARGS 100

// Función para mostrar un mensaje de error
void mostrarError() {
    char mensaje_error[] = "An error has occurred\n";
    write(STDERR_FILENO, mensaje_error, strlen(mensaje_error));
}

// Función para ejecutar comandos integrados como cd, path y exit
int ejecutarIntegrados(char **args, char **rutas) {
    if (strcmp(args[0], "cd") == 0) {
        if (args[1] == NULL || args[2] != NULL) {
            mostrarError();
        } else {
            if (chdir(args[1]) != 0) {
                mostrarError();
            }
        }
        return 1;
    } else if (strcmp(args[0], "exit") == 0) {
        if (args[1] != NULL) {
            mostrarError();
        } else {
            exit(0);
        }
        return 1;
    } else if (strcmp(args[0], "path") == 0) {
        int i = 1;
        for (i = 1; args[i] != NULL; i++);
        rutas[0] = NULL;  // Resetear las rutas
        for (i = 1; args[i] != NULL; i++) {
            rutas[i-1] = strdup(args[i]);
        }
        rutas[i-1] = NULL;
        return 1;
    }
    return 0;
}

// Función para ejecutar comandos con redirección
void ejecutarComando(char **args, char **rutas, char *archivo_salida) {
    pid_t pid;
    int status;

    pid = fork();
    if (pid == 0) {
        // Proceso hijo
        if (archivo_salida != NULL) {
            int fd = open(archivo_salida, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
            if (fd < 0) {
                mostrarError();
                exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            close(fd);
        }

        // Probar las rutas para encontrar el comando
        for (int i = 0; rutas[i] != NULL; i++) {
            char comando[256];
            snprintf(comando, sizeof(comando), "%s/%s", rutas[i], args[0]);
            execv(comando, args);
        }

        // Si llegamos aquí, execv falló
        mostrarError();
        exit(1);
    } else if (pid < 0) {
        // Error al crear el proceso hijo
        mostrarError();
    } else {
        // Proceso padre espera a que termine el hijo
        waitpid(pid, &status, 0);
    }
}

// Función para ejecutar varios comandos en paralelo
// Función para ejecutar varios comandos en paralelo
void ejecutarComandosParalelo(char **comandos, char **rutas) {
    char *args[MAX_ARGS];
    char *archivo_salida = NULL;
    pid_t pid;
    int i = 0;

    for (i = 0; comandos[i] != NULL; i++) {
        // Parsear el comando individual
        int idx = 0;
        char *token = strtok(comandos[i], " \t");

        // Si el primer token es NULL, significa que no hay comando
        if (token == NULL) {
            // En este caso, simplemente retornamos 0
            continue; // Esto hará que no se ejecute ningún comando.
        }

        // Guardamos el primer token para verificar si hay un comando
        args[idx++] = token;

        // Procesar los siguientes tokens
        while ((token = strtok(NULL, " \t")) != NULL) {
            if (strcmp(token, ">") == 0) {
                // Redirección de salida
                token = strtok(NULL, " \t");
                if (token == NULL || strtok(NULL, " \t") != NULL) {
                    mostrarError();
                    return;
                }
                archivo_salida = token;
                break; // Salimos del bucle para no añadir más argumentos.
            }
            args[idx++] = token; // Agregar el token a args.
        }
        args[idx] = NULL; // Finalizar la lista de argumentos.

        // Verificar si hay al menos un comando antes de intentar ejecutarlo
        if (args[0] != NULL && !ejecutarIntegrados(args, rutas)) {
            // Si no es un comando integrado, ejecutar el comando en un nuevo proceso
            pid = fork();
            if (pid == 0) {
                // Proceso hijo
                ejecutarComando(args, rutas, archivo_salida);
                exit(0); // Termina el hijo después de ejecutar el comando
            }
        }
    }

    // Esperar a que todos los procesos hijos terminen
    while (wait(NULL) > 0);
}



// Función principal para leer, parsear y ejecutar comandos
void bucleShell(char **rutas) {
    char *linea = NULL;
    size_t longitud = 0;
    ssize_t nread;

    while (1) {
        printf("wish> ");
        nread = getline(&linea, &longitud, stdin);
        if (nread == -1) {
            break;
        }

        // Eliminar salto de línea
        if (linea[nread - 1] == '\n') {
            linea[nread - 1] = '\0';
        }

        // Dividir comandos por '&' para ejecución en paralelo
        char *comandos[MAX_ARGS];
        int idx = 0;
        char *comando = strtok(linea, "&");
        while (comando != NULL) {
            comandos[idx++] = comando;
            comando = strtok(NULL, "&");
        }
        comandos[idx] = NULL;

        // Ejecutar los comandos en paralelo
        ejecutarComandosParalelo(comandos, rutas);
    }

    free(linea);
}

// Modo batch
void modoBatch(char *nombre_archivo, char **rutas) {
    FILE *archivo = fopen(nombre_archivo, "r");
    if (archivo == NULL) {
        mostrarError();
        exit(1);
    }

    char *linea = NULL;
    size_t longitud = 0;
    ssize_t nread;

    while ((nread = getline(&linea, &longitud, archivo)) != -1) {
        // Eliminar salto de línea
        if (linea[nread - 1] == '\n') {
            linea[nread - 1] = '\0';
        }

        // Dividir comandos por '&' para ejecución en paralelo
        char *comandos[MAX_ARGS];
        int idx = 0;
        char *comando = strtok(linea, "&");
        while (comando != NULL) {
            comandos[idx++] = comando;
            comando = strtok(NULL, "&");
        }
        comandos[idx] = NULL;

        // Ejecutar los comandos en paralelo
        ejecutarComandosParalelo(comandos, rutas);
    }

    free(linea);
    fclose(archivo);
}

// Función principal
int main(int argc, char *argv[]) {
    // Definir rutas iniciales
    char *rutas[MAX_ARGS] = {"/bin", NULL};

    // Verificar argumentos
    if (argc == 1) {
        // Modo interactivo
        bucleShell(rutas);
    } else if (argc == 2) {
        // Modo batch
        modoBatch(argv[1], rutas);
    } else {
        mostrarError();
        return 1;
    }

    return 0;
}
