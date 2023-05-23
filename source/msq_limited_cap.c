#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "rngs.h"
#include "rvgs.h"

#define START    0.0                    /* initial (open the door)        */
#define STOP     20000.0                /* terminal (close the door) time */
#define SERVERS  1                      /* number of servers              */

double lambda = 2.0;
double mu = 3.0;
int queue_len = 2;
double arrival = START;

typedef struct {                        /* the next-event list    */
  double t;                             /*   next event time      */
  int    x;                             /*   event status, 0 or 1 */
} event_list[SERVERS + 1];              

double GetArrival(void)
{     
  SelectStream(0);

  arrival += Exponential(1.0/lambda);
  return arrival;
}
   
double GetService(void)
{                 
  SelectStream(1);

  return Exponential(1.0/(mu/SERVERS));    
}


int NextEvent(event_list event)
{
  int e;                                      
  int i = 0;

  while (event[i].x == 0)       /* find the index of the first 'active' */
    i++;                        /* element in the event list            */ 
  e = i;                        
  while (i < SERVERS) {         /* now, check the others to find which  */
    i++;                        /* event type is most imminent          */
    if ((event[i].x == 1) && (event[i].t < event[e].t))
      e = i;
  }
  return (e);
}


int FindServer(event_list event)
{
  int s;
  int i = 1;

  while(event[i].x == 1){      /* find the index of the first available */
    i++;                        /* (idle) server                         */
  }
  s = i;
  while(i < SERVERS){         /* now, check the others to find which   */ 
    i++;                        /* has been idle longest                 */
    if((event[i].x == 0) && (event[i].t < event[s].t)){
      s = i;
    }
  }
  return s;
}


int main(void)
{
  struct {
    double current;                  /* current time                       */
    double next;                     /* next (most imminent) event time    */
  } t;
  event_list event;
  long number = 0;                   /* number in the node                 */
  long rejected = 0;
  int e;                             /* next event index                   */
  int s;                             /* server index                       */
  long index = 0;                    /* used to count processed jobs       */
  double area = 0.0;                 /* time integrated number in the node */
  struct {                           /* accumulated sums of                */
    double service;                  /* service times                      */
    long   served;                   /* number served                      */
  } sum[SERVERS + 1];

  PlantSeeds(13);
  t.current = START;
  event[0].t = GetArrival();
  event[0].x = 1;
  for (s = 1; s <= SERVERS; s++) {
    event[s].t = START;          /* this value is arbitrary because */
    event[s].x = 0;              /* all servers are initially idle  */
    sum[s].service = 0.0;
    sum[s].served  = 0;
  }

  while((event[0].x != 0) || (number != 0)){
    e = NextEvent(event);                  /* next event index */
    t.next = event[e].t;                        /* next event time  */
    area += (t.next - t.current) * number;     /* update integral  */
    t.current = t.next;                            /* advance the clock*/

    if(e == 0){                                  /* process an arrival*/
      if(number <= queue_len){
        if(number < SERVERS){
          double service = GetService();
          s = FindServer(event);
          sum[s].service += service;
          sum[s].served++;
          event[s].t = t.current + service;
          event[s].x = 1;
        }
        number++;
      }
      else{
        rejected++;
      }
      event[0].t = GetArrival();
        if (event[0].t > STOP){
          event[0].x = 0;
        }
    }
    else {                                         /* process a departure */
      index++;                                     /* from server s       */  
      number--;
      s = e;                       
      if(number >= SERVERS){
        double service = GetService();
        sum[s].service += service;
        sum[s].served++;
        event[s].t = t.current + service;
      }
      else{
        event[s].x = 0;
      }
    }
  }

  printf("\nfor %ld jobs the service node statistics are:\n\n", index);
  printf("  avg interarrivals .. = %6.4lf\n", (event[0].t - START) / index);
  printf("  avg wait ........... = %6.4lf\n", area / index);
  printf("  avg # in node ...... = %6.4lf\n", area / t.current);

  for (s = 1; s <= SERVERS; s++){       /* adjust area to calculate */ 
    area -= sum[s].service;             /* averages for the queue   */    
  }

  printf("  avg delay .......... = %6.4lf\n", area / index);
  printf("  avg # in queue ..... = %6.4lf\n", area / t.current);
  printf("  ploss .............. = %6.3lf\n", (double) rejected * 100 / (rejected + index));
  printf("\nthe server statistics are:\n\n");
  printf("    server     utilization     avg service        share\n");
  for (s = 1; s <= SERVERS; s++){
    printf("%8d %14.3f %15.2f %15.3f\n", s, sum[s].service / t.current, sum[s].service / sum[s].served, (double) sum[s].served / index);
  }
  printf("\n");

  return (0);
}