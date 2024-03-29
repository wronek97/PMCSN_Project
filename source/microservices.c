/*
  problems to solve:
  1) ploss too high                                             (= 43.14 %)
  2) average response time of complete reservations too high    (= 56.23 s)

  QoS achived
  1) ploss on payment_control node                      <  5 %  (=  4.15 %)
  2) average response time of complete reservations     < 12 s  (= 11.84 s)

  QoS achived (alternative solution)
  1) ploss on payment_control node                     <  0 %  (=  0.00 %)
  2) average response time for priority class '1'      < 12 s  (= 12.00 s for priority class '1')
*/

#include "config.h"
#include "lib/utils.h"

double lambda[3][NODES] = {{1.9, 0.8, 0.0, 0.0}, {1.9, 0.8, 0.0, 0.0}, {1.9, 0.8, 0.0, 0.0}};
double mu[3][NODES] = {{1.0/2, 1.0/3.2, 1.0/2.5, 1.0/1.3}, {1.0/2, 1.0/3.2, 1.0/2.5, 1.0/1.3}, {1.0/2, 1.0/3.2, 1.0/2.5, 1.0/1.3}};
int servers_num[3][NODES] = {{4, 4, 2, 2}, {5, 6, 3, 4}, {5, 6, 3, 4}};
unsigned long queue_len[3][NODES] = { {INFINITE_CAPACITY, INFINITE_CAPACITY, INFINITE_CAPACITY, 8}, 
                                      {INFINITE_CAPACITY, INFINITE_CAPACITY, INFINITE_CAPACITY, 8},
                                      {INFINITE_CAPACITY, INFINITE_CAPACITY, INFINITE_CAPACITY, INFINITE_CAPACITY}
                                    };
double p[3] = {0.65, 0.2, 0.4};
double priority_probs[PRIORITY_CLASSES] = {0.8569, 0.1431};

int seed = 17;
unsigned long external_arrivals;
unsigned long max_processable_jobs = INFINITE_PROCESSABLE_JOBS;
double first_batch_arrival[NODES] = {START, START, START, START};
double priority_batch_arrival[PRIORITY_CLASSES] = {START, START};
double current_time = START;
node_stats *priority_classes;
time_integrated *priority_areas;

int mode;
double stop_time;
long iter_num;
int batch_size;
project_topology topology;

double GetInterArrival(node_id);
double GetService(node_id);
void process_arrival(event**, double, node_stats*, int, int);
void process_arrival_priority(event**, double, node_stats*, int, int);
void process_departure(event**, double, node_stats*, int, int);
void process_departure_priority(event**, double, node_stats*, int, int);
void execute_replica(event**, node_stats*, time_integrated*);
void execute_replica_priority(event**, node_stats*, time_integrated*); 
void execute_batch(event**, node_stats*, time_integrated*, int, int);
void execute_batch_priority(event**, node_stats*, time_integrated*, int, int);
void init_event_list(event**);
void init_servers(server_stats**, int);
void init_nodes(node_stats**);
void init_priority_nodes(node_stats**, node_id);
void init_areas(time_integrated**);
void init_priority_areas(time_integrated**);
void init_result(analysis***);
void init_priority_result(analysis***);


int main(int argc, char *argv[])
{
  event *ev = NULL, *event_list = NULL;
  node_stats *nodes;
  time_integrated *areas;
  node_id actual_node;
  int actual_server, current_batch = 0;
  analysis **result, **priority_result;
  statistic_analysis statistic_result, priority_statistic_result;

  fflush(stdout);
  if(argc != 3){
    printf("Usage: ./simulation <BASE/RESIZED/IMPROVED> <FINITE|INFINITE>\n");
    exit(0);
  }
  if(strcmp(argv[1], "BASE") == 0){
    topology = base;
  }
  else if(strcmp(argv[1], "RESIZED") == 0){
    topology = resized;
  }
  else if(strcmp(argv[1], "IMPROVED") == 0){
    topology = improved;
  }
  else{
    printf("Specify the topology: BASE or RESIZED or IMPROVED\n");
    exit(0);
  }
  if(strcmp(argv[2], "FINITE") == 0){
    mode = finite_horizon;
    stop_time = FINITE_HORIZON_STOP;
    iter_num = REPLICAS_NUM;
  }
  else if(strcmp(argv[2], "INFINITE") == 0){
    mode = infinite_horizon;
    stop_time = INFINITE_HORIZON_STOP;
    iter_num = BATCH_NUM;
    batch_size = BATCH_SIZE;
  }
  else{
    printf("Specify the simulation mode: FINITE or INFINITE\n");
    exit(0);
  }
  
  PlantSeeds(seed);

  printf("Simulation in progress, please wait\n");
  loading_bar(0.0);

  switch(mode){
    case finite_horizon:
      if(topology == improved){
        init_result(&result);
        init_priority_result(&priority_result);
        for(int rep=0; rep<iter_num; rep++){
          external_arrivals = 0;
          init_event_list(&event_list);
          init_nodes(&nodes);
          init_priority_nodes(&priority_classes, payment_control);
          init_areas(&areas);
          init_priority_areas(&priority_areas);

          // execute a single simulation run
          execute_replica_priority(&event_list, nodes, areas);
          
          // extract analysis data from the single replica
          extract_analysis(result[rep], nodes, areas, servers_num[topology], current_time, NULL);
          extract_priority_analysis(priority_result[rep], priority_classes, priority_areas, servers_num[topology][payment_control], current_time, NULL);
          
          // free memory
          free(areas);
          free(nodes);

          // update loading bar
          loading_bar((double)(rep+1)/iter_num);
        }

        // extract statistic analysis data from the entire simulation
        extract_statistic_analysis(result, &statistic_result, mode);
        extract_priority_statistic_analysis(result, priority_result, &priority_statistic_result, mode);

        // print output and save analysis to csv
        print_improved_statistic_result(&statistic_result, &priority_statistic_result, priority_probs, mode);
        save_improved_to_csv(&statistic_result, &priority_statistic_result, topology, seed, mode);
      }
      else{
        init_result(&result);
        for(int rep=0; rep<iter_num; rep++){
          external_arrivals = 0;
          init_event_list(&event_list);
          init_nodes(&nodes);
          init_areas(&areas);

          // execute a single simulation run
          execute_replica(&event_list, nodes, areas);
                    
          // extract analysis data from the single replica
          extract_analysis(result[rep], nodes, areas, servers_num[topology], current_time, NULL);
          
          // free memory
          free(areas);
          free(nodes);

          // update loading bar
          loading_bar((double)(rep+1)/iter_num);
        }

        // extract statistic analysis data from the entire simulation
        extract_statistic_analysis(result, &statistic_result, mode);

        // print output and save analysis to csv
        print_statistic_result(&statistic_result, mode);
        save_to_csv(&statistic_result, topology, seed, mode);
      }
      
      break;


    case infinite_horizon:
      if(topology == improved){
        init_result(&result);
        init_priority_result(&priority_result);
        init_event_list(&event_list);
        init_nodes(&nodes);
        init_priority_nodes(&priority_classes, payment_control);
        init_areas(&areas);
        init_priority_areas(&priority_areas);

        int current_batch = 0;
        external_arrivals = 0;

        // execute and extract statistic result from every single batch
        double batch_period = (BATCH_SIZE / (lambda[topology][0] + lambda[topology][1]));
        for(int k=0; k<iter_num; k++){
          execute_batch_priority(&event_list, nodes, areas, batch_size, k);
          extract_analysis(result[k], nodes, areas, servers_num[topology], batch_period, first_batch_arrival);
          extract_priority_analysis(priority_result[k], priority_classes, priority_areas, servers_num[topology][payment_control], batch_period, first_batch_arrival);
          reset_stats(nodes, areas, first_batch_arrival);
          reset_priority_stats(priority_classes, priority_areas);
          loading_bar((double)(k+1)/iter_num);
        }

        // extract statistic analysis data from the entire simulation
        extract_statistic_analysis(result, &statistic_result, mode);
        extract_priority_statistic_analysis(result, priority_result, &priority_statistic_result, mode);

        // print output and save analysis to csv
        print_improved_statistic_result(&statistic_result, &priority_statistic_result, priority_probs, mode);
        save_improved_to_csv(&statistic_result, &priority_statistic_result, topology, seed, mode);
      }
      else{
        init_result(&result);
        init_event_list(&event_list);
        init_nodes(&nodes);
        init_areas(&areas);
        int current_batch = 0;
        external_arrivals = 0;

        // execute and extract statistic result from every single batch
        for (int k=0; k<iter_num; k++) {
          execute_batch(&event_list, nodes, areas, batch_size, k);
          extract_analysis(result[k], nodes, areas, servers_num[topology], (BATCH_SIZE / (lambda[topology][0] + lambda[topology][1])), first_batch_arrival);
          loading_bar((double)(k+1)/iter_num);
          reset_stats(nodes, areas, first_batch_arrival);
        }

        // extract statistic analysis data from the entire simulation
        extract_statistic_analysis(result, &statistic_result, mode);

        // print output and save analysis to csv
        print_statistic_result(&statistic_result, mode);
        save_to_csv(&statistic_result, topology, seed, mode);
      }
      
      break;


    default:
      break;
  }

  return 0;
}

double GetInterArrival(node_id k){
  SelectStream(20*k);
  return Exponential(1.0/lambda[topology][k]);
}
   
double GetService(node_id k){                 
  SelectStream(20*(NODES+k));
  return Exponential(1.0/(mu[topology][k]));    
}

void process_arrival(event **list, double current_time, node_stats *nodes, int actual_node, int actual_server) {
  job *job = NULL;
  event *new_dep, *new_arr;
  
  if(nodes[actual_node].node_jobs < nodes[actual_node].total_servers + queue_len[topology][actual_node]){ // there is available space in queue
    job = GenerateJob(current_time, GetService(actual_node), 0);
    if(nodes[actual_node].node_jobs < nodes[actual_node].total_servers){
      int selected_server = SelectServer(nodes[actual_node]); // find available server
      nodes[actual_node].servers[selected_server].status = busy;
      nodes[actual_node].servers[selected_server].serving_job = job;
      new_dep = GenerateEvent(job_departure, actual_node, selected_server, current_time + job->service);
      InsertEvent(list, new_dep);
    }
    else { // insert job in queue
      InsertJob(&(nodes[actual_node].queue), job);
      nodes[actual_node].queue_jobs++;
    }
    nodes[actual_node].node_jobs++;
    nodes[actual_node].last_arrival = current_time;
  }
  else { // reject the job
    nodes[actual_node].rejected_jobs++;
  }

  if(actual_server == outside){ // generate next arrival event and schedule on condition
    new_arr = GenerateEvent(job_arrival, actual_node, outside, current_time + GetInterArrival(actual_node));
    if(new_arr->time < stop_time && external_arrivals < max_processable_jobs){
      InsertEvent(list, new_arr);
      external_arrivals++;
    }
  }
}

void process_arrival_priority(event **list, double current_time, node_stats *nodes, int actual_node, int actual_server) {
  job *new_job = NULL;
  event *new_dep, *new_arr;

  if(nodes[actual_node].node_jobs < nodes[actual_node].total_servers + queue_len[topology][actual_node]){ // there is availble space in queue
    new_job = GenerateJob(current_time, GetService(actual_node), SelectPriorityClass(PRIORITY_CLASSES, priority_probs));
    if(nodes[actual_node].node_jobs < nodes[actual_node].total_servers){
      int selected_server = SelectServer(nodes[actual_node]); // find available server
      nodes[actual_node].servers[selected_server].status = busy;
      nodes[actual_node].servers[selected_server].serving_job = new_job;
      new_dep = GenerateEvent(job_departure, actual_node, selected_server, current_time + new_job->service);
      InsertEvent(list, new_dep);
    }
    else { // insert job in queue
      if(actual_node == payment_control){
        InsertPriorityJob(&(nodes[payment_control].queue), new_job);
        priority_classes[new_job->priority].queue_jobs++;
      }
      else{
        InsertJob(&(nodes[actual_node].queue), new_job);
      }
      nodes[actual_node].queue_jobs++;
    }
    nodes[actual_node].node_jobs++;
    nodes[actual_node].last_arrival = current_time;
    if(actual_node == payment_control){
      priority_classes[new_job->priority].node_jobs++;
      priority_classes[new_job->priority].last_arrival = current_time;
    }
  }
  else { // reject the job
    nodes[actual_node].rejected_jobs++;
  }

  if(actual_server == outside){
    new_arr = GenerateEvent(job_arrival, actual_node, outside, current_time + GetInterArrival(actual_node)); // generate next arrival event
    if(new_arr->time < stop_time && external_arrivals < max_processable_jobs){ // schedule event only on condition
      InsertEvent(list, new_arr);
      external_arrivals++;
    }
  }
}

void process_departure(event **list, double current_time, node_stats *nodes, int actual_node, int actual_server) {
  job *job = NULL;
  event *new_dep, *new_arr;
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
    InsertEvent(list, new_dep);
  }
  else{
    nodes[actual_node].servers[actual_server].status = idle;
  }
  
  new_arr = GenerateEvent(job_arrival, SwitchNode(p, actual_node), actual_server, current_time);
  InsertEvent(list, new_arr);
}

void process_departure_priority(event **list, double current_time, node_stats *nodes, int actual_node, int actual_server) {
  job *new_job = NULL;
  event *new_dep, *new_arr;
  job *serving_job = nodes[actual_node].servers[actual_server].serving_job;

  nodes[actual_node].servers[actual_server].service_time += serving_job->service;
  nodes[actual_node].servers[actual_server].served_jobs++;
  nodes[actual_node].servers[actual_server].last_departure_time = current_time;

  nodes[actual_node].processed_jobs++;
  nodes[actual_node].node_jobs--;

  if(actual_node == payment_control){
    priority_classes[serving_job->priority].servers[actual_server].service_time += serving_job->service;
    priority_classes[serving_job->priority].servers[actual_server].served_jobs++;
    priority_classes[serving_job->priority].servers[actual_server].last_departure_time = current_time;
    
    priority_classes[serving_job->priority].processed_jobs++;
    priority_classes[serving_job->priority].node_jobs--;
  }

  free(nodes[actual_node].servers[actual_server].serving_job);

  if(nodes[actual_node].queue_jobs > 0){
    new_job = ExtractJob(&(nodes[actual_node].queue));
    nodes[actual_node].servers[actual_server].serving_job = new_job;
    nodes[actual_node].queue_jobs--;
    if(actual_node == payment_control){
      priority_classes[new_job->priority].queue_jobs--;
    }

    new_dep = GenerateEvent(job_departure, actual_node, actual_server, current_time + new_job->service);
    InsertEvent(list, new_dep);
  }
  else{
    nodes[actual_node].servers[actual_server].status = idle;
  }
  
  new_arr = GenerateEvent(job_arrival, SwitchNode(p, actual_node), actual_server, current_time);
  InsertEvent(list, new_arr);
}

void execute_replica(event **list, node_stats *nodes, time_integrated *areas) {
  event *ev;
  node_id actual_node;
  int actual_server;
  double next_time;
  while(*list != NULL){
    // extract next event
    ev = ExtractEvent(list);
    actual_node = ev->node;
    actual_server = ev->server;
    next_time = ev->time;

    // update integrals for every node
    for(int node=0; node<NODES; node++){ 
      areas[node].node_area += (next_time - current_time) * nodes[node].node_jobs;
      areas[node].queue_area += (next_time - current_time) * nodes[node].queue_jobs;
    }
    current_time = next_time;

    // process an arrival on a free server or in queue
    if(ev->type == job_arrival) process_arrival(list, current_time, nodes, actual_node, actual_server);
    
    // process a departure from the specific busy server 
    else process_departure(list, current_time, nodes, actual_node, actual_server);
    
    free(ev);
  }
}

void execute_replica_priority(event **list, node_stats *nodes, time_integrated *areas) {
  event *ev;
  node_id actual_node;
  int actual_server;
  double next_time;

  while(*list != NULL){
    // extract next event
    ev = ExtractEvent(list);
    actual_node = ev->node;
    actual_server = ev->server;
    next_time = ev->time;

    // update integrals for every node
    for(int node=0; node<NODES; node++){ 
      areas[node].node_area += (next_time - current_time) * nodes[node].node_jobs;
      areas[node].queue_area += (next_time - current_time) * nodes[node].queue_jobs;
    }
    if(topology == improved){
      for(int class=0; class<PRIORITY_CLASSES; class++){ 
        priority_areas[class].node_area += (next_time - current_time) * priority_classes[class].node_jobs;
        priority_areas[class].queue_area += (next_time - current_time) * priority_classes[class].queue_jobs;
      }
    }
    current_time = next_time;

    // process an arrival on a free server or in queue
    if(ev->type == job_arrival){
      process_arrival_priority(list, current_time, nodes, actual_node, actual_server);
    }
    
    // process a departure from the specific busy server 
    else{
      process_departure_priority(list, current_time, nodes, actual_node, actual_server);
    }
    
    free(ev);
  }
}

void execute_batch(event **list, node_stats *nodes, time_integrated *areas, int b, int k){
  event *ev;
  node_id actual_node;
  int actual_server;
  double next_time;
  // spawn new event until we achieve b jobs in the batch
  while (external_arrivals < (b * (k + 1))){
    // extract next event
    ev = ExtractEvent(list);
    actual_node = ev->node;
    actual_server = ev->server;
    next_time = ev->time;

    // update integrals for every node
    for(int node=0; node<NODES; node++){ 
      areas[node].node_area += (next_time - current_time) * nodes[node].node_jobs;
      areas[node].queue_area += (next_time - current_time) * nodes[node].queue_jobs;
    }
    current_time = next_time;

    // process an arrival on a free server or in queue
    if(ev->type == job_arrival) process_arrival(list, current_time, nodes, actual_node, actual_server);
    
    // process a departure from the specific busy server
    else process_departure(list, current_time, nodes, actual_node, actual_server);
    
    free(ev);
  }
}

void execute_batch_priority(event **list, node_stats *nodes, time_integrated *areas, int b, int k){
  event *ev;
  node_id actual_node;
  int actual_server;
  double next_time;
  // spawn new event until we achieve b jobs in the batch
  while (external_arrivals < (b * (k + 1))){
    // extract next event
    ev = ExtractEvent(list);
    actual_node = ev->node;
    actual_server = ev->server;
    next_time = ev->time;

    // update integrals for every node
    for(int node=0; node<NODES; node++){ 
      areas[node].node_area += (next_time - current_time) * nodes[node].node_jobs;
      areas[node].queue_area += (next_time - current_time) * nodes[node].queue_jobs;
    }
    for(int class=0; class<PRIORITY_CLASSES; class++){ 
      priority_areas[class].node_area += (next_time - current_time) * priority_classes[class].node_jobs;
      priority_areas[class].queue_area += (next_time - current_time) * priority_classes[class].queue_jobs;
    }
    current_time = next_time;

    // process an arrival on a free server or in queue
    if(ev->type == job_arrival){
      process_arrival_priority(list, current_time, nodes, actual_node, actual_server);
    }
    
    // process a departure from the specific busy server
    else{
      process_departure_priority(list, current_time, nodes, actual_node, actual_server);
    }
    
    free(ev);
  }
}

void init_event_list(event **list){
  event *new_arrival;

  for(int node=0; node<NODES; node++){
    if(lambda[node] != 0){
      new_arrival = GenerateEvent(job_arrival, node, outside, START + GetInterArrival(node));
      if(new_arrival->time < stop_time && external_arrivals < max_processable_jobs){
        InsertEvent(list, new_arrival);
        external_arrivals++;
      }
    }
  }
}

void init_servers(server_stats **servers, int servers_n){
  *servers = calloc(servers_n, sizeof(server_stats));
  if(*servers == NULL){
    printf("Error allocating memory for: server_stats\n");
    exit(1);
  }
}

void init_nodes(node_stats **nodes){
  *nodes = calloc(NODES, sizeof(node_stats));
  if(*nodes == NULL){
    printf("Error allocating memory for: nodes_stats\n");
    exit(2);
  }
  for(int i=0; i<NODES; i++){
    (*nodes)[i].total_servers = servers_num[topology][i];
    init_servers(&((*nodes)[i].servers), servers_num[topology][i]);
  }
}

void init_priority_nodes(node_stats **nodes, node_id node){
  *nodes = calloc(PRIORITY_CLASSES, sizeof(node_stats));
  if(*nodes == NULL){
    printf("Error allocating memory for: nodes_stats\n");
    exit(2);
  }
  for(int i=0; i<PRIORITY_CLASSES; i++){
    (*nodes)[i].total_servers = servers_num[improved][node];
    init_servers(&((*nodes)[i].servers), servers_num[improved][node]);
  }
}

void init_areas(time_integrated **areas){
  *areas = calloc(NODES, sizeof(time_integrated));
  if(*areas == NULL){
    printf("Error allocating memory for: time_integrated\n");
    exit(3);
  }
}

void init_priority_areas(time_integrated **areas){
  *areas = calloc(PRIORITY_CLASSES, sizeof(time_integrated));
  if(*areas == NULL){
    printf("Error allocating memory for: time_integrated\n");
    exit(3);
  }
}

void init_result(analysis ***result){
  *result = calloc(iter_num, sizeof(analysis*));
  if(*result == NULL){
    printf("Error allocating memory for: analysis\n");
    exit(4);
  }
  for(int rep=0; rep<iter_num; rep++){
    (*result)[rep] = calloc(NODES, sizeof(analysis));
    if(*result == NULL){
      printf("Error allocating memory for: analysis\n");
      exit(4);
    }
    for(int i=0; i<NODES; i++){
      (*result)[rep][i].server_utilization = calloc(servers_num[topology][i], sizeof(double));
      (*result)[rep][i].server_service = calloc(servers_num[topology][i], sizeof(double));
      (*result)[rep][i].server_share = calloc(servers_num[topology][i], sizeof(double));
      if((*result)[rep][i].server_utilization == NULL || (*result)[rep][i].server_service == NULL || (*result)[rep][i].server_share == NULL){
        printf("Error allocating memory: analysis server statistics\n");
        exit(4);
      }
    }
  }
}

void init_priority_result(analysis ***result){
  *result = calloc(iter_num, sizeof(analysis*));
  if(*result == NULL){
    printf("Error allocating memory for: analysis\n");
    exit(4);
  }
  for(int rep=0; rep<iter_num; rep++){
    (*result)[rep] = calloc(PRIORITY_CLASSES, sizeof(analysis));
    if(*result == NULL){
      printf("Error allocating memory for: analysis\n");
      exit(4);
    }
    for(int i=0; i<PRIORITY_CLASSES; i++){
      (*result)[rep][i].server_utilization = calloc(servers_num[improved][payment_control], sizeof(double));
      (*result)[rep][i].server_service = calloc(servers_num[improved][payment_control], sizeof(double));
      (*result)[rep][i].server_share = calloc(servers_num[improved][payment_control], sizeof(double));
      if((*result)[rep][i].server_utilization == NULL || (*result)[rep][i].server_service == NULL || (*result)[rep][i].server_share == NULL){
        printf("Error allocating memory: analysis server statistics\n");
        exit(4);
      }
    }
  }
}