#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "rngs.h"
#include "rvgs.h"

#define START             0.0               // initial (open the door)
#define STOP              200000.0          // terminal (close the door) time
#define NODES             3

double arrival[NODES] = {START, START, START};
int seed = 13;
long max_processable_jobs[NODES] = {1 << 25, 1 << 25, 1 << 25};
long queue_len[NODES] = {6, 8, (long) INFINITY};

double lambda[NODES] = {2.0, 2.0, 0.0};
double mu[NODES] = {3.0 / 4, 3.0 / 2, 5.0};
int servers_num[NODES] = {4, 2, 1};
double p = 0.1;

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

typedef struct job{
  double arrival;
  double service;
  struct job *next;
} job;

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
  double ploss;
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

next_event FindFirstEvent(event_list **events){ //find first 'active' event in the list
  next_event ev = {0, 0};
  for(int k=0; k<NODES; k++){
    for(int i=0; i <= servers_num[k]; i++){
      if(events[k][i].status == 1){
        ev.node = k;
        ev.server = i;
        return ev;
      }
    }
  }
}

next_event NextEvent(event_list **events)
{
  next_event next = FindFirstEvent(events);
  
  for(int node=next.node; node<NODES; node++){
    for(int i=0; i <= servers_num[node]; i++){ // check to find which event type is most imminent
      if((events[node][i].status == 1) && (events[node][i].time < events[next.node][next.server].time)){
        next.node = node;
        next.server = i;
      }
    }
  }

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
  while(i < servers_num[k]){          // now, check the others to find which
    i++;                              // has been idle longest
    if((event[k][i].status == 0) && (event[k][i].time < event[k][s].time)){
      s = i;
    }
  }

  return s;
}

int SwitchFromNodeOne(double p)
{
  if(p < 0 || p > 1){
    printf("Parameter 'p' must be between 0 and 1\n");
    exit(0);
  }

  SelectStream(NODES * 2);
  if(Random() < p) return 1;  // switch to node 2
  return 0;                   // switch to node 3
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
    printf("Error allocating memory for: accumulated_sums\n");
    exit(1);
  }
  for(int i=0; i<NODES; i++){
    (*list)[i] = calloc(servers_num[i] + 1, sizeof(accumulated_sums));
    if((*list)[i] == NULL){
      printf("Error allocating memory for: accumulated_sums\n");
      exit(2);
    }
  }
}

void queue_init(job ***list){
  *list = malloc(NODES * sizeof(job*));
  if(*list == NULL){
    printf("Error allocating memory for: job_queue\n");
    exit(1);
  }
  for(int i=0; i<NODES; i++){
    (*list)[i] = calloc(servers_num[i] + 1, sizeof(job*));
    if((*list)[i] == NULL){
      printf("Error allocating memory for: job_queue\n");
      exit(2);
    }
  }
}

void serving_job_init(job ****list){
  *list = malloc(NODES * sizeof(job**));
  if(*list == NULL){
    printf("Error allocating memory for: serving_job\n");
    exit(1);
  }
  for(int i=0; i<NODES; i++){
    (*list)[i] = calloc(servers_num[i] + 1, sizeof(job*));
    if((*list)[i] == NULL){
      printf("Error allocating memory for: serving_job\n");
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
  long spawned_jobs[NODES] = {0, 0, 0};
  long node_jobs[NODES] = {0, 0, 0};            // jobs in the node (service + queue)
  long queue_jobs[NODES] = {0, 0, 0};           // jobs in queue
  long processed_jobs[NODES] = {0, 0, 0};       // used to count processed jobs
  long rejected_jobs[NODES] = {0, 0, 0};        // jobs rejected from node's queue
  double area[NODES] = {0.0, 0.0, 0.0};         // time integrated jobs in the node
  double delay_area[NODES] = {0.0, 0.0, 0.0};   // time integrated jobs in the queue
  accumulated_sums **sum;
  int actual_node, actual_server;

  job **queue;
  job *new_job = NULL, ***serving_job;

  double total_arrival[NODES + 1];
  double total_service[NODES + 1];
  double total_served[NODES + 1];
  for(int k=0; k<NODES; k++){
    total_arrival[k] = 0;
    total_service[k] = 0;
    total_served[k] = 0;
  }

  PlantSeeds(13);
  t.current = START;
  event_list_init(&event);
  event[node_3][0].time = INFINITY;            //node_3 has no external arrivals
  event[node_3][0].status = 0;
  sum_init(&sum);
  queue_init(&queue);
  serving_job_init(&serving_job);

  while(event[node_1][0].status != 0 || event[node_2][0].status != 0 || 
        node_jobs[node_1] > 0 || node_jobs[node_2] > 0 || node_jobs[node_3] > 0) {

    next_ev = NextEvent(event);                             // next event scheduled
    actual_node = next_ev.node;
    actual_server = next_ev.server;
    t.next = event[actual_node][actual_server].time;        // next event time
    for(int node=0; node<NODES; node++){
      area[node] += (t.next - t.current) * node_jobs[node];
      delay_area[node] += (t.next - t.current) * queue_jobs[node];
    }
    t.current = t.next;                                     // advance the clock

    if(actual_server == 0){                                 // process an arrival
      
      // da mettere in un metodo e gli passiamo il nodo con tutti i dati
      new_job = GenerateJob(t.current, GetService(actual_node));
      spawned_jobs[actual_node]++;
      if(queue_jobs[actual_node] < queue_len[actual_node]) {  // there is space in queue so we can add a job
        node_jobs[actual_node]++;
        t.last_arrival[actual_node] = t.current;
        if(node_jobs[actual_node] <= servers_num[actual_node]){
          actual_server = FindServer(event, actual_node);
          serving_job[actual_node][actual_server] = new_job;
          event[actual_node][actual_server].time = t.current + serving_job[actual_node][actual_server]->service;
          event[actual_node][actual_server].status = 1;
        }
        else{
          InsertJob(&(queue[actual_node]), new_job);
          queue_jobs[actual_node]++;
        }
        // increment total arrival time after dispatching the new job
        total_arrival[actual_node] += new_job->arrival - START;
      }
      else rejected_jobs[actual_node]++;
      /////////

      event[actual_node][0].time = GetArrival(actual_node);
      if(event[actual_node][0].time > STOP || spawned_jobs[actual_node] >= max_processable_jobs[actual_node]){
        event[actual_node][0].status = 0;
      }
    }
    else{ //process a departure from server 's'
      sum[actual_node][actual_server].service += serving_job[actual_node][actual_server]->service;
      sum[actual_node][actual_server].served++;
      processed_jobs[actual_node]++;
      node_jobs[actual_node]--;
      free(serving_job[actual_node][actual_server]);

      if(node_jobs[actual_node] >= servers_num[actual_node]){
        serving_job[actual_node][actual_server] = ExtractJob(&(queue[actual_node]));
        event[actual_node][actual_server].time = t.current + serving_job[actual_node][actual_server]->service;
        queue_jobs[actual_node]--;
      }
      else event[actual_node][actual_server].status = 0;


      // send event to next node (2 or 3)
      if(actual_node == node_1) {
        if(SwitchFromNodeOne(p)) {
          new_job = GenerateJob(t.current, GetService(node_2));
          spawned_jobs[node_2]++;
          if(queue_jobs[node_2] < queue_len[node_2]) {  // there is space in queue so we can add a job
            node_jobs[node_2]++;

            // check if dispatched event is previous than new arrival on node 2
            if (t.last_arrival[node_2] < t.current) t.last_arrival[node_2] = t.current;
            
            if(node_jobs[node_2] <= servers_num[node_2]){
              actual_server = FindServer(event, node_2);
              serving_job[node_2][actual_server] = new_job;
              event[node_2][actual_server].time = t.current + serving_job[node_2][actual_server]->service;
              event[node_2][actual_server].status = 1;
            }
            else{
              InsertJob(&(queue[node_2]), new_job);
              queue_jobs[node_2]++;
            }
            total_arrival[node_2] += new_job->arrival - START;
          }
          else rejected_jobs[node_2]++;
        }
        else {
          new_job = GenerateJob(t.current, GetService(node_3));
          spawned_jobs[node_3]++;
          if(queue_jobs[node_3] < queue_len[node_3]) {  // there is space in queue so we can add a job
            node_jobs[node_3]++;
            t.last_arrival[node_3] = t.current;
            if(node_jobs[node_3] <= servers_num[node_3]){
              actual_server = FindServer(event, node_3);
              serving_job[node_3][actual_server] = new_job;
              event[node_3][actual_server].time = t.current + serving_job[node_3][actual_server]->service;
              event[node_3][actual_server].status = 1;
            }
            else{
              InsertJob(&(queue[node_3]), new_job);
              queue_jobs[node_3]++;
            }
            total_arrival[node_3] += new_job->arrival - START;
          }
          else rejected_jobs[node_3]++;
        }
      }

      // event from node 2 goes directly on node 3
      if(actual_node == node_2) {
        new_job = GenerateJob(t.current, GetService(node_3));
        spawned_jobs[node_3]++;
        if(queue_jobs[node_3] < queue_len[node_3]) {  // there is space in queue so we can add a job
          node_jobs[node_3]++;

          // check if dispatched event is previous than new arrival on node 3
          if (t.last_arrival[node_3] < t.current) t.last_arrival[node_3] = t.current;
          
          if(node_jobs[node_3] <= servers_num[node_3]){
            actual_server = FindServer(event, node_3);
            serving_job[node_3][actual_server] = new_job;
            event[node_3][actual_server].time = t.current + serving_job[node_3][actual_server]->service;
            event[node_3][actual_server].status = 1;
          }
          else{
            InsertJob(&(queue[node_3]), new_job);
            queue_jobs[node_3]++;
          }
          total_arrival[node_3] += new_job->arrival - START;
        }
        else rejected_jobs[node_3]++;
      }



    }
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
