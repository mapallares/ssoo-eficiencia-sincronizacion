#include <stdio.h>      // Para funciones estándar de entrada y salida.
#include <stdlib.h>     // Incluye funciones de manejo de memoria dinámica y control de procesos.
#include <pthread.h>    // Incluye las funciones y tipos para hilos POSIX.
#include <time.h>       // Incluye funciones para medir el tiempo.
#include <unistd.h>

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

Individuo* poblacion;
int npoblacion, nthreads, ngeneraciones;

pthread_barrier_t barreraHilos;
pthread_barrier_t barreraPrincipal;

typedef struct {
    int hiloId;
    int startIndex;
    int endIndex;
} ArgumentosHilo;

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
        printf("Hilo #%d terminó de evolucionar la región (%d,%d).\n", argumentos->hiloId, argumentos->startIndex, argumentos->endIndex - 1);

        // Esperamos a que todos los hilos terminen de evolucionar la región asignada de la población
        pthread_barrier_wait(&barreraHilos);

        // Esperamos a que el hilo principal termine de imprimir
        pthread_barrier_wait(&barreraPrincipal);
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
        error("Error leyendo el número de individuos.");
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

    // Creamos dos barreras, una para los hilos trabajadores y otra para el hilo principal
    pthread_barrier_init(&barreraHilos, NULL, nthreads + 1);
    pthread_barrier_init(&barreraPrincipal, NULL, nthreads + 1);

    int delta = npoblacion / nthreads;
    for (int index = 0; index < nthreads; index++) {
        argumentos[index].hiloId = index;
        argumentos[index].startIndex = index * delta;
        argumentos[index].endIndex = (index == nthreads - 1) ? npoblacion : (index + 1) * delta;
        pthread_create(&hilos[index], NULL, evolucionar, &argumentos[index]);
    }

    for (int g = 0; g < ngeneraciones; g++) {
        // Esperamos a que los hilos trabjadores terminen
        pthread_barrier_wait(&barreraHilos);

        combSort(poblacion, npoblacion);
        printf("Generación %d - Mejores 10 individuos:\n", g + 1);
        printTop10();
        
        // Permitimos a los hilos trabajadores continuar con la siguiente generación
        pthread_barrier_wait(&barreraPrincipal);
    }

    for (int index = 0; index < nthreads; index++) {
        pthread_join(hilos[index], NULL);
    }

    pthread_barrier_destroy(&barreraHilos);
    pthread_barrier_destroy(&barreraPrincipal);
    free(poblacion);
    return 0;
}