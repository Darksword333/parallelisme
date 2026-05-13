#include <stdio.h>
#include <stdlib.h>

#define MAX_NUM_OBJ 100

int num_obj = 0;
int capacity;
int M[MAX_NUM_OBJ];
int U[MAX_NUM_OBJ];

void read_problem(char *filename){
  char line[256];

  FILE *problem = fopen(filename,"r");
  if (problem == NULL){
    fprintf(stderr,"File %s not found.\n",filename);
    exit(EXIT_FAILURE);
  }

  while (fgets(line, 256, problem) != NULL){
    switch(line[0]){
    case 'c': // capacity
      if (sscanf(&(line[2]),"%d\n", &capacity) != 1){
	fprintf(stderr,"Error in file format in line:\n");
	fprintf(stderr, "%s", line);
	fclose(problem);
	exit(EXIT_FAILURE);
      }
      break;

    case 'o': // graph size
      if (num_obj >= MAX_NUM_OBJ){
	fprintf(stderr,"Too many objects (%d): limit is %d\n", num_obj, MAX_NUM_OBJ);
	fclose(problem);
	exit(EXIT_FAILURE);
      }
      if (sscanf(&(line[2]),"%d %d\n", &(M[num_obj]), &(U[num_obj])) != 2){
	fprintf(stderr,"Error in file format in line:\n");
	fprintf(stderr, "%s", line);
	fclose(problem);
	exit(EXIT_FAILURE);
      }
      else
	num_obj++;
      break;

    default:
      break;
    }
  }
  fclose(problem);
  if (num_obj == 0){
    fprintf(stderr,"Could not find any object in the problem file. Exiting.\n");
    exit(EXIT_FAILURE);
  }

}

int max(int a, int b){
    return a > b ? a : b;
}

void afficher_matrice(int S[num_obj][capacity+1]) {
    printf("     ");
    for (int j = 0; j < capacity+1; j++) printf("%4d ", j);
    printf("\n     ");
    for (int j = 0; j < capacity+1; j++) printf("-----");
    printf("\n");

    for (int i = 0; i < num_obj; i++) {
        printf("[%d] |", i);
        for (int j = 0; j < capacity+1; j++) {
            printf("%4d ", S[i][j]);
        }
        printf("\n");
    }
}

void create_mat(int M[MAX_NUM_OBJ], int U[MAX_NUM_OBJ], int S[num_obj][capacity+1]){
     // Cas particulier de la première ligne
    for (int j = 0; j < capacity+1; j++){
        if (M[0] <= j)
            S[0][j] = U[0];
        else
            S[0][j] = 0;
    }

    // Reste de la matrice
    for (int i = 1; i < num_obj; i++){
        for (int j = 0; j < capacity+1; j++){
            int old_m = S[i-1][j];
            if (M[i] <= j){
                int new_m = S[i-1][j-M[i]]+U[i];
                S[i][j] = max(old_m, new_m);
            }
            else
                S[i][j] = old_m;
        }
    }
}

int main(void){
    read_problem("pb1.txt");
    // Init
    int E[MAX_NUM_OBJ];
    int utilite_max;
    int S[MAX_NUM_OBJ][capacity+1];
    create_mat(M, U, S);
    afficher_matrice(S);

    utilite_max = S[num_obj-1][capacity];
    // Quels objets nous donne cette Utilité max ?
    int j = capacity;
    for (int i = num_obj-1; i > 0; i--){
        if (S[i][j] == S[i-1][j]){
            E[i] = 0;
        }
        else {
            E[i] = 1;
            j -= M[i];
        }
    }

    // Cas de la dernière ligne
    E[0] = (S[0][j] > 0) ? 1 : 0;

    printf("Objets Utiles :\n");
    for (int i = num_obj-1; i >= 0; i--){
        if (E[i])
            printf("%d ", i);
    }
    printf("\n");
    return 0;
}