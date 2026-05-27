#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>

// Structure représentant un pixel en couleur (RGB)
typedef struct ColorPixel {
    unsigned char r;
    unsigned char g;
    unsigned char b;
} ColorPixel;

// Structure représentant une image en couleur
typedef struct ColorImage {
    int width;
    int height;
    ColorPixel *pixels;
} ColorImage;

// Structure représentant une image en niveaux de gris
typedef struct GreyImage {
    int width;
    int height;
    unsigned char *pixels;
} GreyImage;

/**********************************************************************/

// Fonction pour charger une image au format PPM (P3 ASCII)
ColorImage *loadColorImage(const char *fileName) {
    char format[8];
    int width, height, maxValue;
    ColorImage *image = NULL;
    
    FILE *f = fopen(fileName, "r");
    if (!f) {
        fprintf(stderr, "Erreur : impossible d'ouvrir le fichier %s...\n", fileName);
        exit(EXIT_FAILURE);
    }
    
    // Lecture du format de l'image
    if (fscanf(f, "%7s\n", format) != 1) {
        fprintf(stderr, "Erreur : format de fichier invalide dans %s\n", fileName);
        fclose(f);
        exit(EXIT_FAILURE);
    }
    
    // Le format doit être P3
    if (format[0] != 'P' || format[1] != '3') {
        fprintf(stderr, "Erreur : ce programme ne supporte que le format PPM ASCII (P3)\n");
        fclose(f);
        exit(EXIT_FAILURE);
    }
    
    // Sauter les commentaires commençant par '#'
    int c = fgetc(f);
    while (c == '#') {
        while ((c = fgetc(f)) != '\n' && c != EOF);
        c = fgetc(f);
    }
    if (c != EOF) {
        ungetc(c, f);
    }
    
    // Lecture des dimensions et valeur max
    if (fscanf(f, "%d %d\n", &width, &height) != 2) {
        fprintf(stderr, "Erreur : impossible de lire les dimensions de l'image %s\n", fileName);
        fclose(f);
        exit(EXIT_FAILURE);
    }
    if (fscanf(f, "%d\n", &maxValue) != 1) {
        fprintf(stderr, "Erreur : impossible de lire la valeur maximale du pixel dans %s\n", fileName);
        fclose(f);
        exit(EXIT_FAILURE);
    }
    
    // Allocation de la structure image
    image = malloc(sizeof(ColorImage));
    if (image == NULL) {
        fprintf(stderr, "Erreur : échec de l'allocation mémoire pour l'image couleur\n");
        fclose(f);
        exit(EXIT_FAILURE);
    }
    
    image->width = width;
    image->height = height;
    
    // Allocation du tableau de pixels
    image->pixels = malloc(width * height * sizeof(ColorPixel));
    if (image->pixels == NULL) {
        fprintf(stderr, "Erreur : échec de l'allocation de %d pixels couleur\n", width * height);
        free(image);
        fclose(f);
        exit(EXIT_FAILURE);
    }
    
    // Lecture des valeurs de pixels (R G B)
    for (int i = 0; i < width * height; i++) {
        int r, g, b;
        if (fscanf(f, "%d %d %d", &r, &g, &b) != 3) {
            fprintf(stderr, "Erreur : données de pixel incomplètes à l'index %d dans %s\n", i, fileName);
            free(image->pixels);
            free(image);
            fclose(f);
            exit(EXIT_FAILURE);
        }
        image->pixels[i].r = (unsigned char)r;
        image->pixels[i].g = (unsigned char)g;
        image->pixels[i].b = (unsigned char)b;
    }
    
    fclose(f);
    return image;
}

/**********************************************************************/

// Fonction pour allouer une image en niveaux de gris
GreyImage *createGreyImage(int width, int height) {
    GreyImage *image = malloc(sizeof(GreyImage));
    if (image == NULL) {
        fprintf(stderr, "Erreur : échec de l'allocation mémoire pour la structure image gris\n");
        exit(EXIT_FAILURE);
    }
    
    image->width = width;
    image->height = height;
    
    image->pixels = malloc(width * height * sizeof(unsigned char));
    if (image->pixels == NULL) {
        fprintf(stderr, "Erreur : échec de l'allocation mémoire pour les pixels image gris\n");
        free(image);
        exit(EXIT_FAILURE);
    }
    
    return image;
}

/**********************************************************************/

// Fonction pour sauvegarder une image gris au format PGM (P2 ASCII)
void saveGreyImage(const char *fileName, const GreyImage *image) {
    FILE *f = fopen(fileName, "w");
    if (!f) {
        fprintf(stderr, "Erreur : impossible de créer le fichier de sortie %s\n", fileName);
        exit(EXIT_FAILURE);
    }
    
    fprintf(f, "P2\n%d %d\n255\n", image->width, image->height);
    for (int i = 0; i < image->width * image->height; i++) {
        fprintf(f, "%d\n", image->pixels[i]);
    }
    
    fclose(f);
}

/**********************************************************************/

// Libération de la mémoire de l'image couleur
void freeColorImage(ColorImage *image) {
    if (image != NULL) {
        if (image->pixels != NULL) {
            free(image->pixels);
        }
        free(image);
    }
}

// Libération de la mémoire de l'image gris
void freeGreyImage(GreyImage *image) {
    if (image != NULL) {
        if (image->pixels != NULL) {
            free(image->pixels);
        }
        free(image);
    }
}

/**********************************************************************/

// --- Versions Séquentielles ---

// Conversion couleur vers niveaux de gris séquentielle
void colorToGreySeq(const ColorImage *colImg, GreyImage *greyImg) {
    int size = colImg->width * colImg->height;
    for (int i = 0; i < size; i++) {
        const ColorPixel *pix = &(colImg->pixels[i]);
        greyImg->pixels[i] = (unsigned char)((299 * pix->r + 587 * pix->g + 114 * pix->b) / 1000);
    }
}

// Égalisation d'histogramme séquentielle
void enhanceContrastSeq(GreyImage *image) {
    int size = image->width * image->height;
    long long h[256] = {0};
    long long c[256] = {0};
    
    // 1. Calcul de l'histogramme
    for (int i = 0; i < size; i++) {
        h[image->pixels[i]]++;
    }
    
    // 2. Calcul de l'histogramme cumulé
    c[0] = h[0];
    for (int i = 1; i < 256; i++) {
        c[i] = c[i - 1] + h[i];
    }
    
    // 3. Transformation des pixels in-place
    for (int i = 0; i < size; i++) {
        // Cast en long long pour éviter l'overflow lors de la multiplication
        image->pixels[i] = (unsigned char)((255 * (long long)c[image->pixels[i]]) / size);
    }
}

/**********************************************************************/

// --- Versions Parallélisées avec OpenMP ---

// Conversion couleur vers niveaux de gris en parallèle
void colorToGreyOmp(const ColorImage *colImg, GreyImage *greyImg, int numThreads) {
    int size = colImg->width * colImg->height;
    omp_set_num_threads(numThreads);
    
    #pragma omp parallel for
    for (int i = 0; i < size; i++) {
        const ColorPixel *pix = &(colImg->pixels[i]);
        greyImg->pixels[i] = (unsigned char)((299 * pix->r + 587 * pix->g + 114 * pix->b) / 1000);
    }
}

// Égalisation d'histogramme parallèle (Région parallèle unique)
void enhanceContrastOmp(GreyImage *image, int numThreads) {
    int size = image->width * image->height;
    long long h[256] = {0};
    long long c[256] = {0};
    
    omp_set_num_threads(numThreads);
    
    #pragma omp parallel
    {
        // Histogramme local à chaque thread
        long long localH[256] = {0};
        
        // Remplissage de l'histogramme local en parallèle
        #pragma omp for nowait
        for (int i = 0; i < size; i++) {
            localH[image->pixels[i]]++;
        }
        
        // Fusion des histogrammes locaux dans l'histogramme global en exclusion mutuelle
        #pragma omp critical
        {
            for (int k = 0; k < 256; k++) {
                h[k] += localH[k];
            }
        }
        
        // Attente que tous les threads aient terminé de fusionner
        #pragma omp barrier
        
        // Calcul de l'histogramme cumulé par un seul thread
        #pragma omp single
        {
            c[0] = h[0];
            for (int i = 1; i < 256; i++) {
                c[i] = c[i - 1] + h[i];
            }
        } // Barrière implicite ici à la fin du bloc single
        
        // Transformation des pixels en parallèle
        #pragma omp for
        for (int i = 0; i < size; i++) {
            image->pixels[i] = (unsigned char)((255 * (long long)c[image->pixels[i]]) / size);
        }
    }
}

/**********************************************************************/

int main(int argc, char **argv) {
    ColorImage *colImg = NULL;
    GreyImage *greyImg = NULL;
    
    if (argc < 3) {
        printf("Usage: %s <input PPM image> <output PGM image> [numThreads]\n", argv[0]);
        printf("  numThreads = 0 : Version Séquentielle (par défaut)\n");
        printf("  numThreads > 0 : Version Parallèle (OpenMP) avec le nombre de threads indiqué\n");
        return EXIT_FAILURE;
    }
    
    char *inputFile = argv[1];
    char *outputFile = argv[2];
    int numThreads = 0;
    
    if (argc >= 4) {
        numThreads = atoi(argv[3]);
    }
    
    // Chargement de l'image couleur
    colImg = loadColorImage(inputFile);
    // Création de l'image gris vide
    greyImg = createGreyImage(colImg->width, colImg->height);
    
    double start, mid, end;
    
    if (numThreads > 0) {
        // --- Exécution Parallèle ---
        start = omp_get_wtime();
        colorToGreyOmp(colImg, greyImg, numThreads);
        mid = omp_get_wtime();
        enhanceContrastOmp(greyImg, numThreads);
        end = omp_get_wtime();
    } else {
        // --- Exécution Séquentielle ---
        start = omp_get_wtime();
        colorToGreySeq(colImg, greyImg);
        mid = omp_get_wtime();
        enhanceContrastSeq(greyImg);
        end = omp_get_wtime();
    }
    
    // Calcul des durées
    double toGreyTime = mid - start;
    double contrastTime = end - mid;
    double totalTime = end - start;
    
    // Affichage des temps (séparés par des virgules pour faciliter l'analyse par script)
    printf("%f,%f,%f\n", toGreyTime, contrastTime, totalTime);
    
    // Sauvegarde de l'image finale
    saveGreyImage(outputFile, greyImg);
    
    // Libération de la mémoire
    freeColorImage(colImg);
    freeGreyImage(greyImg);
    
    return EXIT_SUCCESS;
}