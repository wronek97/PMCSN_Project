#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "rngs.h"
#include "rvgs.h"

#define START    0.0                    /* initial (open the door)        */
#define STOP     20000                /* terminal (close the door) time */
#define NODES 3

double arrival[NODES] = {START, START, START};
int seed = 13;

double lambda[NODES] = {2.0, 2.0, 2.0};
double mu[NODES] = {3.0 / 4, 3.0, 3.0};
const int servers_num[NODES] = {4, 2, 1};
double p = 0.10;

typedef enum{
    node_1,
    node_2,
    node_3
} node;

typedef struct {
  double current;                           // current time
  double next;                              // next (most imminent) event time
  double last_arrival;  // last arrival time of a job of class 'k' (global last arrival if k=PRIORITY_CLASSES)
} time;

typedef struct {
  double node;                    /* time integrated jobs in the node  */
  double queue;                   /* time integrated jobs in the queue */
  double service;                 /* time integrated jobs in service   */
} time_integrated;

typedef struct {                   // accumulated sums PER SERVER
  double service;                  // service times
  long   served;                   // node_jobs served
} accumulated_sums;

typedef struct {                        /* the next-event list    */
  double time;                          /*   next event time      */
  int    status;                        /*   event status, 0 or 1 */
} event_list;

typedef struct {
  long jobs;
  double interarrival;
  double wait;
  double delay;
  double service;
  double Ns;
  double Nq;
  double utilization;
} analysis;         

double GetArrival(node k)
{
  SelectStream(0);

  arrival[k] += Exponential(1.0/lambda[k]);
  return arrival[k];
}
   
double GetService(node k)
{                 
  SelectStream(1);

  return Exponential(1.0/(mu[k]));    
}


int NextEvent(event_list *events)
{
  int e;                                      
  int i = 0;

  while(events[i].status == 0){ // find the index of the first 'active' element in the event list
    i++;
  }
  e = i;   

  while(i < SERVERS){         /* now, check the others to find which  */
    i++;                        /* event type is most imminent          */
    if(events[k][i].status == 1 && events[k][i].time < events[k][e].time){
      e = i;
    }
  }
  return e;
}


int FindServer(event_list events, node k)
{
  int s;
  int i = 1;

  while(events[k][i].status == 1){      /* find the processed_jobs[NODES] of the first available */
    i++;                        /* (idle) server                         */
  }
  s = i;
  while(i < SERVERS){         /* now, check the others to find which   */ 
    i++;                        /* has been idle longest                 */
    if(events[k][i].status == 0 && events[k][i].time < events[k][s].time){
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
  event_list event_1[servers_num[node_1] + 1];
  event_list event_2[servers_num[node_2] + 1];
  event_list event_3[servers_num[node_3] + 1];
  long node_jobs[NODES] = {0, 0, 0};             /* jobs in the node                 */
  int e;                      /* next event processed_jobs[NODES]                   */
  int s;                      /* server processed_jobs[NODES]                       */
  long processed_jobs[NODES] = {0, 0, 0};             /* used to count processed jobs       */
  double area = 0.0;           /* time integrated node_jobs[NODES] in the node */
  struct {                           /* accumulated sums of                */
    double service;                  /*   service times                    */
    long   served;                   /*   node_jobs[NODES] served                    */
  } sum[servers_num[node_1] + 1];

  PlantSeeds(13);
  t.current = START;
  event[node_1][0].time = GetArrival(node_1);
  event[node_1][0].status = 1;
  for (s = 1; s <= SERVERS; s++) {
    event[node_1][s].time = START;          /* this value is arbitrary because */
    event[node_1][s].status = 0;              /* all servers are initially idle  */
    sum[s].service = 0.0;
    sum[s].served  = 0;
  }

  while((event[node_1][0].status != 0) || (node_jobs[node_1] != 0)){
    e = NextEvent(event, node_1);                  /* next event processed_jobs[NODES] */
    t.next = event[node_1][e].time;                        /* next event time  */
    area += (t.next - t.current) * node_jobs[node_1];     /* update integral  */
    t.current = t.next;                            /* advance the clock*/

    if(e == 0){                                  /* process an arrival*/
      node_jobs[node_1]++;
      
      if(node_jobs[node_1] <= SERVERS){
        double service = GetService(node_1);
        s = FindServer(event, node_1);
        sum[s].service += service;
        sum[s].served++;
        event[node_1][s].time = t.current + service;
        event[node_1][s].status = 1;
      }
    
      event[0].time = GetArrival(node_1);
      if(event[0].time > STOP){
        event[0].status = 0;
      }
    }
    else {                                         /* process a departure */
      s = e;
      processed_jobs[node_1]++;                                     /* from server s       */  
      node_jobs[node_1]--;
      if(node_jobs[node_1] >= SERVERS){
        double service = GetService(node_1);
        sum[s].service += service;
        sum[s].served++;
        event[s].time = t.current + service;
      }
      else{
        event[s].status = 0;
      }
    }
  }

  printf("\nfor %ld jobs the service node statistics are:\n\n", processed_jobs[node_1]);
  printf("  avg interarrivals .. = %6.4lf\n", event[0].time / processed_jobs[node_1]);
  printf("  avg wait ........... = %6.4lf\n", area / processed_jobs[node_1]);
  printf("  avg # in node ...... = %6.4lf\n", area / t.current);

  for (s = 1; s <= SERVERS; s++){       /* adjust area to calculate */ 
    area -= sum[s].service;             /* averages for the queue   */    
  }

  printf("  avg delay .......... = %6.4lf\n", area / processed_jobs[node_1]);
  printf("  avg # in queue ..... = %6.4lf\n", area / t.current);
  printf("\nthe server statistics are:\n\n");
  printf("    server     utilization     avg service        share\n");
  for (s = 1; s <= SERVERS; s++){
    printf("%8d %14.3f %15.2f %15.3f\n", s, sum[s].service / t.current, sum[s].service / sum[s].served, (double) sum[s].served / processed_jobs[node_1]);
  }
  printf("\n");

  return (0);
}
