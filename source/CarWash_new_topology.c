#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "rngs.h"
#include "rvgs.h"

#define START             0.0         // initial (open the door) time
#define STOP              72000.0       // terminal (close the door) time
#define NODES             3           // number of nodes in the system
#define INFINITE_CAPACITY 999999      // large number to simulate infinite queue
#define outside           2023        /* variable to distinguish external and internal arrivals 
                                         must be > max(all servers_num[node]) */

int seed = 13;
unsigned external_arrivals = 0;
unsigned long max_processable_jobs = 1 << 25;

double lambda[NODES] = {1.6, 0.0, 0.0};
double mu[NODES] = {2.0, 0.2, 0.1};
int servers_num[NODES] = {1, 8, 7};
unsigned long queue_len[NODES] = {INFINITE_CAPACITY, INFINITE_CAPACITY, INFINITE_CAPACITY};
double p[3] = {0.68, 0.22, 0.1};

typedef enum{
  checkout,
  exterior_wash,
  interior_wash,
  system_exit
} node_id;

typedef enum{
  job_arrival,
  job_departure
} event_type;

typedef enum{
  idle,
  busy
} status;

typedef struct {
  double node_area;         // time integrated jobs in the node
  double queue_area;        // time integrated jobs in the queue
} time_integrated;

typedef struct event{
  event_type type;
  node_id node;
  int server;               // if type == job_arrival, it's used to distinguish between external and internal arrival
  double time;              // event occurrence time
  struct event *next;
} event;

typedef struct job{
  double arrival;
  double service;
  struct job *next;
} job;

typedef struct {
  int    status;                   // 0 = idle, 1 = busy
  double service_time;             // total service time of the server
  long   served_jobs;              // total jobs served by the server
  double last_departure_time;      // required to select longest idle server
  job *serving_job;
} server_stats;

typedef struct {
  long queue_jobs;        // jobs actually in the queue
  long service_jobs;      // jobs actually in the servers
  long node_jobs;         // jobs actually in the node
  long rejected_jobs;     // jobs rejected by the limited queue
  long processed_jobs;    // jobs processed by the node
  double last_arrival;    // last arrival time of a job in the node
  job *queue;             // 
  server_stats *servers;  // 
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
int SwitchFromNode(double*, node_id);
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
void print_event(event*);
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

  PlantSeeds(seed);
  init_event_list(&event_list);
  init_nodes(&nodes);
  init_areas(&areas);

  while(event_list != NULL){ // && there are jobs in the system (in one of his nodes)
    ev = ExtractEvent(&event_list);
    actual_node = ev->node;
    actual_server = ev->server;
    next_time = ev->time;

    for(int node=0; node<NODES; node++){
      areas[node].node_area += (next_time - current_time) * nodes[node].node_jobs;
      areas[node].queue_area += (next_time - current_time) * nodes[node].queue_jobs;
    }
    current_time = next_time;

    if(ev->type == job_arrival){ // process an arrival
      if(nodes[actual_node].node_jobs < nodes[actual_node].total_servers + queue_len[actual_node]){ // there is availble space in queue
        job = GenerateJob(current_time, GetService(actual_node));
        if(nodes[actual_node].node_jobs < nodes[actual_node].total_servers){
          int selected_server = SelectServer(nodes[actual_node]); // find available server
          nodes[actual_node].servers[selected_server].status = busy;
          nodes[actual_node].servers[selected_server].serving_job = job;
          new_dep = GenerateEvent(job_departure, actual_node, selected_server, current_time + job->service);
          InsertEvent(&event_list, new_dep);
        }
        else{ // insert job in queue
          InsertJob(&(nodes[actual_node].queue), job);
          nodes[actual_node].queue_jobs++;
        }
        nodes[actual_node].node_jobs++;
        nodes[actual_node].last_arrival = current_time;
      }
      else{ // reject the job
        nodes[actual_node].rejected_jobs++;
      }

      if(actual_server == outside){
        new_arr = GenerateEvent(job_arrival, actual_node, outside, current_time + GetInterArrival(actual_node)); // generate next arrival event
        if(new_arr->time < STOP && external_arrivals < max_processable_jobs){ // schedule event only on condition
          InsertEvent(&event_list, new_arr);
          external_arrivals++;
        }
      }
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

      if(nodes[actual_node].queue_jobs > 0){
        job = ExtractJob(&(nodes[actual_node].queue));
        nodes[actual_node].servers[actual_server].serving_job = job;
        nodes[actual_node].queue_jobs--;

        new_dep = GenerateEvent(job_departure, actual_node, actual_server, current_time + job->service);
        InsertEvent(&event_list, new_dep);
      }
      else{
        nodes[actual_node].servers[actual_server].status = idle;
      }
      
      int switch_node = SwitchFromNode(p, actual_node);
      if(actual_node == checkout && switch_node != system_exit) { // if departure is from checkout
        new_arr = GenerateEvent(job_arrival, switch_node, actual_server, current_time);
        InsertEvent(&event_list, new_arr);
      }
      else if(actual_node == exterior_wash && switch_node != system_exit) { // if departure is from external_wash
        new_arr = GenerateEvent(job_arrival, switch_node, actual_server, current_time); // switch to checkout
        InsertEvent(&event_list, new_arr);
      }
    }

    free(ev);
  }
  printf("To clear the jobs left in the queue, the system ran for %.2lf minutes after close\n", current_time - STOP);

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
  for(int i=0; i<NODES; i++){
    result[i].jobs = nodes[i].processed_jobs;
    result[i].interarrival = (nodes[i].last_arrival - START) / nodes[i].processed_jobs;
    result[i].wait = areas[i].node_area / nodes[i].processed_jobs;
    result[i].delay = areas[i].queue_area / nodes[i].processed_jobs;
    result[i].service = total_service[i] / total_served[i];
    result[i].Ns = areas[i].node_area / current_time;
    result[i].Nq = areas[i].queue_area / current_time;
    result[i].utilization = (total_service[i] / servers_num[i]) / current_time;
    if(nodes[i].rejected_jobs == 0 && nodes[i].processed_jobs == 0){
      result[i].ploss = 0;
    }
    else{
      result[i].ploss = (double) nodes[i].rejected_jobs / (nodes[i].rejected_jobs + nodes[i].processed_jobs);
    }
  }

  printf("\n");
  for(int k=0; k<NODES; k++){
    printf("Node %d:\n", k+1);
    printf("    processed jobs       = %ld\n", result[k].jobs);
    printf("    avg interarrival     = %lf\n", result[k].interarrival);
    printf("    avg wait             = %lf\n", result[k].wait);
    printf("    avg delay            = %lf\n", result[k].delay);
    printf("    avg service          = %lf\n", result[k].service);
    printf("    avg # in node        = %lf\n", result[k].Ns);
    printf("    avg # in queue       = %lf\n", result[k].Nq);
    printf("    avg utilizzation     = %lf\n", result[k].utilization);
    printf("    ploss                = %.3lf%%\n", 100 * result[k].ploss);
    printf("\n  the server statistics are:\n\n");
    printf("  server     utilization     avg service        share\n");
    for(int s=0; s<servers_num[k]; s++){
      printf("%6d %15lf %15lf %15.4lf%%\n", s+1, nodes[k].servers[s].service_time / current_time, nodes[k].servers[s].service_time / nodes[k].servers[s].served_jobs, (double) nodes[k].servers[s].served_jobs * 100 / nodes[k].processed_jobs);
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

int SwitchFromNode(double *prob, node_id node)
{
  double tmp = 0;
  for(int i=0; i<3; i++){
    tmp += p[i];
  }
  if(tmp != 1){
    printf("Parameters 'p' must be between 0 and 1\n");
    exit(0);
  }

  SelectStream(NODES * 2 + node);

  if(node == checkout){
    if(Random() < prob[1]){ // switch to interior_wash node
      return interior_wash;           
    }
    else{ 
      return exterior_wash; // switch to exterior_wash node
    }
  }
  else if(node == exterior_wash){
    if(Random() < prob[2] / (prob[0] + prob[2])){ // switch to interior_wash node
      return interior_wash;           
    }
  }

  return system_exit;
}

int SelectServer(node_stats node) // find the processed_jobs of the first available (idle) server
{
  int i=0, s;
  while(node.servers[i].status == busy){     
    i++;
  }
  if(i >= node.total_servers){
    printf("Error: You are trying to serve a job when all servers are busy!!\n\n");
    exit(0);
  }

  s = i;
  for(i=s+1; i<node.total_servers; i++){
    if(node.servers[i].status == idle && node.servers[i].last_departure_time < node.servers[s].last_departure_time){
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
      new_arrival = GenerateEvent(job_arrival, node, outside, START + GetInterArrival(node));
      if(new_arrival->time < STOP && external_arrivals < max_processable_jobs){ // schedule event only on condition
        InsertEvent(list, new_arrival);
        external_arrivals++;
      }
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

void print_event(event* event){
  if(event == NULL){
    return;
  }
  
  if(event->type == job_arrival){
    printf("event type = job_arrival\n");
  }
  else{
    printf("event type = job_departure\n");
  }

  if(event->node == exterior_wash){
    printf("event node_id = exterior_wash\n");
  }
  else if(event->node == interior_wash){
    printf("event node_id = interior_wash\n");
  }
  else{
    printf("event node_id = checkout\n");
  }

  if(event->server != outside){
    printf("event server = %d\n", event->server);
  }
  else{
    printf("event server = external_enviornment\n");
  }
  printf("event time = %lf\n", event->time);
  printf("/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\\n");
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
    if(tmp->server != outside){
      printf("event_%d server = %d\n", i, tmp->server);
    }
    else{
      printf("event_%d server = external_enviornment\n", i);
    }
    printf("event_%d time = %lf\n", i, tmp->time);
    tmp = tmp->next;
    i++;
    printf("_-_-_-_-_-_-_-_-_-_-_-_-_\n");
  }
  printf("\n");
}