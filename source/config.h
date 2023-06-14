#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "lib/rngs.h"
#include "lib/rvgs.h"
#include "lib/rvms.h"

// SIMULATION CONFIG VALUES
#define REPLICAS_NUM                    200         // number of runs
#define BATCH_NUM                       200         // number of batches
#define BATCH_SIZE                      500000      // number of jobs in a single batch
// 500000*200 = 110.000.000 jobs che sono prodotti in circa 41.000.000 s -> arrivi esterni 1.9 j/s + 0.8 j/s = 2.7 j/s

#define START                           0.0         // initial (open the door) time
#define FINITE_HORIZON_STOP             86400.0     // terminal (close the door) time (1 day for transient analysis)
#define INFINITE_HORIZON_STOP           43200000.0  // terminal (close the door) time (500 days for steady state analysis)

#define NODES                           4           // number of nodes in the system
#define PRIORITY_CLASSES                2           // number of priority queues of the last node in the improved scenario
#define INFINITE_CAPACITY               1 << 27     // large number to simulate infinite queue
#define INFINITE_PROCESSABLE_JOBS       1 << 27     // large number to simulate infinite
#define LOC                             0.99        // level of confidence, use 0.99 for 99% confidence


// DATA STRUCTURES
typedef enum {
  base,
  resized,
  improved
} project_phase;

typedef enum {
  finite_horizon,
  infinite_horizon
} simulation_mode;

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
  int priority;
  struct job *next;
} job;

typedef enum {
  idle,
  busy
} status;

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
  job *queue;             // list of jobs in the queue of the node
  int total_servers;      // number of servers in the node
  server_stats *servers;  // server status and stats
} node_stats;

typedef struct {
  double node_area;         // time integrated jobs in the node
  double queue_area;        // time integrated jobs in the queue
} time_integrated;

enum {
  mean,
  interval
};

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

typedef struct {    // [0] = mean, [1] = confidence interval
  double interarrival[NODES][2];
  double wait[NODES][2];
  double delay[NODES][2];
  double service[NODES][2];
  double Ns[NODES][2];
  double Nq[NODES][2];
  double utilization[NODES][2];
  double ploss[NODES][2];
  double avg_max_wait[2];
  double priority_avg_max_wait[PRIORITY_CLASSES][2];    // used for priority queues
} statistic_analysis;