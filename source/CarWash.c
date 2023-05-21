#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "rngs.h"
#include "rvgs.h"

#define START             0.0               // initial (open the door)
#define STOP              1.5          // terminal (close the door) time
#define NODES             3
#define ext_arr           666

int seed = 13;
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

double GetInterArrival(node_id);
double GetService(node_id);
int SwitchFromNodeOne(double);
int SelectServer(node_stats);
event* GenerateEvent(event_type, node_id, int, double);
void InsertEvent(event**, event*);
event* ExtractEvent(event**);
job* GenerateJob(double, double);
void InsertJob(job**, job*);
job* ExtractJob(job**);
void init_event_list(event**);
void init_servers(server_stats**, int);
void init_nodes(node_stats**);
void init_areas(time_integrated**);
void print_event_list(event*);

int main(void)
{
  event *ev = NULL, *event_list, *new_dep, *new_arr;
  node_stats *nodes;
  time_integrated *areas;
  job *job = NULL;
  node_id actual_node;
  int actual_server;
  double current_time = START, next_time;
  int arrival_process_status[NODES] = {1, 1, 0};

  PlantSeeds(seed);
  init_event_list(&event_list);
  init_nodes(&nodes);
  init_areas(&areas);

  /*for(int i=0; i<NODES; i++){
    for(int j=0; j<servers_num[i]; j++){
      printf("nodes[%d].servers[%d].service_time = %lf\n", i, j, nodes[i].servers[j].service_time);
      printf("nodes[%d].servers[%d].served_jobs = %ld\n", i, j, nodes[i].servers[j].served_jobs);
    }
  }*/

  while(event_list != NULL){ // && there are jobs in the system (in one of his nodes)
    
    printf("-------- current_time = %lf\n\n", current_time);
    print_event_list(event_list);

    ev = ExtractEvent(&event_list);
    printf("-------------- event extracted\n");
    print_event_list(event_list);
    actual_node = ev->node;
    actual_server = ev->server;
    next_time = ev->time;

    for(int node=0; node<NODES; node++){
      areas[node].node_area += (next_time - current_time) * nodes[node].node_jobs;
      areas[node].queue_area += (next_time - current_time) * nodes[node].queue_jobs;

      printf("areas[%d].node_area = %lf\n", node, areas[node].node_area);
      printf("areas[%d].queue_area = %lf\n\n", node, areas[node].queue_area);
    }
    current_time = next_time;

    if(ev->type == job_arrival){ // process an arrival
      if(actual_server == ext_arr){
        external_arrivals++;
      }
      nodes[actual_node].arrived_jobs++;
      if(nodes[actual_node].queue_jobs < queue_len[actual_node]){ // there is availble space in queue
        job = GenerateJob(current_time, GetService(actual_node));
        if(nodes[actual_node].node_jobs < servers_num[actual_node]){
          int selected_server = SelectServer(nodes[actual_node]); // find available server
          nodes[actual_node].servers[selected_server].status = 1;
          nodes[actual_node].servers[selected_server].serving_job = job;
          new_dep = GenerateEvent(job_departure, actual_node, selected_server, current_time + job->service);
          InsertEvent(&event_list, new_dep);
          print_event_list(event_list);
          printf("cccc\n");
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

      if(actual_server == ext_arr){/* && arrival_process_status[actual_node] == 1*/ // if an external arrival is handled
        new_arr = GenerateEvent(job_arrival, actual_node, ext_arr, current_time + GetInterArrival(actual_node)); // generate next arrival event
        printf("actual node = %d\n", actual_node);
        if(new_arr->time < STOP && external_arrivals < max_processable_jobs){ // schedule event only on condition
          InsertEvent(&event_list, new_arr);
          print_event_list(event_list);
        }
        /*else{
          arrival_process_status[actual_node] = 0;
        }*/
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
        printf("nodes[%d].queue_jobs = %d\n", actual_node, nodes[actual_node].queue_jobs);
        printf("job arrival = %lf\n", nodes[actual_node].queue->arrival);
        job = ExtractJob(&(nodes[actual_node].queue));
        printf("iiii\n");
        nodes[actual_node].servers[actual_server].serving_job = job;
        nodes[actual_node].queue--;

        new_dep = GenerateEvent(job_departure, actual_node, actual_server, current_time + job->service);
        InsertEvent(&event_list, new_dep);
        print_event_list(event_list);
        printf("llll\n");
      }
      else{
        nodes[actual_node].servers[actual_server].status = 0;
      }
      
      if(actual_node == exterior_wash) { // if departure is from external_wash
        if(SwitchFromNodeOne(p)){ // switch to interior_wash
          new_arr = GenerateEvent(job_arrival, interior_wash, actual_server, current_time);
          InsertEvent(&event_list, new_arr);
          print_event_list(event_list);
          printf("mmmm\n");
        }
        else{ // switch to checkout
          new_arr = GenerateEvent(job_arrival, checkout, actual_server, current_time);
          InsertEvent(&event_list, new_arr);
          print_event_list(event_list);
          printf("nnnn\n");
        }
      }

      if(actual_node == interior_wash) { // if departure is from interior_wash
        new_arr = GenerateEvent(job_arrival, checkout, actual_server, current_time); // switch to checkout
        InsertEvent(&event_list, new_arr);
        print_event_list(event_list);
        printf("oooo\n");
      }
      printf("pppp\n");
    }

    free(ev);
    printf("zzzz\n\n");
  }

  double total_service[NODES];
  long total_served[NODES];

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
    for(int s=0; s<servers_num[k]; s++){
      printf("%8d %14f %15f %15f\n", s, nodes[k].servers[s].service_time / current_time, nodes[k].servers[s].service_time / nodes[k].servers[s].served_jobs, (double) nodes[k].servers[s].served_jobs / nodes[k].processed_jobs);
    }
    printf("\n");
  }
  printf("\n");

  return (0);
}

double GetInterArrival(node_id k)
{
  SelectStream(k);

  return Exponential(1.0/lambda[k]);
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
  while(i < node.total_servers){ // now, check the others to find which has been idle longest
    i++;
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
  }
  else{
    event *prev = NULL, *tmp = *list;
    while(tmp->next != NULL && tmp->time < new_event->time){
      prev = tmp;
      tmp = tmp->next;
    }
    if(prev != NULL){
      if(tmp->time < new_event->time){
        tmp->next = new_event;
      }
      else{
        prev->next = new_event;
        new_event->next = tmp;
      }
    }
    else{
      if(tmp->time < new_event->time){
        tmp->next = new_event;
      }
      else{
        new_event->next = *list;
        *list = new_event;
      }
    }
  }
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
  }
  else{
    job *aux = *queue;
    while(aux->next != NULL){
      aux = aux->next;
    }
    aux->next = job_to_insert;
  }
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
  event *new_arrival;

  for(int node=0; node<NODES; node++){
    if(lambda[node] != 0){
      new_arrival = GenerateEvent(job_arrival, node, ext_arr, START + GetInterArrival(node));
      InsertEvent(list, new_arrival);
    }
  }
}

void init_servers(server_stats **servers, int servers_num){
  *servers = calloc(servers_num, sizeof(server_stats));
  if(*servers == NULL){
    printf("Error allocating memory for: server_stats\n");
    exit(1);
  }
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

void print_event_list(event* event_list){
  event *tmp = event_list;
  int i=1;

  if(event_list == NULL){
    return;
  }
  
  while(tmp != NULL){
    if(tmp->type == job_arrival){
      printf("event_%d type = job_arrival\n", i);
    }
    else{
      printf("event_%d type = job_departure\n", i);
    }

    if(tmp->node == exterior_wash){
      printf("event_%d node_id = exterior_wash\n", i);
    }
    else if(tmp->node == interior_wash){
      printf("event_%d node_id = interior_wash\n", i);
    }
    else{
      printf("event_%d node_id = checkout\n", i);
    }
    printf("event_%d server = %d\n", i, tmp->server);
    printf("event_%d time = %lf\n", i, tmp->time);
    tmp = tmp->next;
    i++;
    printf("____________\n");
  }
  printf("\n");
}