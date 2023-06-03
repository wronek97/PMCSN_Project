/*
  QoS achived
  1) ploss on payment_control node <  5 %  (=  0.00 %)
  2) max average response time     < 12 s  (= 11.36 s)
*/

#include "config.h"
#include "lib/utils.h"

double lambda[NODES] = {1.9, 0.8, 0.0, 0.0};
double mu[NODES] = {1.0/2, 1.0/3.2, 1.0/2.5, 1.0/1.3};
int servers_num[NODES] = {5, 6, 3, 5};
unsigned long queue_len[NODES] = {INFINITE_CAPACITY, INFINITE_CAPACITY, INFINITE_CAPACITY, INFINITE_CAPACITY};
double p[3] = {0.65, 0.2, 0.4};

int seed = 17;
unsigned long external_arrivals;
unsigned long max_processable_jobs = INFINITE_PROCESSABLE_JOBS;
double first_batch_arrival[NODES] = {0, 0, 0, 0};

int mode;
double stop_time;
long iter_num;
project_phase phase = improved;

double GetInterArrival(node_id);
double GetService(node_id);
void process_arrival(event**, double, node_stats*, int, int);
void process_departure(event**, double, node_stats*, int, int);
void init_event_list(event**);
void init_servers(server_stats**, int);
void init_nodes(node_stats**);
void init_areas(time_integrated**);
void init_result(analysis***);


int main(int argc, char *argv[])
{
  event *ev = NULL, *event_list = NULL;
  node_stats *nodes;
  time_integrated *areas;
  node_id actual_node;
  int actual_server, current_batch = 0;
  double current_time = START, next_time;
  analysis **result;
  statistic_analysis statistic_result;

  fflush(stdout);
  if (argc != 2) {
    printf("Usage: ./simulate_improved <FINITE|INFINITE>\n");
    exit(0);
  }
  if(strcmp(argv[1], "FINITE") == 0){
    mode = finite_horizon;
    stop_time = FINITE_HORIZON_STOP;
    iter_num = REPLICAS_NUM;
  }
  else if(strcmp(argv[1], "INFINITE") == 0){
    mode = infinite_horizon;
    stop_time = INFINITE_HORIZON_STOP;
    iter_num = BATCH_NUM;
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
      init_result(&result);
      for(int rep=0; rep<iter_num; rep++){
        external_arrivals = 0;
        init_event_list(&event_list);
        init_nodes(&nodes);
        init_areas(&areas);

        while(event_list != NULL){
          // extract next event
          ev = ExtractEvent(&event_list);
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
          if(ev->type == job_arrival) process_arrival(&event_list, current_time, nodes, actual_node, actual_server);
          
          // process a departure from the specific busy server 
          else process_departure(&event_list, current_time, nodes, actual_node, actual_server);
          
          free(ev);
        }

        // extract analysis data from the single replica
        extract_analysis(result[rep], nodes, areas, servers_num, current_time, NULL);
        
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
      save_to_csv(&statistic_result, phase, mode, seed);
      break;


    case infinite_horizon:
      init_result(&result);
      init_event_list(&event_list);
      init_nodes(&nodes);
      init_areas(&areas);
      external_arrivals = 0;

      while(event_list != NULL){
        // extract next event
        ev = ExtractEvent(&event_list);
        actual_node = ev->node;
        actual_server = ev->server;
        next_time = ev->time;

        // extract analysis data from the single batch
        if(current_time >= (current_batch + 1) * (stop_time / iter_num) && current_batch != iter_num - 1){
          extract_analysis(result[current_batch], nodes, areas, servers_num, stop_time / iter_num, first_batch_arrival);
          loading_bar((double)(current_batch+1)/iter_num); // update loading bar
          reset_stats(nodes, areas, first_batch_arrival);
          current_batch++;
        }

        // update integrals for every node
        for(int node=0; node<NODES; node++){ 
          areas[node].node_area += (next_time - current_time) * nodes[node].node_jobs;
          areas[node].queue_area += (next_time - current_time) * nodes[node].queue_jobs;
        }
        current_time = next_time;

        // process an arrival on a free server or in queue
        if(ev->type == job_arrival) process_arrival(&event_list, current_time, nodes, actual_node, actual_server);
        
        // process a departure from the specific busy server
        else process_departure(&event_list, current_time, nodes, actual_node, actual_server);
        
        free(ev);
      }

      // extract analysis data from last batch and complete loading bar
      extract_analysis(result[iter_num - 1], nodes, areas, servers_num, current_time, first_batch_arrival);
      loading_bar(1.0);
      
      // extract statistic analysis data from the entire simulation
      extract_statistic_analysis(result, &statistic_result, mode);

      // print output and save analysis to csv
      print_statistic_result(&statistic_result, mode);
      save_to_csv(&statistic_result, phase, mode, seed);
      break;

    default:
      break;
  }

  return 0;
}

double GetInterArrival(node_id k){
  SelectStream(20*k);
  return Exponential(1.0/lambda[k]);
}
   
double GetService(node_id k){                 
  SelectStream(20*(NODES+k));
  return Exponential(1.0/(mu[k]));    
}

void process_arrival(event **list, double current_time, node_stats *nodes, int actual_node, int actual_server) {
  job *job = NULL;
  event *new_dep, *new_arr;
  if(nodes[actual_node].node_jobs < nodes[actual_node].total_servers + queue_len[actual_node]){ // there is availble space in queue
    job = GenerateJob(current_time, GetService(actual_node));
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

void init_event_list(event **list){
  event *new_arrival;

  for(int node=0; node<NODES; node++){
    if(lambda[node] != 0){
      new_arrival = GenerateEvent(job_arrival, node, outside, START + GetInterArrival(node));
      if(new_arrival->time < stop_time && external_arrivals < max_processable_jobs){ // schedule event only on condition
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
    exit(2);
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
      (*result)[rep][i].server_utilization = calloc(servers_num[i], sizeof(double));
      (*result)[rep][i].server_service = calloc(servers_num[i], sizeof(double));
      (*result)[rep][i].server_share = calloc(servers_num[i], sizeof(double));
      if((*result)[rep][i].server_utilization == NULL || (*result)[rep][i].server_service == NULL || (*result)[rep][i].server_share == NULL){
        printf("Error allocating memory: analysis server statistics\n");
        exit(4);
      }
    }
  }
}