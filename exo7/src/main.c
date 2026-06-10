#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <omp.h>

// Définition d'un point tridimensionnel (3D)
typedef struct {
    double x;
    double y;
    double z;
} Point;

// Fonction de chargement du jeu de données avec replis de chemins en cas d'erreur de positionnement du fichier
Point* loadDataset(const char* filename, int* numPoints) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        char path[512];
        snprintf(path, sizeof(path), "data/%s", filename);
        file = fopen(path, "r");
        if (!file) {
            snprintf(path, sizeof(path), "../data/%s", filename);
            file = fopen(path, "r");
            if (!file) {
                fprintf(stderr, "Erreur : Impossible d'ouvrir le fichier %s dans le répertoire courant, dans data/ ou dans ../data/.\n", filename);
                return NULL;
            }
        }
    }
    
    if (fscanf(file, "%d", numPoints) != 1) {
        fprintf(stderr, "Erreur lors de la lecture du nombre de points dans le jeu de données.\n");
        fclose(file);
        return NULL;
    }
    
    Point* points = malloc(sizeof(Point) * (*numPoints));
    if (!points) {
        perror("Allocation échouée pour le tableau de points");
        fclose(file);
        return NULL;
    }
    
    for (int i = 0; i < *numPoints; i++) {
        if (fscanf(file, "%lf %lf %lf", &points[i].x, &points[i].y, &points[i].z) != 3) {
            fprintf(stderr, "Erreur lors de la lecture des coordonnées du point %d\n", i);
            free(points);
            fclose(file);
            return NULL;
        }
    }
    
    fclose(file);
    return points;
}

// Fonction utilitaire pour calculer le carré de la distance euclidienne (évite le coût de l'opération sqrt)
double distSquared(Point a, Point b) {
    return (a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z);
}

// Implémentation séquentielle classique de l'algorithme K-Means
void runSequentialKMeans(Point* points, int numPoints, int k, Point* centroids, int* counts, int* outIt) {
    Point* newCentroids = malloc(sizeof(Point) * k);
    int* globalCounts = malloc(sizeof(int) * k);
    if (!newCentroids || !globalCounts) {
        perror("Allocation échouée dans runSequentialKMeans");
        exit(EXIT_FAILURE);
    }
    
    // Initialisation déterministe : centroids initiaux choisis comme les k premiers points du jeu de données
    for (int i = 0; i < k; i++) {
        centroids[i] = points[i];
    }
    
    int it = 0;
    int maxIt = 1000;
    int converged = 0;
    
    while (it < maxIt && !converged) {
        // Réinitialisation des décomptes et des nouveaux centroids temporaires
        for (int c = 0; c < k; c++) {
            newCentroids[c].x = 0.0;
            newCentroids[c].y = 0.0;
            newCentroids[c].z = 0.0;
            globalCounts[c] = 0;
        }
        
        // Affectation de chaque point au centroid le plus proche
        for (int i = 0; i < numPoints; i++) {
            double minDist = -1.0;
            int bestCluster = -1;
            Point p = points[i];
            
            for (int c = 0; c < k; c++) {
                double dist = distSquared(p, centroids[c]);
                if (bestCluster == -1 || dist < minDist) {
                    minDist = dist;
                    bestCluster = c;
                }
            }
            
            newCentroids[bestCluster].x += p.x;
            newCentroids[bestCluster].y += p.y;
            newCentroids[bestCluster].z += p.z;
            globalCounts[bestCluster]++;
        }
        
        // Recalcul des coordonnées des centroids (moyenne arithmétique des points affectés)
        for (int c = 0; c < k; c++) {
            if (globalCounts[c] > 0) {
                newCentroids[c].x /= globalCounts[c];
                newCentroids[c].y /= globalCounts[c];
                newCentroids[c].z /= globalCounts[c];
            } else {
                newCentroids[c] = centroids[c];
            }
        }
        
        // Évaluation de la convergence : la variation est la somme des distances euclidiennes entre anciens et nouveaux centroids
        double variation = 0.0;
        for (int c = 0; c < k; c++) {
            double dx = centroids[c].x - newCentroids[c].x;
            double dy = centroids[c].y - newCentroids[c].y;
            double dz = centroids[c].z - newCentroids[c].z;
            variation += sqrt(dx * dx + dy * dy + dz * dz);
        }
        
        // Vérification si la variation globale est inférieure au seuil de convergence (1e-4)
        if (variation < 1e-4) {
            converged = 1;
        }
        
        // Mise à jour des centroids actifs et des décomptes d'éléments pour l'itération suivante
        for (int c = 0; c < k; c++) {
            centroids[c] = newCentroids[c];
            counts[c] = globalCounts[c];
        }
        
        it++;
    }
    
    *outIt = it;
    free(newCentroids);
    free(globalCounts);
}

// Implémentation parallèle de l'algorithme K-Means avec OpenMP
void runParallelKMeans(Point* points, int numPoints, int k, int numThreads, Point* centroids, int* counts, int* outIt) {
    Point* newCentroids = malloc(sizeof(Point) * k);
    int* globalCounts = malloc(sizeof(int) * k);
    
    // Déclaration de tableaux d'accumulation locaux à chaque thread (Thread-Local Reduction Arrays)
    // structurés sous forme d'un tableau unidimensionnel plat de taille numThreads * k.
    // Cette structure évite l'utilisation d'instructions atomiques (#pragma omp atomic) ou de sections
    // critiques (#pragma omp critical) lors de la phase d'accumulation des coordonnées pour 200 000 points.
    Point* localSums = calloc(numThreads * k, sizeof(Point));
    int* localCounts = calloc(numThreads * k, sizeof(int));
    
    if (!newCentroids || !globalCounts || !localSums || !localCounts) {
        perror("Allocation échouée dans runParallelKMeans");
        exit(EXIT_FAILURE);
    }
    
    // Initialisation déterministe : centroids de départ = k premiers points du jeu de données
    for (int i = 0; i < k; i++) {
        centroids[i] = points[i];
    }
    
    int it = 0;
    int maxIt = 1000;
    int converged = 0;
    
    // REGION PARALLELE UNIQUE : Choix fort d'englober la boucle externe `while` pour éviter
    // le coût prohibitif de création/destruction de l'équipe de threads (fork-join overhead) à chaque itération.
    #pragma omp parallel num_threads(numThreads)
    {
        int tid = omp_get_thread_num();
        
        while (it < maxIt && !converged) {
            // Étape 1 : Réinitialisation locale des accumulateurs locaux de chaque thread.
            // Ce traitement est fait individuellement par chaque thread sur sa propre section.
            for (int c = 0; c < k; c++) {
                localSums[tid * k + c].x = 0.0;
                localSums[tid * k + c].y = 0.0;
                localSums[tid * k + c].z = 0.0;
                localCounts[tid * k + c] = 0;
            }
            
            // Barrière de synchronisation 1 : On s'assure que tous les threads ont bien fini
            // de remettre à zéro leurs accumulateurs avant de lancer la phase d'accumulation des points.
            #pragma omp barrier
            
            // Étape 2 : Affectation des points au centroid le plus proche.
            // La boucle sur les points est distribuée statiquement entre les threads.
            // Utilisation de nowait pour supprimer la barrière implicite de fin de boucle car une barrière
            // explicite et plus parlante est insérée juste après.
            #pragma omp for nowait
            for (int i = 0; i < numPoints; i++) {
                double minDist = -1.0;
                int bestCluster = -1;
                Point p = points[i];
                
                for (int c = 0; c < k; c++) {
                    Point centroid = centroids[c];
                    double dist = (p.x - centroid.x) * (p.x - centroid.x) +
                                  (p.y - centroid.y) * (p.y - centroid.y) +
                                  (p.z - centroid.z) * (p.z - centroid.z);
                    if (bestCluster == -1 || dist < minDist) {
                        minDist = dist;
                        bestCluster = c;
                    }
                }
                
                localSums[tid * k + bestCluster].x += p.x;
                localSums[tid * k + bestCluster].y += p.y;
                localSums[tid * k + bestCluster].z += p.z;
                localCounts[tid * k + bestCluster]++;
            }
            
            // Barrière de synchronisation 2 : On s'assure que l'ensemble des threads a achevé
            // le traitement local de ses points avant de procéder au recalcul des centroids.
            #pragma omp barrier
            
            // Étape 3 : Recalcul global des centroids par réduction des accumulateurs thread-local.
            // On distribue le calcul parallèlement sur les k clusters (jusqu'à 31). Chaque thread
            // se charge d'un sous-ensemble de clusters, réduisant les sommes des threads.
            // La clause nowait est utilisée de la même façon que précédemment.
            #pragma omp for nowait
            for (int c = 0; c < k; c++) {
                double sumX = 0.0;
                double sumY = 0.0;
                double sumZ = 0.0;
                int totalCount = 0;
                
                for (int t = 0; t < numThreads; t++) {
                    sumX += localSums[t * k + c].x;
                    sumY += localSums[t * k + c].y;
                    sumZ += localSums[t * k + c].z;
                    totalCount += localCounts[t * k + c];
                }
                
                globalCounts[c] = totalCount;
                if (totalCount > 0) {
                    newCentroids[c].x = sumX / totalCount;
                    newCentroids[c].y = sumY / totalCount;
                    newCentroids[c].z = sumZ / totalCount;
                } else {
                    newCentroids[c] = centroids[c];
                }
            }
            
            // Barrière de synchronisation 3 : On attend que tous les nouveaux centroids soient calculés
            // par tous les threads avant de laisser un seul thread évaluer la convergence globale.
            #pragma omp barrier
            
            // Étape 4 : Évaluation de la convergence et mise à jour des centroids.
            // Cette tâche séquentielle légère est confiée à un unique thread (#pragma omp single).
            // Le bloc `single` comporte une barrière implicite à sa sortie, garantissant que
            // tous les threads synchronisent leurs mémoires et partagent les nouvelles valeurs
            // de centroids et la variable booléenne de convergence (converged) avant l'itération suivante.
            #pragma omp single
            {
                double variation = 0.0;
                for (int c = 0; c < k; c++) {
                    double dx = centroids[c].x - newCentroids[c].x;
                    double dy = centroids[c].y - newCentroids[c].y;
                    double dz = centroids[c].z - newCentroids[c].z;
                    variation += sqrt(dx * dx + dy * dy + dz * dz);
                }
                
                if (variation < 1e-4) {
                    converged = 1;
                }
                
                for (int c = 0; c < k; c++) {
                    centroids[c] = newCentroids[c];
                    counts[c] = globalCounts[c];
                }
                
                it++;
            } 
        }
    }
    
    *outIt = it;
    
    free(newCentroids);
    free(globalCounts);
    free(localSums);
    free(localCounts);
}

int main(int argc, char** argv) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <filename> <nb_cluster> <nb_threads>\n", argv[0]);
        return EXIT_FAILURE;
    }
    
    const char* filename = argv[1];
    int k = atoi(argv[2]);
    int numThreads = atoi(argv[3]);
    
    if (k <= 0) {
        fprintf(stderr, "Error: Number of clusters must be greater than 0.\n");
        return EXIT_FAILURE;
    }
    if (numThreads <= 0) {
        fprintf(stderr, "Error: Number of threads must be greater than 0.\n");
        return EXIT_FAILURE;
    }
    
    int numPoints = 0;
    Point* points = loadDataset(filename, &numPoints);
    if (!points) {
        return EXIT_FAILURE;
    }
    
    if (k > numPoints) {
        fprintf(stderr, "Erreur : Le nombre de clusters (%d) ne peut pas être plus grand que le nombre de points (%d).\n", k, numPoints);
        free(points);
        return EXIT_FAILURE;
    }
    
    // Allocation d'espace mémoire pour stocker les résultats finals
    Point* seqCentroids = malloc(sizeof(Point) * k);
    int* seqCounts = malloc(sizeof(int) * k);
    Point* parCentroids = malloc(sizeof(Point) * k);
    int* parCounts = malloc(sizeof(int) * k);
    
    if (!seqCentroids || !seqCounts || !parCentroids || !parCounts) {
        perror("Allocation échouée pour les structures de résultats centroids/counts");
        free(points);
        free(seqCentroids);
        free(seqCounts);
        free(parCentroids);
        free(parCounts);
        return EXIT_FAILURE;
    }
    
    int seqIt = 0;
    int parIt = 0;
    
    // 1. Exécution de la version séquentielle classique et mesure de son temps de calcul
    double startSeq = omp_get_wtime();
    runSequentialKMeans(points, numPoints, k, seqCentroids, seqCounts, &seqIt);
    double endSeq = omp_get_wtime();
    double timeSeq = endSeq - startSeq;
    
    // 2. Exécution de la version parallèle OpenMP et mesure de son temps de calcul
    double startPar = omp_get_wtime();
    runParallelKMeans(points, numPoints, k, numThreads, parCentroids, parCounts, &parIt);
    double endPar = omp_get_wtime();
    double timePar = endPar - startPar;
    
    // Print Sequential Results
    printf("séquentiel : \n");
    printf("it : %d  \n", seqIt);
    for (int c = 0; c < k; c++) {
        printf("cluster %d : x=%f y=%f z=%f nb_elem=%d\n", 
               c, seqCentroids[c].x, seqCentroids[c].y, seqCentroids[c].z, seqCounts[c]);
    }
    
    // Print Parallel Results
    printf("parallèle :\n");
    printf("it : %d\n", parIt);
    for (int c = 0; c < k; c++) {
        printf("cluster %d : x=%f y=%f z=%f nb_elem=%d\n", 
               c, parCentroids[c].x, parCentroids[c].y, parCentroids[c].z, parCounts[c]);
    }
    
    // Print Times and Acceleration
    double speedup = timeSeq / timePar;
    printf("temps séquentiel : %f s\n", timeSeq);
    printf("temps parallèle : %f s\n", timePar);
    printf("accélération : %f\n", speedup);
    
    // Libération des ressources mémoire allouées
    free(points);
    free(seqCentroids);
    free(seqCounts);
    free(parCentroids);
    free(parCounts);
    
    return EXIT_SUCCESS;
}
