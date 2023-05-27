#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "rngs.h"
#include "rvgs.h"
#include "rvms.h"

#define START             0.0         // initial (open the door) time
#define STOP              86400.0     // terminal (close the door) time
#define REPLICAS_NUM      100
#define NODES             4           // number of nodes in the system
#define INFINITE_CAPACITY 999999      // large number to simulate infinite queue
#define LOC 0.99                      // level of confidence, use 0.95 for 95% confidence

int seed = 13;
unsigned long external_arrivals;
unsigned long max_processable_jobs = 1 << 25;

double lambda[NODES] = {1.9, 0.8, 0.0, 0.0};
double mu[NODES] = {0.5, 0.3, 0.4, 0.77};
int servers_num[NODES] = {4, 4, 2, 2};
unsigned long queue_len[NODES] = {INFINITE_CAPACITY, INFINITE_CAPACITY, INFINITE_CAPACITY, 8};
double p[3] = {0.65, 0.2, 0.4};

typedef enum {
  flight,
  hotel,
  taxi,
  payment_control,
  outside = INFINITE_CAPACITY
} node_id;

typedef enum {
  job_arrival,
  job_departure
} event_type;

typedef enum {
  idle,
  busy
} status;

enum {
  mean,
  interval
};

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
  double *server_utilization;
  double *server_service;
  double *server_share;
} analysis;

typedef struct { // [0] = mean, [1] = confidence interval
  double interarrival[2];
  double wait[2];
  double delay[2];
  double service[2];
  double Ns[2];
  double Nq[2];
  double utilization[2];
  double ploss[2];
} statistic_analysis;

double GetInterArrival(node_id);
double GetService(node_id);
node_id SwitchNode(double*, node_id);
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
void print_result(analysis[NODES]);
void print_statistic_result(statistic_analysis[NODES]);
void loading_bar(double);

int main(void)
{
  event *ev = NULL, *event_list, *new_dep, *new_arr;
  node_stats *nodes;
  time_integrated *areas;
  job *job = NULL;
  node_id actual_node;
  int actual_server;
  double current_time = START, next_time;
  analysis result[REPLICAS_NUM][NODES];
  statistic_analysis statistic_result[NODES];

  PlantSeeds(seed);
  
  printf("Simulation in progress, please wait\n");
  loading_bar(0.0);
  for(int rep=0; rep<REPLICAS_NUM; rep++){
    external_arrivals = 0;
    init_event_list(&event_list);
    init_nodes(&nodes);
    init_areas(&areas);

    while(event_list != NULL){
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
        
        node_id switch_node = SwitchNode(p, actual_node);
        new_arr = GenerateEvent(job_arrival, switch_node, actual_server, current_time);
        InsertEvent(&event_list, new_arr);
      }

      free(ev);
    }

    // extract analysis data from simulation
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

    for(int i=0; i<NODES; i++){
      result[rep][i].jobs = nodes[i].processed_jobs;
      result[rep][i].interarrival = (nodes[i].last_arrival - START) / nodes[i].processed_jobs;
      result[rep][i].wait = areas[i].node_area / nodes[i].processed_jobs;
      result[rep][i].delay = areas[i].queue_area / nodes[i].processed_jobs;
      result[rep][i].service = total_service[i] / total_served[i];
      result[rep][i].Ns = areas[i].node_area / current_time;
      result[rep][i].Nq = areas[i].queue_area / current_time;
      result[rep][i].utilization = (total_service[i] / servers_num[i]) / current_time;
      if(nodes[i].rejected_jobs == 0 && nodes[i].processed_jobs == 0){
        result[rep][i].ploss = 0;
      }
      else{
        result[rep][i].ploss = (double) nodes[i].rejected_jobs / (nodes[i].rejected_jobs + nodes[i].processed_jobs);
      }

      result[rep][i].server_utilization = calloc(servers_num[i], sizeof(double));
      result[rep][i].server_service = calloc(servers_num[i], sizeof(double));
      result[rep][i].server_share = calloc(servers_num[i], sizeof(double));
      for(int s=0; s<servers_num[i]; s++){
        result[rep][i].server_utilization[s] = nodes[i].servers[s].service_time / current_time;
        result[rep][i].server_service[s] = nodes[i].servers[s].service_time / nodes[i].servers[s].served_jobs;
        result[rep][i].server_share[s] = (double) nodes[i].servers[s].served_jobs * 100 / nodes[i].processed_jobs;
      }
    }
    free(areas);
    free(nodes);

    loading_bar((double)(rep+1)/REPLICAS_NUM);
  }
  //printf("To clear queued jobs, the system ran for %.2lf seconds after close\n", current_time - STOP);

  double u = 1.0 - 0.5 * (1.0 - LOC);                     // interval parameter
  double t = idfStudent(REPLICAS_NUM - 1, u);             // critical value of t
  struct {
    double interarrival;
    double wait;
    double delay;
    double service;
    double Ns;
    double Nq;
    double utilization;
    double ploss;
  } sum;
  double diff;

  for(int i=0; i<NODES; i++){ // use Welford's one-pass method and standard deviation  
    statistic_result[i].interarrival[mean] = 0;
    sum.interarrival = 0;
    statistic_result[i].wait[mean] = 0;
    sum.wait = 0;
    statistic_result[i].delay[mean] = 0;
    sum.delay = 0;
    statistic_result[i].service[mean] = 0;
    sum.service = 0;
    statistic_result[i].Ns[mean] = 0;
    sum.Ns = 0;
    statistic_result[i].Nq[mean] = 0;
    sum.Nq = 0;
    statistic_result[i].utilization[mean] = 0;
    sum.utilization = 0;
    statistic_result[i].ploss[mean] = 0;
    sum.ploss = 0;

    for(int n=1; n<=REPLICAS_NUM; n++){
      diff = result[n-1][i].interarrival - statistic_result[i].interarrival[mean];
      sum.interarrival += diff * diff * (n - 1.0) / n;
      statistic_result[i].interarrival[mean] += diff / n;

      diff = result[n-1][i].wait - statistic_result[i].wait[mean];
      sum.wait += diff * diff * (n - 1.0) / n;
      statistic_result[i].wait[mean] += diff / n;
      
      diff = result[n-1][i].delay - statistic_result[i].delay[mean];
      sum.delay += diff * diff * (n - 1.0) / n;
      statistic_result[i].delay[mean] += diff / n;

      diff = result[n-1][i].service - statistic_result[i].service[mean];
      sum.service += diff * diff * (n - 1.0) / n;
      statistic_result[i].service[mean] += diff / n;

      diff = result[n-1][i].Ns - statistic_result[i].Ns[mean];
      sum.Ns += diff * diff * (n - 1.0) / n;
      statistic_result[i].Ns[mean] += diff / n;

      diff = result[n-1][i].Nq - statistic_result[i].Nq[mean];
      sum.Nq += diff * diff * (n - 1.0) / n;
      statistic_result[i].Nq[mean] += diff / n;

      diff = result[n-1][i].utilization - statistic_result[i].utilization[mean];
      sum.utilization += diff * diff * (n - 1.0) / n;
      statistic_result[i].utilization[mean] += diff / n;

      diff = result[n-1][i].ploss - statistic_result[i].ploss[mean];
      sum.ploss += diff * diff * (n - 1.0) / n;
      statistic_result[i].ploss[mean] += diff / n;
    }

    if(REPLICAS_NUM > 1){
      statistic_result[i].interarrival[interval] = t * sqrt(sum.interarrival / REPLICAS_NUM) / sqrt(REPLICAS_NUM - 1);
      statistic_result[i].wait[interval] = t * sqrt(sum.wait / REPLICAS_NUM) / sqrt(REPLICAS_NUM - 1);
      statistic_result[i].delay[interval] = t * sqrt(sum.delay / REPLICAS_NUM) / sqrt(REPLICAS_NUM - 1);
      statistic_result[i].service[interval] = t * sqrt(sum.service / REPLICAS_NUM) / sqrt(REPLICAS_NUM - 1);
      statistic_result[i].Ns[interval] = t * sqrt(sum.Ns / REPLICAS_NUM) / sqrt(REPLICAS_NUM - 1);
      statistic_result[i].Nq[interval] = t * sqrt(sum.Nq / REPLICAS_NUM) / sqrt(REPLICAS_NUM - 1);
      statistic_result[i].utilization[interval] = t * sqrt(sum.utilization / REPLICAS_NUM) / sqrt(REPLICAS_NUM - 1);
      statistic_result[i].ploss[interval] = t * sqrt(sum.ploss / REPLICAS_NUM) / sqrt(REPLICAS_NUM - 1);
    }
    else{
      printf("ERROR - insufficient data\n");
    }
  }

  //print_result(result[0]);
  print_statistic_result(statistic_result);

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

node_id SwitchNode(double *prob, node_id start_node)
{
  node_id destination_node;
  double rand;

  SelectStream(NODES * 2 + start_node);
  rand = Random();

  switch(start_node){
    case flight:
      if(rand < prob[0]){
        destination_node = payment_control;
      }
      else if(rand > 1 - prob[1]){
        destination_node = hotel;
      }
      else{
        destination_node = taxi;
      }
      break;

    case hotel:
      if(rand < prob[2]){
        destination_node = taxi;           
      }
      else{
        destination_node = payment_control;
      }
      break;
    
    case taxi:
      destination_node = payment_control;
      break;

    default:
      destination_node = outside;
      break;
  }

  return destination_node;
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
  event* new_event;

  if(node >= NODES){
    return NULL;
  }

  new_event = malloc(sizeof(event));
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
  if(new_event == NULL){
    return;
  }

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

void print_result(analysis result[NODES]){
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
      printf("%6d %15lf %15lf %15.4lf%%\n", s+1, result[k].server_utilization[s], result[k].server_service[s], result[k].server_share[s]);
    }
    printf("\n");
  }
  printf("\n");
}

void print_statistic_result(statistic_analysis result[NODES]){
  printf("\nbased upon %ld simulations and with %.2lf%% confidence:\n", REPLICAS_NUM, 100.0 * LOC);
  for(int k=0; k<NODES; k++){
    printf("Node %d:\n", k+1);
    printf("    avg interarrival     = %lf +/- %lf\n", result[k].interarrival[mean], result[k].interarrival[interval]);
    printf("    avg wait             = %lf +/- %lf\n", result[k].wait[mean], result[k].wait[interval]);
    printf("    avg delay            = %lf +/- %lf\n", result[k].delay[mean], result[k].delay[interval]);
    printf("    avg service          = %lf +/- %lf\n", result[k].service[mean], result[k].service[interval]);
    printf("    avg # in node        = %lf +/- %lf\n", result[k].Ns[mean], result[k].Ns[interval]);
    printf("    avg # in queue       = %lf +/- %lf\n", result[k].Nq[mean], result[k].Nq[interval]);
    printf("    avg utilizzation     = %lf +/- %lf\n", result[k].utilization[mean], result[k].utilization[interval]);
    printf("    ploss                = %.3lf%% +/- %.3lf%%\n", 100 * result[k].ploss[mean], 100 * result[k].ploss[interval]);
    printf("\n");
  }
  printf("\n");
}

void loading_bar(double progress){
  int i=0, percentage = (int)(100*progress);

  if(progress == 1){
    printf("\r\33[2K\033[A\33[2K");
  }
  else{
    printf("\rProgress: %3d%% [", percentage);
    while(i<percentage){
      printf("#");
      i++;
    }
    while(i<100){
      printf(" ");
      i++;
    }
    printf("]");
  }
  
  fflush(stdout);
}