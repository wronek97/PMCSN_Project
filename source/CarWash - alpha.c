#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "rngs.h"
#include "rvgs.h"

#define START             0.0               // initial (open the door)
#define STOP              200000.0          // terminal (close the door) time
#define NODES             3

int seed = 13;
double arrival[NODES] = {START, START, START};
long max_processable_jobs[NODES] = {1 << 25, 1 << 25, 1 << 25};

double lambda[NODES] = {2.0, 2.0, 0.0};
double mu[NODES] = {3.0 / 4, 3.0 / 2, 5.0};
int servers_num[NODES] = {4, 2, 1};
long queue_len[NODES] = {6, 8, (long) INFINITY};
double p = 0.1;

typedef enum{
    external_wash,
    interior_wash,
    checkout
} node_ID;

typedef enum{
    job_arrival,
    job_departure
} event_type;

typedef struct {
  double node_area;                    /* time integrated jobs in the node  */
  double queue_area;                   /* time integrated jobs in the queue */
  double service_area;                 /* time integrated jobs in service   */
} time_integrated;

typedef struct event{                 /* the next-event list    */
  event_type type;
  node_ID node;
  int server;
  double time;                        /* occurrence event time      */
  struct event *next;
} event;

typedef struct job{
  double arrival;
  double service;
  struct job *next;
} job;

typedef struct {                   // accumulated sums PER SERVER
  double service;                  // service times
  long   served;                   // node_jobs served
  int    status;                   /* 0 idle or 1 busy */
} server_stats;

typedef struct {
  long spawned_jobs;
  long rejected_jobs;
  long processed_jobs;
  long node_jobs;
  long queue_jobs;
  long service_jobs;
  double last_arrival;                // last arrival time of a job in the node "k"
  double last_departure;
  job *queue;
} node_stats;

typedef struct {
  long jobs;
  double interarrival;
  double wait;
  double delay;
  double service;
  double Ns;
  double Nq;
  double utilization;
  double ploss;
} analysis;


double GetArrival(node_ID k)
{
  SelectStream(k);

  arrival[k] += Exponential(1.0/lambda[k]);
  return arrival[k];
}
   
double GetService(node_ID k)
{                 
  SelectStream(NODES + k);

  return Exponential(1.0/(mu[k]));    
}

job* GenerateJob(double arrival, double service)
{
  job* new_job = malloc(sizeof(job));
  if(new_job == NULL){
    printf("Error allocating memory for a job\n");
    exit(1);
  }
  new_job->arrival = arrival;
  new_job->service = service;
  new_job->next = NULL;

  return new_job;
}

int InsertJob(job** queue, job* job_to_insert) //return 0 if queue was empty, 1 otherwise
{
  if(*queue == NULL){
    *queue = job_to_insert;
    return 0;
  }

  job *aux = *queue;
  while(aux->next != NULL){
    aux = aux->next;
  }
  aux->next = job_to_insert;

  return 1;
}

job* ExtractJob(job **queue)
{
  job* served_job = NULL;

  if(*queue != NULL){
    served_job = *queue;
    *queue = (*queue)->next;
  }

  return served_job;
}

int SwitchFromNodeOne(double p)
{
  if(p < 0 || p > 1){
    printf("Parameter 'p' must be between 0 and 1\n");
    exit(0);
  }

  SelectStream(NODES * 2);
  if(Random() < p){   
    return 1;           // switch to interior_wash node
  }
  return 0;             // switch to checkout node
}

int main(void)
{
  double current_time = 0;
  event *ev = NULL, *event_list;
  node_stats *node;
  job *new_job = NULL;

  PlantSeeds(seed);

  while(/*current time < STOP && there are jobs in the system (in one of his nodes)*/) {
    ev = ExtractNextEvent(&event_list);
    if(ev->type == job_arrival){        // process an arrival
      if(node[ev->node].queue_jobs < node[ev->node].queue_jobs){                             /*there is availble space in queue*/
        // generate job
        node[ev->node].node_jobs++;
        node[ev->node].last_arrival = current_time;
        if(/*exists an available server*/){
          // find available server
          // assign job to the server
        }
        else{// insert job in queue
          InsertJob(&(node[ev->node].queue), new_job);
          node[ev->node].queue_jobs++;
        }
      }
      else{ // reject the job
        node[ev->node].rejected_jobs++;
      }

      // generate next arrival event (arrivals_count++)
      if(/*next arrival < STOP && arrivals_count < max_processable_jobs*/){
        //add event to the event list
      }
    }
    else{ //process a departure from server 's'
      // update variables for the analysis

      if(node_jobs[actual_node] >= servers_num[actual_node]){
        serving_job[actual_node][actual_server] = ExtractJob(&(queue[actual_node]));
        event[actual_node][actual_server].time = t.current + serving_job[actual_node][actual_server]->service;
        queue_jobs[actual_node]--;
      }
      else{
        event[actual_node][actual_server].status = 0;
      }
      
      if(/*we're processing a departure from node_1*/) { // send event to next node (2 or 3)
        if(SwitchFromNodeOne(p)){ // switch to node_2
          // generate arrival event in node_2
        }
        else{
          // generate arrival in node_3
        }
      }

      if(/*we're processing a departure from node_2*/) { // event from node 2 goes directly on node 3
        // generate arrival in node_3
      }
    }

    // update integrals
    // advance current time (current_time = next_event.time)
  }

  for(int k=0; k<NODES; k++){
    for(int s=1; s<=servers_num[k]; s++){
      total_service[k] += sum[k][s].service;
      total_served[k] += sum[k][s].served;
    }
  }
  
  for(int i=0; i<NODES; i++){
    result[i].jobs = processed_jobs[i];
    //result[i].interarrival = total_arrival[i] / processed_jobs[i];
    result[i].interarrival = (t.last_arrival[i] - START) / processed_jobs[i];
    result[i].wait = area[i] / processed_jobs[i];
    result[i].delay = delay_area[i] / processed_jobs[i];
    result[i].service = total_service[i] / total_served[i];
    result[i].Ns = area[i] / t.current;
    result[i].Nq = delay_area[i] / t.current;
    result[i].utilization = (total_service[i] / servers_num[i]) / t.current;
    if(rejected_jobs[i] == 0) result[i].ploss = 0;
    else result[i].ploss = (double) rejected_jobs[i] / spawned_jobs[i];
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
    printf("    ploss                = %lf\n", result[node].ploss);
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
