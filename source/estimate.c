/* ----------------------------------------------------------------------
 * This program reads a data sample from a text file in the format
 *                         one data point per line 
 * and calculates an interval estimate for the mean of that (unknown) much 
 * larger set of data from which this sample was drawn.  The data can be 
 * either discrete or continuous.  A compiled version of this program 
 * supports redirection and can used just like program uvs.c. 
 * 
 * Name              : estimate.c  (Interval Estimation) 
 * Author            : Steve Park & Dave Geyer 
 * Language          : ANSI C 
 * Latest Revision   : 11-16-98 
 * ----------------------------------------------------------------------
 */

#include <math.h>
#include <stdio.h>
#include "rngs.h"
#include "rvgs.h"
#include "rvms.h"

#define MAX_DATA 10000
#define METRICS_NUM 3
#define LOC 0.99                       /* level of confidence, use 0.95 for 95% confidence */

double f[METRICS_NUM] = {2.0, 0.5, 0.666};

int main(void)
{
  long   n    = 0;                     /* counts data points */
  double sum[METRICS_NUM];
  double mean[METRICS_NUM];
  double stdev[METRICS_NUM];
  double u, t, w[METRICS_NUM];
  double diff;

  for(int i=0; i<METRICS_NUM; i++){
    sum[i] = 0;
    mean[i] = 0;
  }

  PlantSeeds(13);

  for(int i=0; i<METRICS_NUM; i++){                 /* use Welford's one-pass method and standard deviation        */
    SelectStream(i);
    n = 0;
    for(int j=0; j<MAX_DATA; j++){
      n++;
      diff  = Exponential(1/f[i]) - mean[i];
      sum[i]  += diff * diff * (n - 1.0) / n;
      mean[i] += diff / n;
    }
  }
  for(int j=0; j<METRICS_NUM; j++){
    stdev[j]  = sqrt(sum[j] / MAX_DATA);
  }

  u = 1.0 - 0.5 * (1.0 - LOC);                      /* interval parameter  */
  t = idfStudent(MAX_DATA - 1, u);                  /* critical value of t */
  for(int j=0; j<METRICS_NUM; j++){
    if(MAX_DATA > 1){
      w[j] = t * stdev[j] / sqrt(MAX_DATA - 1);     /* interval half width */
    }
    else{
      printf("ERROR - insufficient data\n");
    }
  }

  printf("based upon %ld data points", MAX_DATA);
  printf(" and with %d%% confidence:\n", (int) (100.0 * LOC + 0.5));

  printf("  avg interarrival = %lf +/- %lf\n", mean[0], w[0]);
  printf("  avg delay        = %lf +/- %lf\n", mean[1], w[1]);
  printf("  avg wait         = %lf +/- %lf\n", mean[2], w[2]);

  return (0);
}
