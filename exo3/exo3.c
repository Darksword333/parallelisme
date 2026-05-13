#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#define NUM_STEPS 1000
#define NUM_OPS 50

int main(){
  int num = 0;
  #pragma omp parallel reduction(+:num)
    for (int i=0 ; i<NUM_STEPS ; i++){
      #pragma omp for
      for (int j=0 ; j<NUM_OPS ; j++)
        num++;
      #pragma omp for
      for (int j=0 ; j<NUM_OPS ; j++)
        num--;
    }
  printf("num = %d\n", num);
}