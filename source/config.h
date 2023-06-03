#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "lib/rngs.h"
#include "lib/rvgs.h"
#include "lib/rvms.h"

// SIMULATION CONFIG VALUES
#define FINITE_HORIZON_SIMULATION       0           // finite horizon simulation mode
#define REPLICAS_NUM                    400         // number of runs
#define INFINITE_HORIZON_SIMULATION     1           // infinite horizon simulation mode
#define BATCH_NUM                       200         // number of batches

#define START                           0.0         // initial (open the door) time
#define FINITE_HORIZON_STOP             86400.0     // terminal (close the door) time (1 day for transient analysis)
#define INFINITE_HORIZON_STOP           43200000.0  // terminal (close the door) time (500 days for steady state analysis)

#define NODES                           4           // number of nodes in the system
#define INFINITE_CAPACITY               1 << 27     // large number to simulate infinite queue
#define INFINITE_PROCESSABLE_JOBS       1 << 27     // large number to simulate infinite
#define LOC                             0.99        // level of confidence, use 0.99 for 99% confidence



// DATA STRUCTURES
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
} statistic_analysis;