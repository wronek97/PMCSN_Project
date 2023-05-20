#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "rngs.h"
#include "rvgs.h"

#define START             0.0               // initial (open the door)
#define STOP              200          // terminal (close the door) time
#define NODES             3
#define ext_arr           666

int seed = 13;
double arrival[NODES] = {START, START, START};
long external_arrivals = 0;
long max_processable_jobs = 1 << 25;

double lambda[NODES] = {2.0, 2.0, 0.0};
double mu[NODES] = {3.0 / 4, 3.0 / 2, 5.0};
int servers_num[NODES] = {4, 2, 1};
long queue_len[NODES] = {6, 8, (long) INFINITY};
double p = 0.1;

typedef enum{
    exterior_wash,
    interior_wash,
    checkout
} node_id;

typedef enum{
    job_arrival,
    job_departure
} event_type;

typedef struct {
  double node_area;                    /* time integrated jobs in the node  */
  double queue_area;                   /* time integrated jobs in the queue */
} time_integrated;

typedef struct event{                 /* the next-event list    */
  event_type type;
  node_id node;
  int server;                         // 0 if type == job_arrival
  double time;                        /* occurrence event time      */
  struct event *next;
} event;

typedef struct job{
  double arrival;
  double service;
  struct job *next;
} job;

typedef struct {
  int    status;                   /* 0 idle or 1 busy */
  double service_time;             // total service time of the server
  long   served_jobs;              // total jobs served by the server
  double last_departure_time;
  job *serving_job;
} server_stats;

typedef struct {
  long arrived_jobs;
  long queue_jobs;
  long service_jobs;
  long node_jobs;
  long rejected_jobs;
  long processed_jobs;
  double last_arrival;                // last arrival time of a job in the node "k"
  double last_departure;
  job *queue;
  server_stats *servers;
  int total_servers;
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


double GetArrival(node_id k)
{
  SelectStream(k);

  arrival[k] += Exponential(1.0/lambda[k]);
  return arrival[k];
}
   
double GetService(node_id k)
{                 
  SelectStream(NODES + k);

  return Exponential(1.0/(mu[k]));    
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

int SelectServer(node_stats node)
{
  int s;
  int i = 0;

  while(node.servers[i].status == 1){     // find the processed_jobs of the first available
    i++;                              // (idle) server
  }       
  s = i;
  while(i < node.total_servers){          // now, check the others to find which
    i++;                              // has been idle longest
    if((node.servers[i].status == 0) && (node.servers[i].last_departure_time < node.servers[s].last_departure_time)){
      s = i;
    }
  }

  return s;
}

event* GenerateEvent(event_type type, node_id node, int server, double time)
{
  event* new_event = malloc(sizeof(event));
  if(new_event == NULL){
    printf("Error allocating memory for a job\n");
    exit(1);
  }
  new_event->type = type;
  new_event->node = node;
  new_event->server = server;
  new_event->time = time;
  new_event->next = NULL;

  return new_event;
}

void InsertEvent(event **list, event *new_event)
{
  if(*list == NULL){
    *list = new_event;
    return;
  }

  event *aux = *list;
  while(aux->next != NULL && aux->time < new_event->time){
    aux = aux->next;
  }
  aux->next = new_event;

  return;
}

event* ExtractEvent(event **list)
{
  event* next_event = NULL;

  if(*list != NULL){
    next_event = *list;
    *list = (*list)->next;
  }

  return next_event;
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

void InsertJob(job** queue, job* job_to_insert) //return 0 if queue was empty, 1 otherwise
{
  if(*queue == NULL){
    *queue = job_to_insert;
    return;
  }

  job *aux = *queue;
  while(aux->next != NULL){
    aux = aux->next;
  }
  aux->next = job_to_insert;

  return;
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

void init_event_list(event **list){
  double arrival_1 = GetArrival(exterior_wash);
  double arrival_2 = GetArrival(interior_wash);

  event *exterior_wash_arrival = GenerateEvent(job_arrival, exterior_wash, ext_arr, arrival_1);
  event *interior_wash_arrival = GenerateEvent(job_arrival, interior_wash, ext_arr, arrival_2);

  if(arrival_1 < arrival_2){
    InsertEvent(list, exterior_wash_arrival);
    InsertEvent(list, interior_wash_arrival);
  }
  else{
    InsertEvent(list, interior_wash_arrival);
    InsertEvent(list, exterior_wash_arrival);
  }

  printf("%lf - %lf\n", exterior_wash_arrival->time, interior_wash_arrival->time);
}

void init_servers(server_stats **servers, int servers_num){
  *servers = calloc(servers_num, sizeof(server_stats));
  if(*servers == NULL){
    printf("Error allocating memory for: server_stats\n");
    exit(1);
  }
  (*servers)->status = 1;
}

void init_nodes(node_stats **nodes){
  *nodes = calloc(NODES, sizeof(node_stats));
  if(*nodes == NULL){
    printf("Error allocating memory for: nodes_stats\n");
    exit(1);
  }
  for(int i=0; i<NODES; i++){
    (*nodes)[i].total_servers = servers_num[i];
    init_servers(&((*nodes)[i].servers), servers_num[i]);
  }
}

void init_areas(time_integrated **areas){
  *areas = calloc(NODES, sizeof(time_integrated));
  if(*areas == NULL){
    printf("Error allocating memory for: time_integrated\n");
    exit(1);
  }
}

int main(void)
{
  event *ev = NULL, *event_list, *new_dep, *new_arr;
  node_stats *nodes;
  time_integrated *areas;
  job *job = NULL;
  node_id actual_node;
  int actual_server;
  double current_time = 0, next_time;
  int arrival_process_status[NODES] = {1, 1, 0};

  PlantSeeds(seed);
  init_event_list(&event_list);
  init_nodes(&nodes);
  init_areas(&areas);

  while(arrival_process_status[exterior_wash] != 0 || 
        arrival_process_status[interior_wash] != 0 ||
        arrival_process_status[checkout] != 0 ||
        nodes[exterior_wash].node_jobs > 0 || nodes[interior_wash].node_jobs > 0 || nodes[checkout].node_jobs > 0){ // && there are jobs in the system (in one of his nodes)
    printf("AAAA\n");
    ev = ExtractEvent(&event_list);
    if(ev == NULL) printf("è NULL\n");
    actual_node = ev->node;
    actual_server = ev->server;

    for(int node=0; node<NODES; node++){
      areas[actual_node].node_area += (next_time - current_time) * nodes[actual_node].node_jobs;
      areas[actual_node].queue_area += (next_time - current_time) * nodes[actual_node].queue_jobs;
    }
    current_time = ev->time;
    printf("bbbb\n");
    if(ev->type == job_arrival){ // process an arrival
      external_arrivals++;
      nodes[actual_node].arrived_jobs++;
      if(nodes[actual_node].queue_jobs < queue_len[actual_node]){ // there is availble space in queue
        job = GenerateJob(current_time, GetService(actual_node));
        if(nodes[actual_node].node_jobs < servers_num[actual_node]){
          printf("cccc\n");
          int selected_server = SelectServer(nodes[actual_node]); // find available server
          nodes[actual_node].servers[selected_server].status = 1;
          nodes[actual_node].servers[selected_server].serving_job = job;
          new_dep = GenerateEvent(job_departure, actual_node, selected_server, job->service);
          InsertEvent(&event_list, new_dep);
          printf("dddd\n");
        }
        else{ // insert job in queue
          InsertJob(&(nodes[actual_node].queue), job);
          nodes[actual_node].queue_jobs++;
          printf("eeee\n");
        }
        nodes[actual_node].node_jobs++;
        nodes[actual_node].last_arrival = current_time;
      }
      else{ // reject the job
        nodes[actual_node].rejected_jobs++;
      }

      if(actual_server == ext_arr && arrival_process_status[actual_node] == 1){ // if an external arrival is handled
        new_arr = GenerateEvent(job_arrival, actual_node, ext_arr, GetArrival(actual_node)); // generate next arrival event
        printf("actual node = %d\n", actual_node);
        if(new_arr->time < STOP && external_arrivals < max_processable_jobs){ // schedule event only on condition
          InsertEvent(&event_list, new_arr);
        }
        else{
          arrival_process_status[actual_node] = 0;
        }
      }
      printf("gggg\n");
    }
    else{ //process a departure from selected server
      actual_server = ev->server;
      double service = nodes[actual_node].servers[actual_server].serving_job->service;
      nodes[actual_node].servers[actual_server].service_time += service;
      nodes[actual_node].servers[actual_server].served_jobs++;
      nodes[actual_node].servers[actual_server].last_departure_time = current_time;

      nodes[actual_node].processed_jobs++;
      nodes[actual_node].node_jobs--;
      free(nodes[actual_node].servers[actual_server].serving_job);
      printf("hhhh\n");
      if(nodes[actual_node].queue_jobs > 0){
        printf("nodes[actual_node].queue_jobs = %d\n", nodes[actual_node].queue_jobs);
        printf("job arrival = %lf\n", nodes[actual_node].queue->arrival);
        job = ExtractJob(&(nodes[actual_node].queue));
        printf("iiii\n");
        nodes[actual_node].servers[actual_server].serving_job = job;
        nodes[actual_node].queue--;

        new_dep = GenerateEvent(job_departure, actual_node, actual_server, job->service);
        InsertEvent(&event_list, new_dep);
        printf("llll\n");
      }
      else{
        nodes[actual_node].servers[actual_server].status = 0;
      }
      
      if(actual_node == exterior_wash) { // if departure is from external_wash
        if(SwitchFromNodeOne(p)){ // switch to interior_wash
          new_arr = GenerateEvent(job_arrival, interior_wash, actual_server, current_time);
          InsertEvent(&event_list, new_arr);
          printf("mmmm\n");
        }
        else{ // switch to checkout
          new_arr = GenerateEvent(job_arrival, checkout, actual_server, current_time);
          InsertEvent(&event_list, new_arr);
          printf("nnnn\n");
        }
      }

      if(actual_node == interior_wash) { // if departure is from interior_wash
        new_arr = GenerateEvent(job_arrival, checkout, actual_server, current_time); // switch to checkout
        InsertEvent(&event_list, new_arr);
        printf("oooo\n");
      }
      printf("pppp\n");
    }

    free(ev);
    printf("zzzz\n\n");
  }

  double total_service[NODES];
  double total_served[NODES];

  for(int k=0; k<NODES; k++){
    total_service[k] = 0;
    total_served[k] = 0;
    for(int s=0; s<servers_num[k]; s++){
      total_service[k] += nodes[k].servers[s].service_time;
      total_served[k] += nodes[k].servers[s].served_jobs;
    }
  }

  analysis result[NODES];
  for(int i=0; i<NODES; i++){ //TODO: elimina print di debug seguenti
    printf("processed_jobs[%d] = %ld\n", i, nodes[i].processed_jobs);
    printf("total_served[%d] = %ld\n", i, total_served[i]);
    result[i].jobs = nodes[i].processed_jobs;
    //result[i].interarrival = total_arrival[i] / processed_jobs[i];
    result[i].interarrival = (nodes[i].last_arrival - START) / nodes[i].processed_jobs;
    result[i].wait = areas[i].node_area / nodes[i].processed_jobs;
    result[i].delay = areas[i].queue_area / nodes[i].processed_jobs;
    result[i].service = total_service[i] / total_served[i];
    result[i].Ns = areas[i].node_area / current_time;
    result[i].Nq = areas[i].queue_area / current_time;
    result[i].utilization = (total_service[i] / servers_num[i]) / current_time;
    if(nodes[i].arrived_jobs == 0) result[i].ploss = 0;
    else result[i].ploss = (double) nodes[i].rejected_jobs / nodes[i].arrived_jobs;
  }

  printf("\n");
  for(int k=0; k<NODES; k++){
    printf("Node %d:\n", k+1);
    printf("    processed jobs       = %ld\n", result[k].jobs);
    printf("    avg interarrivals    = %lf\n", result[k].interarrival);
    printf("    avg wait             = %lf\n", result[k].wait);
    printf("    avg delay            = %lf\n", result[k].delay);
    printf("    avg # in node        = %lf\n", result[k].Ns);
    printf("    avg # in queue       = %lf\n", result[k].Nq);
    printf("    utilizzation         = %lf\n", result[k].utilization);
    printf("    ploss                = %.3lf%%\n", 100 * result[k].ploss);
    printf("\n  the server statistics are:\n\n");
    printf("    server     utilization     avg service        share\n");
    for(int s=1; s<=servers_num[k]; s++){
      printf("%8d %14f %15f %15f\n", s, nodes[k].servers[s].service_time / current_time, nodes[k].servers[s].service_time / nodes[k].servers[s].served_jobs, (double) nodes[k].servers[s].served_jobs / nodes[k].processed_jobs);
    }
    printf("\n");
  }
  printf("\n");

  return (0);
}
