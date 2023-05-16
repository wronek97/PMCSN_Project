#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "rngs.h"
#include "rvgs.h"

#define START             0.0                 /* initial time                   */
#define STOP              20000.0             /* terminal (close the door) time */
#define PRIORITY_CLASSES  2
#define PROCESSABLE_JOBS  1 << 25

double lambda = 2.0;
double mu = 3.0;
int servers_num = 4;
int seed = 13;

double arrival = START;

typedef struct {
  double current;                           // current time
  double next;                              // next (most imminent) event time
  double last_arrival[PRIORITY_CLASSES+1];  // last arrival time of a job of class 'k' (global last arrival if k=PRIORITY_CLASSES)
} time;

typedef struct {
  double node;                    /* time integrated jobs in the node  */
  double queue;                   /* time integrated jobs in the queue */
  double service;                 /* time integrated jobs in service   */
} time_integrated;

typedef struct {                   // accumulated sums per server
  double service;                  // service times
  long   served;                   // node_jobs served
} accumulated_sums;

typedef struct {                        /* the next-event list    */
  double time;                             /*   next event time      */
  int    status;                             /*   event status, 0 or 1 */
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

typedef struct job{
  double size;
  int priority;
  struct job *next;
} job;

job* GenerateJob(double size, int priority)
{
  job* new_job = malloc(sizeof(job));
  if(new_job == NULL){
    printf("Error allocating memory for a job\n");
    exit(1);
  }
  new_job->size = size;
  new_job->priority = priority;
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

job* ExtractJob(job **priority_queue)
{
  job* served_job = NULL;
  int class = 0;

  while(priority_queue[class] == NULL){
    class++;
  }
  if(class < PRIORITY_CLASSES){
    served_job = priority_queue[class];
    priority_queue[class] = priority_queue[class]->next;
  }

  return served_job;
}

void init_queues(job ***queue){
  *queue = calloc(PRIORITY_CLASSES, sizeof(job*));
  if(*queue == NULL){
    printf("Error allocating memory for queues\n");
    exit(2);
  }
}

int NextEvent(next_event *events){
  int e;                                      
  int i = 0;

  while(events[i].status == 0){ //find the index of the first 'active' element in the event list
    i++;
  }       
  e = i;

  while(i < servers_num){ //now, check the others to find which event type is most imminent
    i++;
    if((events[i].status == 1) && (events[i].time < events[e].time)){
      e = i;
    }
  }

  return e;
}

int FindServer(next_event *event)
{
  int s;
  int i = 1;

  while(event[i].status == 1){       /* find the processed_jobs of the first available */
    i++;                        /* (idle) server                         */
  }       
  s = i;
  while(i < servers_num){     /* now, check the others to find which   */ 
    i++;                        /* has been idle longest                 */
    if((event[i].status == 0) && (event[i].time < event[s].time)){
      s = i;
    }
  }

  return s;
}

double GetArrival()
{
  SelectStream(0); 

  arrival += Exponential(1.0/lambda);
  return arrival;
} 

double GetService()
{
  SelectStream(1);

  return Exponential(1.0/(mu / servers_num));
}

int SelectPriorityClass(double *boundaries, double size)
{
  int i;

  SelectStream(2);

  for(i=0; i<PRIORITY_CLASSES; i++){
    if(size < boundaries[i]){
      return i;
    }
  }

  return PRIORITY_CLASSES-1;
}

int main(void){
  analysis result[PRIORITY_CLASSES+1];
  time t;
  next_event event[servers_num + 1];
  int e;                                      // next event index
  int s;                                      // server index
  time_integrated area[PRIORITY_CLASSES+1];   // time integrated variables
  long spawned_jobs = 0;
  long processed_jobs[PRIORITY_CLASSES+1];    // departed jobs of priority 'k' (all priorities if k=PRIORITY_CLASSES)
  long node_jobs[PRIORITY_CLASSES+1];         // jobs of priority 'k'  in the node (all priorities if k=PRIORITY_CLASSES)
  long priority_queue_num[PRIORITY_CLASSES+1];
  accumulated_sums sum[servers_num + 1][PRIORITY_CLASSES+1];
  double service;
  int selected_class;
  job **priority_queue;
  job *new_job = NULL, *serving_job[servers_num + 1];
  double class_boundaries[PRIORITY_CLASSES-1] = {1.0/(mu / servers_num)}; // E(S)

  //init enviornment
  
  for(int i=0; i<=PRIORITY_CLASSES; i++){
    area[i].node = 0.0;
    area[i].queue = 0.0;
    node_jobs[i] = 0;
    processed_jobs[i] = 0;
    priority_queue_num[i] = 0;
  }
  init_queues(&priority_queue);

  PlantSeeds(seed);
  t.current = START;
  event[0].time = GetArrival(lambda);
  event[0].status = 1;
  for(s = 1; s <= servers_num; s++){
    event[s].time = START;            /* this value is arbitrary because */
    event[s].status = 0;              /* all servers are initially idle  */
    for(int k=0; k<=PRIORITY_CLASSES; k++){
      sum[s][k].service = 0.0;
      sum[s][k].served = 0;
    }
    serving_job[s] = NULL;
  }

  while(event[0].status != 0 || node_jobs[PRIORITY_CLASSES] > 0) {
    e = NextEvent(event);                                                   // next event index
    t.next = event[e].time;                                          // next event time
    for(int i=0; i<=PRIORITY_CLASSES; i++){
      area[i].node += (t.next - t.current) * node_jobs[i];           // update integral
      area[i].queue += (t.next - t.current) * priority_queue_num[i];
    }
    t.current = t.next;                                                     // advance the clock

    if(e == 0){ //process an arrival
      service = GetService();
      selected_class = SelectPriorityClass(class_boundaries, service);
      new_job = GenerateJob(service, selected_class);
      spawned_jobs++;
      node_jobs[PRIORITY_CLASSES]++;
      node_jobs[selected_class]++;
      t.last_arrival[selected_class] = t.current;
      t.last_arrival[PRIORITY_CLASSES] = t.current;
      
      if(node_jobs[PRIORITY_CLASSES] <= servers_num){
        s = FindServer(event);
        serving_job[s] = new_job;
        event[s].time = t.current + serving_job[s]->size;
        event[s].status = 1;
      }
      else{
        InsertJob(&priority_queue[selected_class], new_job);
        priority_queue_num[PRIORITY_CLASSES]++;
        priority_queue_num[selected_class]++;
      }

      event[0].time = GetArrival(lambda);
      if(event[0].time > STOP || spawned_jobs >= PROCESSABLE_JOBS){
        event[0].status = 0;
      }
    }
    else{ //process a departure from server 's'
      s = e;
      sum[s][PRIORITY_CLASSES].service += serving_job[s]->size;
      sum[s][PRIORITY_CLASSES].served++;
      sum[s][serving_job[s]->priority].service += serving_job[s]->size;
      sum[s][serving_job[s]->priority].served++;
      processed_jobs[PRIORITY_CLASSES]++;
      processed_jobs[serving_job[s]->priority]++;
      node_jobs[PRIORITY_CLASSES]--;
      node_jobs[serving_job[s]->priority]--;
      free(serving_job[s]);
      if(node_jobs[PRIORITY_CLASSES] >= servers_num){
        serving_job[s] = ExtractJob(priority_queue);
        priority_queue_num[serving_job[s]->priority]--;
        priority_queue_num[PRIORITY_CLASSES]--;
        event[s].time = t.current + serving_job[s]->size;
      }
      else{
        event[s].status = 0;
      }
    }
  }

  double total_service[PRIORITY_CLASSES+1];
  double total_served[PRIORITY_CLASSES+1];
  for(int k=0; k<=PRIORITY_CLASSES; k++){
    total_service[k] = 0;
    total_served[k] = 0;
    for(s = 1; s <= servers_num; s++){           // adjust area to calculate
      total_service[k] += sum[s][k].service;
      total_served[k] += sum[s][k].served;
    }
  }
  
  for(int i=0; i<=PRIORITY_CLASSES; i++){
    result[i].jobs = processed_jobs[i];
    result[i].interarrival = (t.last_arrival[i] - START) / processed_jobs[i];
    result[i].wait = area[i].node / processed_jobs[i];
    result[i].delay = area[i].queue / processed_jobs[i];
    result[i].service = total_service[i] / total_served[i];
    result[i].Ns = area[i].node / t.current;
    result[i].Nq = area[i].queue / t.current;
    result[i].utilization = (total_service[i] / servers_num) / t.current;
  }

  printf("\nfor %ld jobs the service node statistics are:\n", result[PRIORITY_CLASSES].jobs);
  for(int i=0; i<PRIORITY_CLASSES; i++){
    printf("  class[%d]: %.3lf %%\n", i, (double)result[i].jobs * 100 / result[PRIORITY_CLASSES].jobs);
  }
  printf("    average interarrival time = %lf\n", result[PRIORITY_CLASSES].interarrival);
  for(int i=0; i<PRIORITY_CLASSES; i++){
    printf("      class[%d]:               = %lf\n", i, result[i].interarrival);
  }
  printf("    average wait              = %lf\n", result[PRIORITY_CLASSES].wait);
  for(int i=0; i<PRIORITY_CLASSES; i++){
    printf("      class[%d]:               = %lf\n", i, result[i].wait);
  }
  printf("    average delay             = %lf\n", result[PRIORITY_CLASSES].delay);
  for(int i=0; i<PRIORITY_CLASSES; i++){
    printf("      class[%d]:               = %lf\n", i, result[i].delay);
  }
  printf("    average service time      = %lf\n", result[PRIORITY_CLASSES].service);
  printf("    average # in the node     = %lf\n", result[PRIORITY_CLASSES].Ns);
  for(int i=0; i<PRIORITY_CLASSES; i++){
    printf("      class[%d]:               = %lf\n", i, result[i].Ns);
  }
  printf("    average # in the queue    = %lf\n", result[PRIORITY_CLASSES].Nq);
  for(int i=0; i<PRIORITY_CLASSES; i++){
    printf("      class[%d]:               = %lf\n", i, result[i].Nq);
  }
  printf("    utilization               = %lf\n", result[PRIORITY_CLASSES].utilization);
  for(int i=0; i<PRIORITY_CLASSES; i++){
    printf("      class[%d]:               = %lf\n", i, result[i].utilization);
  }

  printf("\nthe server statistics are:\n\n");
  printf("    server     utilization     avg service        share\n");
  for(s=1; s<=servers_num; s++){
    printf("%8d %16lf %15lf %14lf\n", s, sum[s][PRIORITY_CLASSES].service / t.current, sum[s][PRIORITY_CLASSES].service / sum[s][PRIORITY_CLASSES].served, (double) sum[s][PRIORITY_CLASSES].served / processed_jobs[PRIORITY_CLASSES]);
  }
  printf("\n");

  return 0;
}
