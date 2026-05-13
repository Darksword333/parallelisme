#include <stdio.h>
#include <stdlib.h>
#include <omp.h>

#define LIGNES 10000
#define COLONNES 10000

// Version 1 : sequentielle
long long sommeSequentielle(int *matrice, int lignes, int colonnes) {
    long long somme = 0;
    for (int i = 0; i < lignes; i++) {
        for (int j = 0; j < colonnes; j++) {
            somme += matrice[i * colonnes + j];
        }
    }
    return somme;
}

// Version 2 : chaque thread calcule sa somme partielle dans un tableau,
// puis un seul thread fait la somme finale
long long sommePartielle(int *matrice, int lignes, int colonnes, int nb_thread) {
    long long *sommesPartielles = (long long *)calloc(nb_thread, sizeof(long long));
    if (sommesPartielles == NULL) {
        perror("Erreur allocation sommes partielles");
        exit(EXIT_FAILURE);
    }

    #pragma omp parallel num_threads(nb_thread)
    {
        int idThread = omp_get_thread_num();
        long long sommeLocale = 0;
        #pragma omp for
        for (int i = 0; i < lignes; i++) {
            for (int j = 0; j < colonnes; j++) {
                sommeLocale += matrice[i * colonnes + j];
            }
        }
        sommesPartielles[idThread] = sommeLocale;
    }

    // un seul thread (le main ici) fait la somme des sommes partielles
    long long sommeTotale = 0;
    for (int i = 0; i < nb_thread; i++) {
        sommeTotale += sommesPartielles[i];
    }

    free(sommesPartielles);
    return sommeTotale;
}

// Version 3 : chaque thread ajoute sa somme partielle a la somme globale
// en exclusion mutuelle (sans reduction)
long long sommeMutex(int *matrice, int lignes, int colonnes, int nb_thread) {
    long long sommeTotale = 0;

    #pragma omp parallel num_threads(nb_thread)
    {
        long long sommeLocale = 0;
        #pragma omp for nowait
        for (int i = 0; i < lignes; i++) {
            for (int j = 0; j < colonnes; j++) {
                sommeLocale += matrice[i * colonnes + j];
            }
        }
        // section critique : un thread a la fois
        // atomic marchait aussi ici
        // nowait sur le for : pas de barriere, threads entrent en critical
        // des qu ils finissent leur chunk au lieu d attendre les autres
        #pragma omp critical
        {
            sommeTotale += sommeLocale;
        }
    }

    return sommeTotale;
}

// Version 4 : avec reduction
long long sommeReduction(int *matrice, int lignes, int colonnes, int nb_thread) {
    long long sommeTotale = 0;

    #pragma omp parallel for num_threads(nb_thread) reduction(+:sommeTotale)
    for (int i = 0; i < lignes; i++) {
        for (int j = 0; j < colonnes; j++) {
            sommeTotale += matrice[i * colonnes + j];
        }
    }

    return sommeTotale;
}

int main(void) {
    int nbThreadsTest[] = {1, 2, 4, 8, 16};
    int nbConfigs = sizeof(nbThreadsTest) / sizeof(nbThreadsTest[0]);

    size_t taille = (size_t)LIGNES * COLONNES;
    int *matrice = (int *)malloc(taille * sizeof(int));
    if (matrice == NULL) {
        perror("Erreur allocation matrice");
        return EXIT_FAILURE;
    }

    // initialisation a 1 pour eviter les debordements
    for (size_t i = 0; i < taille; i++) {
        matrice[i] = 1;
    }

    double debut, fin;
    long long resultat;

    printf("Taille matrice : %d x %d\n", LIGNES, COLONNES);
    printf("Threads dispo (max OpenMP) : %d\n\n", omp_get_max_threads());

    // V1 : sequentielle, une seule fois (independant du nb threads)
    debut = omp_get_wtime();
    resultat = sommeSequentielle(matrice, LIGNES, COLONNES);
    fin = omp_get_wtime();
    double tempsSeq = fin - debut;
    printf("V1 Sequentielle      : somme = %lld, temps = %f s\n\n", resultat, tempsSeq);

    printf("%-8s | %-12s | %-12s | %-12s\n",
           "Threads", "V2 Partiel", "V3 Mutex", "V4 Reduction");
    printf("---------+--------------+--------------+-------------\n");

    for (int k = 0; k < nbConfigs; k++) {
        int nb_thread = nbThreadsTest[k];

        debut = omp_get_wtime();
        resultat = sommePartielle(matrice, LIGNES, COLONNES, nb_thread);
        fin = omp_get_wtime();
        double tV2 = fin - debut;

        debut = omp_get_wtime();
        resultat = sommeMutex(matrice, LIGNES, COLONNES, nb_thread);
        fin = omp_get_wtime();
        double tV3 = fin - debut;

        debut = omp_get_wtime();
        resultat = sommeReduction(matrice, LIGNES, COLONNES, nb_thread);
        fin = omp_get_wtime();
        double tV4 = fin - debut;

        printf("%-8d | %-12f | %-12f | %-12f\n",
               nb_thread, tV2, tV3, tV4);
    }

    free(matrice);
    return EXIT_SUCCESS;
}
