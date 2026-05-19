#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>

#define MAX_NUM_OBJ 1000

// Structure représentant le problème du sac à dos
typedef struct {
    int capacity;
    int numObj;
    int m[MAX_NUM_OBJ]; // Tableau des masses
    int u[MAX_NUM_OBJ]; // Tableau des utilités
} Problem;

/**
 * Fonction de lecture du fichier contenant le problème
 * @param filename Nom du fichier
 * @param p Pointeur vers la structure Problem à remplir
 */
void readProblem(const char *filename, Problem *p) {
    char line[256];
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "Erreur : fichier %s introuvable.\n", filename);
        exit(EXIT_FAILURE);
    }

    p->numObj = 0;
    p->capacity = 0;

    while (fgets(line, 256, file) != NULL) {
        switch (line[0]) {
            case 'c': // Lecture de la capacité
                if (sscanf(&(line[2]), "%d\n", &(p->capacity)) != 1) {
                    fprintf(stderr, "Erreur de format dans le fichier à la ligne :\n%s", line);
                    fclose(file);
                    exit(EXIT_FAILURE);
                }
                break;
            case 'o': // Lecture d'un objet (masse et utilité)
                if (p->numObj >= MAX_NUM_OBJ) {
                    fprintf(stderr, "Trop d'objets (%d): la limite est %d\n", p->numObj, MAX_NUM_OBJ);
                    fclose(file);
                    exit(EXIT_FAILURE);
                }
                if (sscanf(&(line[2]), "%d %d\n", &(p->m[p->numObj]), &(p->u[p->numObj])) != 2) {
                    fprintf(stderr, "Erreur de format dans le fichier à la ligne :\n%s", line);
                    fclose(file);
                    exit(EXIT_FAILURE);
                } else {
                    p->numObj++;
                }
                break;
            default:
                break; // Ignorer les autres lignes (commentaires potentiels)
        }
    }
    fclose(file);
    if (p->numObj == 0) {
        fprintf(stderr, "Aucun objet trouvé dans le fichier. Arrêt du programme.\n");
        exit(EXIT_FAILURE);
    }
}

/**
 * Résolution du problème du sac à dos, avec ou sans OpenMP.
 * @param p Le problème à résoudre
 * @param numThreads Nombre de threads (0 pour l'exécution séquentielle)
 * @param computeTime Pointeur pour stocker le temps de calcul
 */
void solveKnapsack(Problem *p, int numThreads, double *computeTime) {
    long long cols = p->capacity + 1;
    long long size = (long long)p->numObj * cols;
    
    // Allocation dynamique de la matrice S pour éviter un Stack Overflow
    int *s = (int *)malloc(size * sizeof(int));
    if (s == NULL) {
        fprintf(stderr, "Échec de l'allocation mémoire pour la matrice S.\n");
        exit(EXIT_FAILURE);
    }
    double start;
    if (numThreads > 0) {
        // --- Version Parallèle (OpenMP) ---
        omp_set_num_threads(numThreads);
        
        // Début de la mesure du temps
        start = omp_get_wtime();

        #pragma omp parallel
        {
            // Initialisation de la ligne 0 (parallélisée via omp for)
            #pragma omp for
            for (int j = 0; j <= p->capacity; j++) {
                if (p->m[0] <= j) {
                    s[0 * cols + j] = p->u[0];
                } else {
                    s[0 * cols + j] = 0;
                }
            }

            // Remplissage du reste de la matrice
            for (int i = 1; i < p->numObj; i++) {
                // Chaque thread traite une partie de la ligne i
                // Une barrière implicite à la fin de 'omp for' garantit que la ligne i est finie avant i+1
                #pragma omp for
                for (int j = 0; j <= p->capacity; j++) {
                    int oldM = s[(i - 1) * cols + j];
                    if (p->m[i] <= j) {
                        int newM = s[(i - 1) * cols + (j - p->m[i])] + p->u[i];
                        s[i * cols + j] = (oldM > newM) ? oldM : newM;
                    } else {
                        s[i * cols + j] = oldM;
                    }
                }
            }
        }
    } else {
        // --- Version Séquentielle ---
        start = omp_get_wtime();
        
        // Initialisation de la ligne 0
        for (int j = 0; j <= p->capacity; j++) {
            if (p->m[0] <= j) {
                s[0 * cols + j] = p->u[0];
            } else {
                s[0 * cols + j] = 0;
            }
        }

        // Remplissage du reste de la matrice
        for (int i = 1; i < p->numObj; i++) {
            for (int j = 0; j <= p->capacity; j++) {
                int oldM = s[(i - 1) * cols + j];
                if (p->m[i] <= j) {
                    int newM = s[(i - 1) * cols + (j - p->m[i])] + p->u[i];
                    s[i * cols + j] = (oldM > newM) ? oldM : newM;
                } else {
                    s[i * cols + j] = oldM;
                }
            }
        }
    }

    // Fin de la mesure du temps
    double end = omp_get_wtime();
    *computeTime = end - start;

    // L'utilité maximale se trouve dans la dernière case de la matrice
    int maxUtility = s[(p->numObj - 1) * cols + p->capacity];

    // Reconstruction du tableau des décisions E
    int *e = (int *)malloc(p->numObj * sizeof(int));
    if (e == NULL) {
        fprintf(stderr, "Échec de l'allocation mémoire pour le tableau E.\n");
        free(s);
        exit(EXIT_FAILURE);
    }

    int j = p->capacity;
    for (int i = p->numObj - 1; i > 0; i--) {
        if (s[i * cols + j] == s[(i - 1) * cols + j]) {
            e[i] = 0; // L'objet i n'a pas été pris
        } else {
            e[i] = 1; // L'objet i a été pris
            j -= p->m[i];
        }
    }
    e[0] = (s[0 * cols + j] > 0) ? 1 : 0; // Cas du premier objet

    // Vérification : somme des masses et somme des utilités
    int checkUtil = 0;
    int checkMass = 0;
    for (int i = 0; i < p->numObj; i++) {
        if (e[i]) {
            checkUtil += p->u[i];
            checkMass += p->m[i];
        }
    }

    if (checkUtil != maxUtility) {
        fprintf(stderr, "Erreur de vérification : utilité calculée = %d, utilité attendue = %d\n", checkUtil, maxUtility);
    }
    if (checkMass > p->capacity) {
        fprintf(stderr, "Erreur de vérification : masse = %d dépasse la capacité de = %d\n", checkMass, p->capacity);
    }

    // Affichage des résultats (le script bash lira ces valeurs au format CSV)
    printf("%f,%d\n", *computeTime, maxUtility);

    // Libération de la mémoire
    free(s);
    free(e);
}

int main(int argc, char *argv[]) {
    // Vérification du nombre d'arguments
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <problem_file> [num_threads]\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *filename = argv[1];
    int numThreads = 0; // 0 indique l'exécution séquentielle

    if (argc >= 3) {
        numThreads = atoi(argv[2]);
    }

    Problem p;
    readProblem(filename, &p);

    double computeTime = 0.0;
    solveKnapsack(&p, numThreads, &computeTime);

    return EXIT_SUCCESS;
}
