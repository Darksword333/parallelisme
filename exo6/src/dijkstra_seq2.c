#include <stdlib.h>
#include <stdio.h>
#include <omp.h>

#define INFINITE (1<<30) // a very large positive integer

struct direct_edge_struct;
struct direct_edge_struct {
  int destination_node;
  int weight;
  struct direct_edge_struct *next;
};

int num_nodes, num_edges;
// table 'edges' is used to store all edge data
struct direct_edge_struct *edges;
// edge_counter is used to allocate entries in table 'edges'
int edge_counter = 0;
// table 'nodes' contains the direct edges out of each node
struct direct_edge_struct **nodes;
int *d;
char *P;

int main ( int argc, char **argv );
void read_graph(char *filename);
int get_distance(int node1, int node2);
void dijkstra_naive_seq();
void dijkstra_naive_omp(int num_threads);
void dijkstra_opt_seq();
void dijkstra_opt_omp(int num_threads);
void print_results(char *label, double elapsed);

/******************************************************************************/
int get_distance(int node1, int node2){
  if (node1 == node2)
    return 0;
  struct direct_edge_struct *edge = nodes[node1];
  int min_w = INFINITE;
  while (edge != NULL){
    if (edge->destination_node == node2) {
      if (edge->weight < min_w) {
        min_w = edge->weight;
      }
    }
    edge = edge->next;
  }
  return min_w;
}

/******************************************************************************/
void dijkstra_naive_seq(){
  P[0] = 1;
  for (int i = 1; i < num_nodes; i++)
    P[i] = 0;

  for (int i = 0; i < num_nodes; i++)
    d[i] = get_distance(0,i);

  for (int step = 1; step < num_nodes; step++ ){
    int shortest_dist = INFINITE;
    int nearest_node = -1;
    for (int i = 0; i < num_nodes; i++){
      if ( !P[i] && d[i] < shortest_dist ){
        shortest_dist = d[i];
        nearest_node = i;
      }
    }

    if ( nearest_node == -1 ){
      break;
    }

    P[nearest_node] = 1;
    for (int i = 0; i < num_nodes; i++){
      if ( !P[i] ){
        int dist = get_distance(nearest_node,i);
        if ( dist < INFINITE ){
          if ( d[nearest_node] + dist < d[i] )
            d[i] = d[nearest_node] + dist;
        }
      }
    }
  }
}

/******************************************************************************/
void dijkstra_naive_omp(int num_threads){
  omp_set_num_threads(num_threads);

  int max_threads = omp_get_max_threads();
  
  /* Allocations dynamiques des tableaux de reduction manuelle.
     Chaque thread va y deposer son minimum local pour eviter
     les contentions et les "data races" lors de la recherche globale. */
  int *local_min_dist = malloc(max_threads * sizeof(int));
  int *local_min_node = malloc(max_threads * sizeof(int));
  if (local_min_dist == NULL || local_min_node == NULL) {
    fprintf(stderr, "Error: memory allocation failed for thread-local arrays.\n");
    exit(-1);
  }

  int nearest_node = -1;
  int shortest_dist = INFINITE;

  /* Creation d'une region parallele unique pour tout l'algorithme.
     Cela evite le cout de creation/destruction repetitive des threads (fork-join)
     a chaque etape du parcours (qui s'execute num_nodes fois). */
  #pragma omp parallel
  {
    int tid = omp_get_thread_num();
    int actual_threads = omp_get_num_threads();

    /* Initialisation parallele du tableau des nœuds permanents P. */
    #pragma omp for
    for (int i = 1; i < num_nodes; i++)
      P[i] = 0;

    /* Le nœud source (0) est rendu permanent par un seul thread. */
    #pragma omp single
    {
      P[0] = 1;
    }

    /* Initialisation parallele des distances initiales depuis la source. */
    #pragma omp for
    for (int i = 0; i < num_nodes; i++)
      d[i] = get_distance(0,i);

    /* Boucle principale de Dijkstra executant (num_nodes - 1) etapes. */
    for (int step = 1; step < num_nodes; step++ ){
      int my_shortest_dist = INFINITE;
      int my_nearest_node = -1;

      /* Etape A : Recherche parallele du minimum local a chaque thread.
         nowait : supprime la barriere implicite de fin de boucle car on va
         se synchroniser manuellement avec un barrier apres avoir ecrit les resultats. */
      #pragma omp for nowait
      for (int i = 0; i < num_nodes; i++){
        if ( !P[i] && d[i] < my_shortest_dist ){
          my_shortest_dist = d[i];
          my_nearest_node = i;
        }
      }

      /* Sauvegarde sans conflit (data-race free) du min local dans nos tableaux partages,
         chaque thread ecrivant a son propre index tid. */
      local_min_dist[tid] = my_shortest_dist;
      local_min_node[tid] = my_nearest_node;

      /* Synchronisation obligatoire : s'assurer que tous les threads ont fini 
         leur recherche locale avant que l'on reduise les resultats. */
      #pragma omp barrier

      /* Etape B : Reduction sequentielle des resultats des threads par un seul thread.
         Cela remplace une section critical dans la boucle parallele, beaucoup plus couteuse.
         La barriere implicite a la fin du bloc single garantit que tous les threads
         recoivent la valeur finale de nearest_node avant de continuer. */
      #pragma omp single
      {
        shortest_dist = INFINITE;
        nearest_node = -1;
        for (int t = 0; t < actual_threads; t++){
          if (local_min_dist[t] < shortest_dist){
            shortest_dist = local_min_dist[t];
            nearest_node = local_min_node[t];
          }
        }
        if (nearest_node != -1) {
          P[nearest_node] = 1;
        }
      } // barriere implicite de fin de single

      /* Si aucun nœud n'est trouve (graphe non connexe), on doit s'arreter. 
         Il faut une barriere pour s'assurer que tous les threads lisent la meme valeur 
         de nearest_node avant de decider du break. */
      #pragma omp barrier

      if (nearest_node == -1) {
        break;
      }

      /* Etape C : Mise a jour parallele des distances.
         nowait : la barriere explicite juste apres garantit la coherence memoire
         avant de repasser a l'etape de recherche du minimum suivante. */
      #pragma omp for nowait
      for (int i = 0; i < num_nodes; i++){
        if ( !P[i] ){
          int dist = get_distance(nearest_node,i);
          if ( dist < INFINITE ){
            if ( d[nearest_node] + dist < d[i] )
              d[i] = d[nearest_node] + dist;
          }
        }
      }
      
      /* Synchronisation memoire : garantit que toutes les ecritures dans d[i] 
         sont terminees et visibles avant d'entamer l'etape suivante de recherche. */
      #pragma omp barrier
    }
  }

  free(local_min_dist);
  free(local_min_node);
}

/******************************************************************************/
void dijkstra_opt_seq(){
  P[0] = 1;
  for (int i = 1; i < num_nodes; i++)
    P[i] = 0;

  for (int i = 0; i < num_nodes; i++)
    d[i] = INFINITE;
  d[0] = 0;

  struct direct_edge_struct *edge = nodes[0];
  while (edge != NULL) {
    if (edge->weight < d[edge->destination_node]) {
      d[edge->destination_node] = edge->weight;
    }
    edge = edge->next;
  }

  for (int step = 1; step < num_nodes; step++ ){
    int shortest_dist = INFINITE;
    int nearest_node = -1;
    for (int i = 0; i < num_nodes; i++){
      if ( !P[i] && d[i] < shortest_dist ){
        shortest_dist = d[i];
        nearest_node = i;
      }
    }

    if ( nearest_node == -1 ){
      break;
    }

    P[nearest_node] = 1;

    edge = nodes[nearest_node];
    while (edge != NULL) {
      int dest = edge->destination_node;
      if (!P[dest]) {
        if (d[nearest_node] + edge->weight < d[dest]) {
          d[dest] = d[nearest_node] + edge->weight;
        }
      }
      edge = edge->next;
    }
  }
}

/******************************************************************************/
void dijkstra_opt_omp(int num_threads){
  omp_set_num_threads(num_threads);

  int max_threads = omp_get_max_threads();
  
  /* Allocation des tableaux de reduction manuelle pour identifier le nœud de distance minimale. */
  int *local_min_dist = malloc(max_threads * sizeof(int));
  int *local_min_node = malloc(max_threads * sizeof(int));
  if (local_min_dist == NULL || local_min_node == NULL) {
    fprintf(stderr, "Error: memory allocation failed for thread-local arrays.\n");
    exit(-1);
  }

  int nearest_node = -1;
  int shortest_dist = INFINITE;

  /* Region parallele unique creee pour tout le calcul afin de minimiser l'overhead. */
  #pragma omp parallel
  {
    int tid = omp_get_thread_num();
    int actual_threads = omp_get_num_threads();

    /* Initialisation parallele des nœuds permanents. */
    #pragma omp for
    for (int i = 1; i < num_nodes; i++)
      P[i] = 0;

    #pragma omp single
    {
      P[0] = 1;
    }

    /* Initialisation parallele du tableau des distances d a INFINITE. */
    #pragma omp for
    for (int i = 0; i < num_nodes; i++)
      d[i] = INFINITE;

    /* Seul le thread "single" initialise la source et ses voisins directs.
       Cela prend un temps proportionnel au degre de la source O(deg(0)) au lieu de O(V).
       L'utilisation de single impose une barriere a la fin du bloc, garantissant
       que l'initialisation est visible de tous les threads avant la boucle principale. */
    #pragma omp single
    {
      d[0] = 0;
      struct direct_edge_struct *edge = nodes[0];
      while (edge != NULL) {
        if (edge->weight < d[edge->destination_node]) {
          d[edge->destination_node] = edge->weight;
        }
        edge = edge->next;
      }
    } // barriere implicite

    /* Boucle principale de Dijkstra. */
    for (int step = 1; step < num_nodes; step++ ){
      int my_shortest_dist = INFINITE;
      int my_nearest_node = -1;

      /* Etape A : Recherche parallele du minimum local sur le tableau d de taille V.
         Cette operation etant en O(V), elle tire grandement parti de la parallelisation.
         nowait : pas de barriere de fin de boucle pour economiser une synchronisation. */
      #pragma omp for nowait
      for (int i = 0; i < num_nodes; i++){
        if ( !P[i] && d[i] < my_shortest_dist ){
          my_shortest_dist = d[i];
          my_nearest_node = i;
        }
      }

      /* Enregistrement du minimum local dans les tableaux de reduction. */
      local_min_dist[tid] = my_shortest_dist;
      local_min_node[tid] = my_nearest_node;

      /* Barriere obligatoire pour s'assurer que tous les threads ont ecrit. */
      #pragma omp barrier

      /* Etape B : Selection sequentielle du minimum global par un seul thread.
         Met a jour l'etat global (nearest_node) et marque le nœud comme permanent. */
      #pragma omp single
      {
        shortest_dist = INFINITE;
        nearest_node = -1;
        for (int t = 0; t < actual_threads; t++){
          if (local_min_dist[t] < shortest_dist){
            shortest_dist = local_min_dist[t];
            nearest_node = local_min_node[t];
          }
        }
        if (nearest_node != -1) {
          P[nearest_node] = 1;
        }
      } // barriere implicite

      /* Barriere de controle pour propager l'information de fin de recherche. */
      #pragma omp barrier

      if (nearest_node == -1) {
        break;
      }

      /* Etape C : Mise a jour sequentielle des voisins directs (Optimisation Algorithmique).
         Comme nous parcourons la liste d'adjacence du nœud courant, nous ne visitons que
         ses voisins (en moyenne 3 a 4 nœuds). Effectuer une distribution de boucle (omp for)
         sur si peu d'elements engendrerait un overhead parallele disproportionne.
         Nous confions donc cette mise a jour a un unique thread via omp single.
         La barriere implicite de fin de single garantit que toutes les mises a jour
         de d[i] sont visibles de tous les threads avant l'etape de recherche du minimum suivante. */
      #pragma omp single
      {
        struct direct_edge_struct *edge = nodes[nearest_node];
        while (edge != NULL) {
          int dest = edge->destination_node;
          if (!P[dest]) {
            if (d[nearest_node] + edge->weight < d[dest]) {
              d[dest] = d[nearest_node] + edge->weight;
            }
          }
          edge = edge->next;
        }
      } // barriere implicite de fin de single
    }
  }

  free(local_min_dist);
  free(local_min_node);
}

/******************************************************************************/
void print_results(char *label, double elapsed) {
  long long sum = 0;
  int reachable = 0;
  for (int i = 0; i < num_nodes; i++) {
    if (d[i] < INFINITE) {
      sum += d[i];
      reachable++;
    }
  }
  printf("%-20s | Reachable: %6d | Sum: %12lld | Time: %.6f s\n", label, reachable, sum, elapsed);
}

/******************************************************************************/
void read_graph(char *filename){
  char line[256];
  int node1, node2, weight;

  FILE *graph = fopen(filename,"r");
  if (graph == NULL){
    fprintf(stderr,"File %s not found.\n",filename);
    exit(-1);
  }

  while (fgets(line, 256, graph) != NULL){
    switch(line[0]){
      case 'c': // comment
        break;

      case 'p': // graph size
        if (sscanf(&(line[5]),"%d %d\n", &num_nodes, &num_edges) != 2){
          fprintf(stderr,"Error in file format in line:\n");
          fprintf(stderr, "%s", line);
          exit(-1);
        }
        else {
          fprintf(stderr,"Graph contains %d nodes and %d edges\n", num_nodes, num_edges);
          edges = malloc(num_edges*2 * sizeof(struct direct_edge_struct));
          if (edges == NULL){
            fprintf(stderr,"Error: cannot allocate memory.\n");
            exit(-1);
          }
          nodes = malloc(num_nodes * sizeof(struct direct_edge_struct *));
          if (nodes == NULL){
            fprintf(stderr,"Error: cannot allocate memory.\n");
            exit(-1);
          }
          for (int i=0; i<num_nodes; i++)
            nodes[i] = NULL;

          d = malloc(num_nodes * sizeof(int));
          if (d == NULL){
            fprintf(stderr,"Error: cannot allocate memory.\n");
            exit(-1);
          }
          P = malloc(num_nodes * sizeof(char));
          if (P == NULL){
            fprintf(stderr,"Error: cannot allocate memory.\n");
            exit(-1);
          }
        }
        break;

      case 'a': // edge definition
        if (sscanf(&(line[2]),"%d %d %d\n", &node1, &node2, &weight) != 3){
          fprintf(stderr,"Error in file format in line:\n");
          fprintf(stderr, "%s", line);
          exit(-1);
        }
        node1--; node2--; // number nodes from 0
        struct direct_edge_struct *new_edge;
        struct direct_edge_struct *e;
        new_edge = &edges[edge_counter++];
        new_edge->destination_node = node2;
        new_edge->weight = weight;
        new_edge->next = NULL;
        if (nodes[node1] == NULL)
          nodes[node1] = new_edge;
        else {
          e = nodes[node1];
          while (e->next != NULL)
            e = e->next;
          e->next = new_edge;
        }
        new_edge = &edges[edge_counter++];
        new_edge->destination_node = node1;
        new_edge->weight = weight;
        new_edge->next = NULL;
        if (nodes[node2] == NULL)
          nodes[node2] = new_edge;
        else {
          e = nodes[node2];
          while (e->next != NULL)
            e = e->next;
          e->next = new_edge;
        }
        break;
    }
  }
  fclose(graph);
}

/******************************************************************************/
int main ( int argc, char **argv ){
  if (argc < 2){
    fprintf(stderr,"Usage: %s <graph file name> [num_threads] [version]\n", argv[0]);
    fprintf(stderr,"  version: 0 = Naive Sequential, 1 = Naive Parallel\n");
    fprintf(stderr,"           2 = Optimized Sequential, 3 = Optimized Parallel\n");
    fprintf(stderr,"           (default: runs verification comparing all 4)\n");
    exit(-1);
  }

  read_graph(argv[1]);

  int num_threads = 4;
  if (argc >= 3){
    num_threads = atoi(argv[2]);
  }

  int version = -1;
  if (argc >= 4){
    version = atoi(argv[3]);
  }

  if (version == 0) {
    double start = omp_get_wtime();
    dijkstra_naive_seq();
    double end = omp_get_wtime();
    print_results("Naive Sequential", end - start);
  }
  else if (version == 1) {
    double start = omp_get_wtime();
    dijkstra_naive_omp(num_threads);
    double end = omp_get_wtime();
    print_results("Naive Parallel", end - start);
  }
  else if (version == 2) {
    double start = omp_get_wtime();
    dijkstra_opt_seq();
    double end = omp_get_wtime();
    print_results("Optimized Sequential", end - start);
  }
  else if (version == 3) {
    double start = omp_get_wtime();
    dijkstra_opt_omp(num_threads);
    double end = omp_get_wtime();
    print_results("Optimized Parallel", end - start);
  }
  else {
    printf("=== Verification mode on %s (Threads: %d) ===\n", argv[1], num_threads);

    // 1. Naive Sequential
    double start = omp_get_wtime();
    dijkstra_naive_seq();
    double end = omp_get_wtime();
    long long sum_seq = 0;
    int reachable_seq = 0;
    for (int i = 0; i < num_nodes; i++) {
      if (d[i] < INFINITE) {
        sum_seq += d[i];
        reachable_seq++;
      }
    }
    printf("Naive Sequential:   Reachable = %d, Sum = %lld | Time: %.6f s\n", reachable_seq, sum_seq, end - start);

    // 2. Naive Parallel
    start = omp_get_wtime();
    dijkstra_naive_omp(num_threads);
    end = omp_get_wtime();
    long long sum_naive_omp = 0;
    int reachable_naive_omp = 0;
    for (int i = 0; i < num_nodes; i++) {
      if (d[i] < INFINITE) {
        sum_naive_omp += d[i];
        reachable_naive_omp++;
      }
    }
    printf("Naive Parallel:     Reachable = %d, Sum = %lld | Time: %.6f s -> %s\n", 
           reachable_naive_omp, sum_naive_omp, end - start,
           (sum_seq == sum_naive_omp && reachable_seq == reachable_naive_omp) ? "OK" : "FAIL");

    // 3. Optimized Sequential
    start = omp_get_wtime();
    dijkstra_opt_seq();
    end = omp_get_wtime();
    long long sum_opt_seq = 0;
    int reachable_opt_seq = 0;
    for (int i = 0; i < num_nodes; i++) {
      if (d[i] < INFINITE) {
        sum_opt_seq += d[i];
        reachable_opt_seq++;
      }
    }
    printf("Optimized Seq:      Reachable = %d, Sum = %lld | Time: %.6f s -> %s\n", 
           reachable_opt_seq, sum_opt_seq, end - start,
           (sum_seq == sum_opt_seq && reachable_seq == reachable_opt_seq) ? "OK" : "FAIL");

    // 4. Optimized Parallel
    start = omp_get_wtime();
    dijkstra_opt_omp(num_threads);
    end = omp_get_wtime();
    long long sum_opt_omp = 0;
    int reachable_opt_omp = 0;
    for (int i = 0; i < num_nodes; i++) {
      if (d[i] < INFINITE) {
        sum_opt_omp += d[i];
        reachable_opt_omp++;
      }
    }
    printf("Optimized Parallel: Reachable = %d, Sum = %lld | Time: %.6f s -> %s\n", 
           reachable_opt_omp, sum_opt_omp, end - start,
           (sum_seq == sum_opt_omp && reachable_seq == reachable_opt_omp) ? "OK" : "FAIL");
  }

  free(nodes);
  free(edges);
  free(d);
  free(P);
  return 0;
}