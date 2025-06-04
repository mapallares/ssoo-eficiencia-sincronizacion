#include <stdio.h>      // Para funciones estándar de entrada y salida.
#include <stdlib.h>     // Incluye funciones de manejo de memoria dinámica y control de procesos.
#include <pthread.h>    // Incluye las funciones y tipos para hilos POSIX.
#include <time.h>       // Incluye funciones para medir el tiempo.

void error(char *message){
    perror(message);
    exit(-1);
}

typedef struct {
    int id;
    double feature1;
    double feature2;
    double fitness;
} Individuo;

typedef struct {
    int hiloId;
    int startIndex;
    int endIndex;
} ArgumentosHilo;

Individuo* poblacion;
int npoblacion, nthreads, ngeneraciones;

// Definimos dos variables de condición una para el hilo principal y otra para los trabadores, y el mutex para la protección de las variables compartidas
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condPrincipal = PTHREAD_COND_INITIALIZER;
pthread_cond_t condHilos = PTHREAD_COND_INITIALIZER;

int hilosListos = 0;
int generacionActual = 0;
int hilosEsperando = 0;
int siguienteGeneracion = 0;

void calcularFitness(Individuo* individuo) {
    individuo->fitness = (individuo->feature1 + individuo->feature2) / 2.0;
}

void* evolucionar(void* arg) {
    ArgumentosHilo* argumentos = (ArgumentosHilo*)arg;

    for (int g = 0; g < ngeneraciones; g++) {
        for (int index = argumentos->startIndex; index < argumentos->endIndex; index++) {
            double delta1 = ((double)rand() / RAND_MAX);
            double delta2 = ((double)rand() / RAND_MAX);
            poblacion[index].feature1 += delta1;
            poblacion[index].feature2 += delta2;
            calcularFitness(&poblacion[index]);
        }

        printf("Hilo #%d terminó de evolucionar la región (%d,%d) de la población.\n", argumentos->hiloId, argumentos->startIndex, argumentos->endIndex - 1);

        // Habilitamos el mutex para modificar variables compartidas de forma segura
        pthread_mutex_lock(&mutex);
        hilosListos++;

        if (hilosListos == nthreads) {
            pthread_cond_signal(&condPrincipal);  // Notificamos al hilo principal cuando todos los hilos hayan terminado
        }

        while (generacionActual == g) {
            pthread_cond_wait(&condHilos, &mutex);  // Esperamos a que el hilo principal indique la siguiente generación
        }

        // Liberamos el mutex después de esperar o continuar
        pthread_mutex_unlock(&mutex);
    }

    return NULL;
}

void combSort(Individuo* individuos, int n) {
    int brecha = n;
    int intercambiado = 1;

    while (brecha > 1 || intercambiado) {
        brecha = (brecha * 10) / 13;
        if (brecha < 1) brecha = 1;

        intercambiado = 0;
        for (int index = 0; index + brecha < n; index++) {
            if (individuos[index].fitness < individuos[index + brecha].fitness) {
                Individuo temporal = individuos[index];
                individuos[index] = individuos[index + brecha];
                individuos[index + brecha] = temporal;
                intercambiado = 1;
            }
        }
    }
}

void printTop10() {
    int tope = npoblacion < 10 ? npoblacion : 10;
    for (int index = 0; index < tope; index++) {
        printf("%d. [#%d] (Feature 1 = %.2f) (Feature 2 = %.2f) (Fitness = %.2f)\n",
            (index + 1),
            poblacion[index].id,
            poblacion[index].feature1,
            poblacion[index].feature2,
            poblacion[index].fitness);
    }
    printf("-----------------------------------------------------------------\n");
}

int main(int argc, char* argv[]) {
    
    srand(time(NULL));

    if(argv[1]) {
        nthreads = atoi(argv[1]);
    }
    else {
        error("Error leyendo el número de hilos.");
    }

    if(argv[2]) {
        ngeneraciones = atoi(argv[2]);
    }
    else {
        error("Error leyendo el número de generaciones.");
    }

    if(argv[3]) {
        npoblacion = atoi(argv[3]);
    }
    else {
        error("Error leyendo el número de indiviuos.");
    }

    poblacion = malloc(npoblacion * sizeof(Individuo));
    if(!poblacion) { 
        error("Error malloc para el vector de individuos."); 
    }

    for (int index = 0; index < npoblacion; index++) {
        poblacion[index].id = index + 1;
        poblacion[index].feature1 = 0;
        poblacion[index].feature2 = 0;
        calcularFitness(&poblacion[index]);
    }

    pthread_t hilos[nthreads];
    ArgumentosHilo argumentos[nthreads];

    int delta = npoblacion / nthreads;
    for (int index = 0; index < nthreads; index++) {
        argumentos[index].hiloId = index;
        argumentos[index].startIndex = index * delta;
        argumentos[index].endIndex = (index == nthreads - 1) ? npoblacion : (index + 1) * delta;
        pthread_create(&hilos[index], NULL, evolucionar, &argumentos[index]);
    }

    for (int g = 0; g < ngeneraciones; g++) {
        // Habilitamos el mutex para acceder a variables compartidas
        pthread_mutex_lock(&mutex);
        while (hilosListos < nthreads) {
            pthread_cond_wait(&condPrincipal, &mutex); // El hilo principal espera hasta que todos los hilos trabajadores terminen la generación actual
        }

        combSort(poblacion, npoblacion);
        printf("Generación %d - Mejores 10 individuos:\n", g + 1);
        printTop10();

        // Actualizamos la generación y reiniciamos el indicador de los hilos trabajadores
        generacionActual++; 
        hilosListos = 0;

        pthread_cond_broadcast(&condHilos);  // Despertamos a todos los hilos para que comiencen con la nueva generación
        pthread_mutex_unlock(&mutex);
    }

    for (int index = 0; index < nthreads; index++) {
        pthread_join(hilos[index], NULL);
    }

    free(poblacion);
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&condHilos);
    pthread_cond_destroy(&condPrincipal);

    return 0;
}
