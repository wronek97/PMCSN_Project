#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "rngs.h"
#include "rvgs.h"

#define START             0.0               // initial (open the door)
#define STOP              20000.0           // terminal (close the door) time
#define NODES             3
#define PROCESSABLE_JOBS  1 << 25

double arrival[NODES] = {START, START, START};
int seed = 13;

double lambda[NODES] = {2.0, 1.0, 0.0};
double mu[NODES] = {3.0 / 4, 3.0, 1.0};
int servers_num[NODES] = {4, 2, 1};
double p = 0.10;

typedef enum{
    node_1,
    node_2,
    node_3
} node;

typedef struct {
  double current;                           // current time
  double next;                              // next (most imminent) event time
  double last_arrival[NODES];               // last arrival time of a job in the node "k"
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
  node node;
  int server;
} next_event;

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
  SelectStream(k);

  arrival[k] += Exponential(1.0/lambda[k]);
  return arrival[k];
}
   
double GetService(node k)
{                 
  SelectStream(NODES + k);

  return Exponential(1.0/(mu[k]));    
}

next_event NextEvent(event_list **events)
{
  next_event next;

  for(int node=0; node<NODES; node++){
    int i = 0;
    while(events[node][i].status == 0){ //find the index of the first 'active' element in the event list
      i++;
    }       
    next.server = i;
    while(i < servers_num[node]){ //now, check the others to find which event type is most imminent
      i++;
      if((events[node][i].status == 1) && (events[node][i].time < events[next.node][next.server].time)){
        next.node = node;
        next.server = i;
      }
    }
  }
  printf("next node = %d\nnext server = %d\nnext time %lf\n", next.node, next.server, events[next.node][next.server].time);

  return next;
}

int FindServer(event_list **event, node k)
{
  int s;
  int i = 1;

  while(event[k][i].status == 1){     // find the processed_jobs of the first available
    i++;                              // (idle) server
  }       
  s = i;
  while(i < servers_num[k]){             // now, check the others to find which
    i++;                              // has been idle longest
    if((event[k][i].status == 0) && (event[k][i].time < event[k][s].time)){
      s = i;
    }
  }

  return s;
}

void event_list_init(event_list ***list){
  *list = malloc(NODES * sizeof(event_list*));
  if(*list == NULL){
    printf("Error allocating memory for: event_list\n");
    exit(1);
  }
  for(int i=0; i<NODES; i++){
    (*list)[i] = malloc((servers_num[i] + 1) * sizeof(event_list));
    if((*list)[i] == NULL){
      printf("Error allocating memory for: event_list\n");
      exit(2);
    }
    (*list)[i][0].time = GetArrival(i);
    (*list)[i][0].status = 1;
    for(int s=1; s<=servers_num[i]; s++) {
      (*list)[i][s].time = START;
      (*list)[i][s].status = 0;
    }
  }
}

void sum_init(accumulated_sums ***list){
  *list = malloc(NODES * sizeof(accumulated_sums*));
  if(*list == NULL){
    printf("Error allocating memory for: event_list\n");
    exit(1);
  }
  for(int i=0; i<NODES; i++){
    (*list)[i] = calloc(servers_num[i] + 1, sizeof(accumulated_sums));
    if((*list)[i] == NULL){
      printf("Error allocating memory for: event_list\n");
      exit(2);
    }
  }
}

int main(void)
{
  analysis result[NODES];
  time t;
  event_list **event;
  next_event next_ev;
  long spawned_jobs = 0;
  long node_jobs[NODES] = {0, 0, 0};            // jobs in the node
  long processed_jobs[NODES] = {0, 0, 0};       // used to count processed jobs
  double area[NODES] = {0.0, 0.0, 0.0};         // time integrated jobs in the node
  double delay_area[NODES] = {0.0, 0.0, 0.0};   // time integrated jobs in the queue
  accumulated_sums **sum;
  int actual_node, actual_server;

  PlantSeeds(13);
  t.current = START;
  event_list_init(&event);
  event[node_3][0].time = INFINITY;            //node_3 has no external arrivals
  event[node_3][0].status = 0;
  sum_init(&sum);

  for(int i=0; i<NODES; i++){
    printf("event[%d][0].time   = %lf\n", i, event[i][0].time);
    printf("event[%d][0].status = %ld\n", i, event[i][0].status);
  }
  printf("\n");

  while((event[node_1][0].status != 0 || event[node_2][0].status != 0) || (node_jobs[node_1] > 0 || node_jobs[node_2] > 0 || node_jobs[node_3] > 0)){
    next_ev = NextEvent(event);                             // next event scheduled
    actual_node = next_ev.node;
    actual_server = next_ev.server;
    printf("actual_node = %d\nactual_server = %d\n" ,actual_node, actual_server);
    t.next = event[actual_node][actual_server].time;        // next event time
    for(int node=0; node<NODES; node++){
      area[node] += (t.next - t.current) * node_jobs[node];
    }
    t.current = t.next;                                     // advance the clock

    if(actual_server == 0){                                 // process an arrival
      //printf("IF\n");
      spawned_jobs++;
      node_jobs[actual_node]++;
      if(node_jobs[actual_node] <= servers_num[actual_node]){
        double service = GetService(actual_node);
        actual_server = FindServer(event, actual_node);
        sum[actual_node][actual_server].service += service;
        sum[actual_node][actual_server].served++;
        event[actual_node][actual_server].time = t.current + service;
        event[actual_node][actual_server].status = 1;
        printf("event[%d][%d].time = %lf\n", actual_node, actual_server, event[actual_node][actual_server].time);
        printf("event[%d][%d].status = %d\n", actual_node, actual_server, event[actual_node][actual_server].status);
      }
    
      event[actual_node][0].time = GetArrival(actual_node);
      if(event[actual_node][0].time > STOP || spawned_jobs >= PROCESSABLE_JOBS){
        event[actual_node][0].status = 0;
      }
    }
    else{                                     // process a departure from server 's'
      //printf("ELSE\n");
      processed_jobs[actual_node]++;
      node_jobs[actual_node]--;
      if(node_jobs[actual_node] >= servers_num[actual_node]){
        double service = GetService(actual_node);
        sum[actual_node][actual_server].service += service;
        sum[actual_node][actual_server].served++;
        event[actual_node][actual_server].time = t.current + service;
      }
      else{
        event[actual_node][actual_server].status = 0;
      }
    }

    for(int i=0; i<NODES; i++){
      //printf("node_jobs[%d] = %ld\n", i, node_jobs[i]);
    }
    printf("\n");
  }

  for(int node=0; node<NODES; node++){
    delay_area[node] = area[node];
    for(int s=1; s<=servers_num[node]; s++){       // adjust area to calculate
      delay_area[node] -= sum[node][s].service;    //averages for the queue   
    }
  }

  double total_service[NODES + 1];
  double total_served[NODES + 1];
  for(int k=0; k<=NODES; k++){
    total_service[k] = 0;
    total_served[k] = 0;
    for(int s=1; s<=servers_num[k]; s++){
      total_service[k] += sum[k][s].service;
      total_served[k] += sum[k][s].served;
    }
  }
  
  for(int i=0; i<=NODES; i++){
    result[i].jobs = processed_jobs[i];
    result[i].interarrival = (event[i][0].time - START) / processed_jobs[i];
    result[i].wait = area[i] / processed_jobs[i];
    result[i].delay = delay_area[i] / processed_jobs[i];
    result[i].service = total_service[i] / total_served[i];
    result[i].Ns = area[i] / t.current;
    result[i].Nq = delay_area[i] / t.current;
    result[i].utilization = (total_service[i] / servers_num[i]) / t.current;
  }

  printf("\n");
  for(int node=0; node<NODES; node++){
    printf("Node %d:\n", node+1);
    printf("    processed jobs       = %ld\n", result[node].jobs);
    printf("    avg interarrivals    = %lf\n", result[node].interarrival);
    printf("    avg wait             = %lf\n", result[node].wait);
    printf("    avg delay            = %lf\n", result[node].delay);
    printf("    avg # in node        = %lf\n", result[node].Ns);
    printf("    avg # in queue       = %lf\n", result[node].Nq);
    printf("    utilizzation         = %lf\n", result[node].utilization);
    printf("\n  the server statistics are:\n\n");
    printf("    server     utilization     avg service        share\n");
    for(int s=1; s<=servers_num[node]; s++){
      printf("%8d %14f %15f %15f\n", s, sum[node][s].service / t.current, sum[node][s].service / sum[node][s].served, (double) sum[node][s].served / processed_jobs[node]);
    }
    printf("\n");
  }
  printf("\n");

  return (0);
}
