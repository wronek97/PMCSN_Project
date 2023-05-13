#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "rngs.h"
#include "rvgs.h"

#define START             0.0                 /* initial time                   */
#define STOP              20000.0             /* terminal (close the door) time */
#define PROCESSABLE_JOBS  1 << 20

double lambda = 2.0;
double mu = 3.0;
static int servers_num = 4;
int seed = 13;

double arrival = START;

typedef struct {
  double current;                           // current time
  double next;                              // next (most imminent) event time
  double last_arrival;  // last arrival time of a job of class 'k' (global last arrival if k=PRIORITY_CLASSES)
} time;

typedef struct time_integrated{
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

typedef struct job {
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

  job *prev = NULL;
  job *actual = *queue;
  while(actual != NULL && job_to_insert->size >= actual->size){
    prev = actual;
    actual = actual->next;
  }
  if(prev == NULL){
    *queue = job_to_insert;
  }else{
    prev->next = job_to_insert;
  }
  job_to_insert->next = actual;
  
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

int main(void){
  analysis result;
  time t;
  next_event event[servers_num + 1];
  int e;                                      // next event index
  int s;                                      // server index
  time_integrated area;   // time integrated variables
  long processed_jobs;    // departed jobs of priority 'k' (all priorities if k=PRIORITY_CLASSES)
  long node_jobs;         // jobs of priority 'k'  in the node (all priorities if k=PRIORITY_CLASSES)
  int queue_num;
  accumulated_sums sum[servers_num + 1];
  double service;
  job *queue = NULL;
  job *new_job = NULL, *serving_job[servers_num + 1];

  //init enviornment
  area.node = 0.0;
  area.queue = 0.0;
  node_jobs = 0;
  processed_jobs = 0;

  PlantSeeds(seed);
  t.current = START;
  event[0].time = GetArrival(lambda);
  event[0].status = 1;
  for(s = 1; s <= servers_num; s++){
    event[s].time = START;            /* this value is arbitrary because */
    event[s].status = 0;              /* all servers are initially idle  */
    sum[s].service = 0.0;
    sum[s].served = 0;
    serving_job[s] = NULL;
  }

  while((event[0].status != 0 || node_jobs > 0) && processed_jobs < PROCESSABLE_JOBS) {
    e = NextEvent(event);                                                   // next event index
    t.next = event[e].time;                                          // next event time
    area.node += (t.next - t.current) * node_jobs;           // update integral
    area.queue += (t.next - t.current) * queue_num;
    t.current = t.next;                                                     // advance the clock

    if(e == 0){ //process an arrival
      new_job = GenerateJob(GetService(), 0);
      InsertJob(&queue, new_job);
      queue_num++;
      node_jobs++;
      t.last_arrival = t.current;

      event[0].time = GetArrival(lambda);
      if(event[0].time > STOP){
        event[0].status = 0;
      }
      if(node_jobs <= servers_num){
        s = FindServer(event);
        serving_job[s] = ExtractJob(&queue);
        queue_num--;
        sum[s].service += serving_job[s]->size;
        sum[s].served++;
        event[s].time = t.current + serving_job[s]->size;
        event[s].status = 1;
      }
    }
    else{ //process a departure from server 's'
      s = e;
      processed_jobs++;
      node_jobs--;
      free(serving_job[s]);
      if(node_jobs >= servers_num){
        serving_job[s] = ExtractJob(&queue);
        queue_num--;
        sum[s].service += serving_job[s]->size;
        sum[s].served++;
        event[s].time = t.current + serving_job[s]->size;
      }
      else{
        event[s].status = 0;
      }
    }
  }

  double total_service = 0;
  double total_served = 0;
  for(s = 1; s <= servers_num; s++){           // adjust area to calculate
    total_service += sum[s].service;
    total_served += sum[s].served;
  }
  
  result.jobs = processed_jobs;
  result.interarrival = t.last_arrival / processed_jobs;
  result.wait = area.node / processed_jobs;
  result.delay = area.queue / processed_jobs;
  result.service = total_service / total_served;
  result.Ns = area.node / t.current;
  result.Nq = area.queue / t.current;
  result.utilization = (total_service / servers_num) / t.current;

  printf("\nfor %ld jobs the service node statistics are:\n", result.jobs);
  printf("   average interarrival time = %6.4lf\n", result.interarrival);
  printf("   average wait ............ = %6.4lf\n", result.wait);
  printf("   average delay ........... = %6.4lf\n", result.delay);
  printf("   average service time .... = %6.4lf\n", result.service);
  printf("   average # in the node ... = %6.4lf\n", result.Ns);
  printf("   average # in the queue .. = %6.4lf\n", result.Nq);
  printf("   utilization ............. = %6.4lf\n", result.utilization);

  printf("the server statistics are:\n\n");
  printf("    server     utilization     avg service        share\n");
  for(s=1; s<=servers_num; s++){
    printf("%8d %16lf %15lf %14lf\n", s, sum[s].service / t.current, sum[s].service / sum[s].served, (double) sum[s].served / processed_jobs);
  }
  printf("\n");

  return 0;
}
