/**
* Find the next node where to send a job
**/
node_id SwitchNode(double *prob, node_id start_node){
  node_id destination_node;
  double rand;

  SelectStream(192);
  rand = Random();

  switch(start_node){
    case flight:
      if(rand < prob[0]) destination_node = payment_control;
      else if(rand > 1 - prob[1]) destination_node = hotel;
      else destination_node = taxi;
      break;

    case hotel:
      if(rand < prob[2]) destination_node = taxi;           
      else destination_node = payment_control;
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

/**
* Find the priority queue responsible for handling a job
**/
int SelectPriorityClass(int classes_num, double *probs)
{
  double tot_prob = 0;
  double aux = 0;
  for(int i=0; i<classes_num; i++){
    if(probs[i]<0 || probs[i]>1){
      printf("Parameter 'p' of class %d must be between 0 and 1\n", i);
      exit(0);
    }
    tot_prob += probs[i];
  }
  if(tot_prob != 1){
    printf("Sum of parameters 'p' must be equal to 1\n");
    exit(0);
  }

  SelectStream(140);

  double ext = Random();
  for(int i=0; i<classes_num; i++){
    aux += probs[i];
    if(ext <= aux) return i;
  }
  return classes_num-1;
}

/**
* Find the first available (idle) server to serve a job
**/ 
int SelectServer(node_stats node){ 
  int i=0, s;
  while(node.servers[i].status == busy) i++;
  
  if(i >= node.total_servers){
    printf("Error: You are trying to serve a job when all servers are busy!!\n\n");
    exit(0);
  }

  s = i;
  for(i=s+1; i<node.total_servers; i++){
    if(node.servers[i].status == idle && node.servers[i].last_departure_time < node.servers[s].last_departure_time) s = i;
  }

  return s;
}

/**
* Generate a new event
**/
event* GenerateEvent(event_type type, node_id node, int server, double time){
  event* new_event;

  if(node >= NODES) return NULL;

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

/**
* Insert an event in the correct position in the event list
**/
void InsertEvent(event **list, event *new_event){
  if(new_event == NULL) return;

  if(*list == NULL) *list = new_event;
  else{
    event *prev = NULL, *tmp = *list;
    while(tmp->next != NULL && tmp->time < new_event->time){
      prev = tmp;
      tmp = tmp->next;
    }
    if(prev != NULL){
      if(tmp->time < new_event->time) tmp->next = new_event;
      else{
        prev->next = new_event;
        new_event->next = tmp;
      }
    }
    else{
      if(tmp->time < new_event->time) tmp->next = new_event;
      else{
        new_event->next = *list;
        *list = new_event;
      }
    }
  }
}

/**
* Extract next event from the event list
**/
event* ExtractEvent(event **list){
  event* next_event = NULL;

  if(*list != NULL){
    next_event = *list;
    *list = (*list)->next;
  }

  return next_event;
}

/**
* Generate a new job
**/
job* GenerateJob(double arrival, double service, int priority){
  job* new_job = malloc(sizeof(job));
  if(new_job == NULL){
    printf("Error allocating memory for a job\n");
    exit(1);
  }
  new_job->arrival = arrival;
  new_job->service = service;
  new_job->priority = priority;
  new_job->next = NULL;

  return new_job;
}

/**
* Insert a job in a node's queue
**/
void InsertJob(job** queue, job* job_to_insert){
  if(*queue == NULL) *queue = job_to_insert;
  else{
    job *aux = *queue;
    while(aux->next != NULL) aux = aux->next;
    aux->next = job_to_insert;
  }
}

/**
* Insert a job in a node's priority queue
**/
void InsertPriorityJob(job** queue, job* job_to_insert){
  if(*queue == NULL){
    *queue = job_to_insert;
    return;
  }

  job *aux = *queue;
  if(job_to_insert->priority < aux->priority){
    job_to_insert->next = aux;
    *queue = job_to_insert;
    return;
  }

  while(aux->next != NULL && job_to_insert->priority >= aux->next->priority){
    aux = aux->next;
  }
  job_to_insert->next = aux->next;
  aux->next = job_to_insert;
}

/**
* Extract next job from the specific queue
**/
job* ExtractJob(job **queue)
{
  job* served_job = NULL;

  if(*queue != NULL){
    served_job = *queue;
    *queue = (*queue)->next;
  }

  return served_job;
}

/**
* Reset integrals to clean values for next batch in infinite horizon simulation
**/
void reset_stats(node_stats *nodes, time_integrated *areas, double *first_batch_arrival){
  for(int i=0; i<NODES; i++){
    first_batch_arrival[i] = nodes[i].last_arrival;
    nodes[i].processed_jobs = 0;
    nodes[i].rejected_jobs = 0;
    for(int s=0; s<nodes[i].total_servers; s++){
      nodes[i].servers[s].service_time = 0;
      nodes[i].servers[s].served_jobs = 0;
    }
    areas[i].node_area = 0;
    areas[i].queue_area = 0;
  }
}

/**
* Extract intermediate results from a single run (finite horizon) or a single batch (infinite horizon) of the base/resized simulation
**/
void extract_analysis(analysis *result, node_stats *nodes, time_integrated *areas, int *servers_num, double oper_period, double *first_batch_arrival){
  double total_service[NODES];

  for(int k=0; k<NODES; k++){
    total_service[k] = 0;
    for(int s=0; s<servers_num[k]; s++) total_service[k] += nodes[k].servers[s].service_time;
  }

  for(int i=0; i<NODES; i++){
    result[i].jobs = nodes[i].processed_jobs;

    if(first_batch_arrival == NULL) result[i].interarrival = (nodes[i].last_arrival - START) / nodes[i].processed_jobs;
    else result[i].interarrival = (nodes[i].last_arrival - first_batch_arrival[i]) / nodes[i].processed_jobs;

    result[i].wait = areas[i].node_area / nodes[i].processed_jobs;

    result[i].delay = areas[i].queue_area / nodes[i].processed_jobs;

    result[i].service = total_service[i] / nodes[i].processed_jobs;

    result[i].Ns = areas[i].node_area / oper_period;

    result[i].Nq = areas[i].queue_area / oper_period;

    result[i].utilization = (total_service[i] / servers_num[i]) / oper_period;

    if(nodes[i].rejected_jobs == 0 && nodes[i].processed_jobs == 0) result[i].ploss = 0;
    else result[i].ploss = (double) nodes[i].rejected_jobs / (nodes[i].rejected_jobs + nodes[i].processed_jobs);

    for(int s=0; s<servers_num[i]; s++){
      result[i].server_utilization[s] = nodes[i].servers[s].service_time / oper_period;
      result[i].server_service[s] = nodes[i].servers[s].service_time / nodes[i].servers[s].served_jobs;
      result[i].server_share[s] = (double) nodes[i].servers[s].served_jobs * 100 / nodes[i].processed_jobs;
    }
  }
}

/**
* Extract intermediate results of a single run (finite horizon) or a single batch (infinite horizon) of the improved simulation
**/
void extract_priority_analysis(analysis *result, node_stats *nodes, time_integrated *areas, int servers_num, double oper_period, double *first_batch_arrival){
  double total_service[PRIORITY_CLASSES];

  for(int k=0; k<PRIORITY_CLASSES; k++){
    total_service[k] = 0;
    for(int s=0; s<servers_num; s++) total_service[k] += nodes[k].servers[s].service_time;
  }

  for(int i=0; i<PRIORITY_CLASSES; i++){
    result[i].jobs = nodes[i].processed_jobs;

    if(first_batch_arrival == NULL) result[i].interarrival = (nodes[i].last_arrival - START) / nodes[i].processed_jobs;
    else result[i].interarrival = (nodes[i].last_arrival - first_batch_arrival[i]) / nodes[i].processed_jobs;

    result[i].wait = areas[i].node_area / nodes[i].processed_jobs;

    result[i].delay = areas[i].queue_area / nodes[i].processed_jobs;

    result[i].service = total_service[i] / nodes[i].processed_jobs;

    result[i].Ns = areas[i].node_area / oper_period;

    result[i].Nq = areas[i].queue_area / oper_period;

    result[i].utilization = (total_service[i] / servers_num) / oper_period;

    if(nodes[i].rejected_jobs == 0 && nodes[i].processed_jobs == 0) result[i].ploss = 0;
    else result[i].ploss = (double) nodes[i].rejected_jobs / (nodes[i].rejected_jobs + nodes[i].processed_jobs);

    for(int s=0; s<servers_num; s++){
      result[i].server_utilization[s] = nodes[i].servers[s].service_time / oper_period;
      result[i].server_service[s] = nodes[i].servers[s].service_time / nodes[i].servers[s].served_jobs;
      result[i].server_share[s] = (double) nodes[i].servers[s].served_jobs * 100 / nodes[i].processed_jobs;
    }
  }
}

/**
* Extract final statistic result from the base/resized simulation
**/
void extract_statistic_analysis(analysis **result, statistic_analysis *statistic_result, int mode){
  long iter_num;
  if (mode == finite_horizon) iter_num = REPLICAS_NUM;
  else iter_num = BATCH_NUM;
  double u = 1.0 - (1.0 - LOC)/2;                     // interval parameter
  double t = idfStudent(iter_num - 1, u);             // critical value of t
  double diff;
  struct {
    double interarrival;
    double wait;
    double delay;
    double service;
    double Ns;
    double Nq;
    double utilization;
    double ploss;
    double max_wait;
  } sum;
  double *max_wait = calloc(iter_num, sizeof(double));
  
  if(max_wait == NULL){
    printf("Error allocating memory: max_wait calloc\n");
    exit(5);
  }
  
  // use Welford's one-pass method and standard deviation  
  for(int i=0; i<NODES; i++){ 
    statistic_result->interarrival[i][mean] = 0;
    sum.interarrival = 0;
    statistic_result->wait[i][mean] = 0;
    sum.wait = 0;
    statistic_result->delay[i][mean] = 0;
    sum.delay = 0;
    statistic_result->service[i][mean] = 0;
    sum.service = 0;
    statistic_result->Ns[i][mean] = 0;
    sum.Ns = 0;
    statistic_result->Nq[i][mean] = 0;
    sum.Nq = 0;
    statistic_result->utilization[i][mean] = 0;
    sum.utilization = 0;
    statistic_result->ploss[i][mean] = 0;
    sum.ploss = 0;

    for(int n=1; n<=iter_num; n++){
      diff = result[n-1][i].interarrival - statistic_result->interarrival[i][mean];
      sum.interarrival += diff * diff * (n - 1.0) / n;
      statistic_result->interarrival[i][mean] += diff / n;

      diff = result[n-1][i].wait - statistic_result->wait[i][mean];
      sum.wait += diff * diff * (n - 1.0) / n;
      statistic_result->wait[i][mean] += diff / n;
      
      diff = result[n-1][i].delay - statistic_result->delay[i][mean];
      sum.delay += diff * diff * (n - 1.0) / n;
      statistic_result->delay[i][mean] += diff / n;

      diff = result[n-1][i].service - statistic_result->service[i][mean];
      sum.service += diff * diff * (n - 1.0) / n;
      statistic_result->service[i][mean] += diff / n;

      diff = result[n-1][i].Ns - statistic_result->Ns[i][mean];
      sum.Ns += diff * diff * (n - 1.0) / n;
      statistic_result->Ns[i][mean] += diff / n;

      diff = result[n-1][i].Nq - statistic_result->Nq[i][mean];
      sum.Nq += diff * diff * (n - 1.0) / n;
      statistic_result->Nq[i][mean] += diff / n;

      diff = result[n-1][i].utilization - statistic_result->utilization[i][mean];
      sum.utilization += diff * diff * (n - 1.0) / n;
      statistic_result->utilization[i][mean] += diff / n;

      diff = result[n-1][i].ploss - statistic_result->ploss[i][mean];
      sum.ploss += diff * diff * (n - 1.0) / n;
      statistic_result->ploss[i][mean] += diff / n;
    }

    if(iter_num > 1){
      statistic_result->interarrival[i][interval] = t * sqrt(sum.interarrival / iter_num) / sqrt(iter_num - 1);
      statistic_result->wait[i][interval] = t * sqrt(sum.wait / iter_num) / sqrt(iter_num - 1);
      statistic_result->delay[i][interval] = t * sqrt(sum.delay / iter_num) / sqrt(iter_num - 1);
      statistic_result->service[i][interval] = t * sqrt(sum.service / iter_num) / sqrt(iter_num - 1);
      statistic_result->Ns[i][interval] = t * sqrt(sum.Ns / iter_num) / sqrt(iter_num - 1);
      statistic_result->Nq[i][interval] = t * sqrt(sum.Nq / iter_num) / sqrt(iter_num - 1);
      statistic_result->utilization[i][interval] = t * sqrt(sum.utilization / iter_num) / sqrt(iter_num - 1);
      statistic_result->ploss[i][interval] = t * sqrt(sum.ploss / iter_num) / sqrt(iter_num - 1);
    }
    else{
      printf("ERROR - insufficient data\n");
      exit(5);
    }
  }

  statistic_result->avg_max_wait[mean] = 0;
  sum.max_wait = 0;
  for(int n=1; n<=iter_num; n++){
    max_wait[n-1] = 0;
    for(int i=0; i<NODES; i++){
      max_wait[n-1] += result[n-1][i].wait;
    }
    diff = max_wait[n-1] - statistic_result->avg_max_wait[mean];
    sum.max_wait += diff * diff * (n - 1.0) / n;
    statistic_result->avg_max_wait[mean] += diff / n;
  }
  if(iter_num > 1){
      statistic_result->avg_max_wait[interval] = t * sqrt(sum.max_wait / iter_num) / sqrt(iter_num - 1);
  }
  else{
    printf("ERROR - insufficient data\n");
    exit(5);
  }
}

/**
* Extract final statistic result from the improved simulation
**/
void extract_priority_statistic_analysis(analysis **result, analysis **priority_result, statistic_analysis *statistic_result, int mode){
  long iter_num;
  if (mode == finite_horizon) iter_num = REPLICAS_NUM;
  else iter_num = BATCH_NUM;
  double u = 1.0 - (1.0 - LOC)/2;                     // interval parameter
  double t = idfStudent(iter_num - 1, u);             // critical value of t
  double diff;
  struct {
    double interarrival;
    double wait;
    double delay;
    double service;
    double Ns;
    double Nq;
    double utilization;
    double ploss;
    double max_wait;
    double priority_max_wait[PRIORITY_CLASSES];
  } sum;
  double *max_wait = calloc(iter_num, sizeof(double));
  double *priority_max_wait[PRIORITY_CLASSES];
  for(int i=0; i<PRIORITY_CLASSES; i++){
    priority_max_wait[i] = calloc(iter_num, sizeof(double));
    if(priority_max_wait[i] == NULL){
      printf("Error allocating memory: priority_max_wait calloc\n");
      exit(5);
    }
  }
  if(max_wait == NULL){
    printf("Error allocating memory: max_wait calloc\n");
    exit(5);
  }
  
  // use Welford's one-pass method and standard deviation  
  for(int i=0; i<PRIORITY_CLASSES; i++){ 
    statistic_result->interarrival[i][mean] = 0;
    sum.interarrival = 0;
    statistic_result->wait[i][mean] = 0;
    sum.wait = 0;
    statistic_result->delay[i][mean] = 0;
    sum.delay = 0;
    statistic_result->service[i][mean] = 0;
    sum.service = 0;
    statistic_result->Ns[i][mean] = 0;
    sum.Ns = 0;
    statistic_result->Nq[i][mean] = 0;
    sum.Nq = 0;
    statistic_result->utilization[i][mean] = 0;
    sum.utilization = 0;
    statistic_result->ploss[i][mean] = 0;
    sum.ploss = 0;

    for(int n=1; n<=iter_num; n++){
      diff = priority_result[n-1][i].interarrival - statistic_result->interarrival[i][mean];
      sum.interarrival += diff * diff * (n - 1.0) / n;
      statistic_result->interarrival[i][mean] += diff / n;

      diff = priority_result[n-1][i].wait - statistic_result->wait[i][mean];
      sum.wait += diff * diff * (n - 1.0) / n;
      statistic_result->wait[i][mean] += diff / n;
      
      diff = priority_result[n-1][i].delay - statistic_result->delay[i][mean];
      sum.delay += diff * diff * (n - 1.0) / n;
      statistic_result->delay[i][mean] += diff / n;

      diff = priority_result[n-1][i].service - statistic_result->service[i][mean];
      sum.service += diff * diff * (n - 1.0) / n;
      statistic_result->service[i][mean] += diff / n;

      diff = priority_result[n-1][i].Ns - statistic_result->Ns[i][mean];
      sum.Ns += diff * diff * (n - 1.0) / n;
      statistic_result->Ns[i][mean] += diff / n;

      diff = priority_result[n-1][i].Nq - statistic_result->Nq[i][mean];
      sum.Nq += diff * diff * (n - 1.0) / n;
      statistic_result->Nq[i][mean] += diff / n;

      diff = priority_result[n-1][i].utilization - statistic_result->utilization[i][mean];
      sum.utilization += diff * diff * (n - 1.0) / n;
      statistic_result->utilization[i][mean] += diff / n;

      diff = priority_result[n-1][i].ploss - statistic_result->ploss[i][mean];
      sum.ploss += diff * diff * (n - 1.0) / n;
      statistic_result->ploss[i][mean] += diff / n;
    }

    if(iter_num > 1){
      statistic_result->interarrival[i][interval] = t * sqrt(sum.interarrival / iter_num) / sqrt(iter_num - 1);
      statistic_result->wait[i][interval] = t * sqrt(sum.wait / iter_num) / sqrt(iter_num - 1);
      statistic_result->delay[i][interval] = t * sqrt(sum.delay / iter_num) / sqrt(iter_num - 1);
      statistic_result->service[i][interval] = t * sqrt(sum.service / iter_num) / sqrt(iter_num - 1);
      statistic_result->Ns[i][interval] = t * sqrt(sum.Ns / iter_num) / sqrt(iter_num - 1);
      statistic_result->Nq[i][interval] = t * sqrt(sum.Nq / iter_num) / sqrt(iter_num - 1);
      statistic_result->utilization[i][interval] = t * sqrt(sum.utilization / iter_num) / sqrt(iter_num - 1);
      statistic_result->ploss[i][interval] = t * sqrt(sum.ploss / iter_num) / sqrt(iter_num - 1);
    }
    else{
      printf("ERROR - insufficient data\n");
      exit(5);
    }
  }

  for(int i=0; i<PRIORITY_CLASSES; i++){
    statistic_result->priority_avg_max_wait[i][mean] = 0;
    sum.priority_max_wait[i] = 0;
  }
  
  for(int n=1; n<=iter_num; n++){
    for(int i=0; i<PRIORITY_CLASSES; i++){
      priority_max_wait[i][n-1] = 0;
      for(int node=0; node<NODES-1; node++){
        priority_max_wait[i][n-1] += result[n-1][node].wait;
      }
      priority_max_wait[i][n-1] += priority_result[n-1][i].wait;

      diff = priority_max_wait[i][n-1] - statistic_result->priority_avg_max_wait[i][mean];
      sum.priority_max_wait[i] += diff * diff * (n - 1.0) / n;
      statistic_result->priority_avg_max_wait[i][mean] += diff / n;
    }
  }
  if(iter_num > 1){
    for(int i=0; i<PRIORITY_CLASSES; i++){
      statistic_result->priority_avg_max_wait[i][interval] = t * sqrt(sum.priority_max_wait[i] / iter_num) / sqrt(iter_num - 1);
    }
  }
  else{
    printf("ERROR - insufficient data\n");
    exit(5);
  }
}

/**
* Print all nodes and servers values
**/
void print_replica(analysis *result, int *servers_num){
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
    for(int s=0; s<servers_num[k]; s++) printf("%6d %15lf %15lf %15.4lf%%\n", s+1, result[k].server_utilization[s], result[k].server_service[s], result[k].server_share[s]);
    printf("\n");
  }
  printf("\n");
}

/**
* Print statistic result of the base/resized simulation
**/
void print_statistic_result(statistic_analysis *result, int mode){
  if(mode == finite_horizon) printf("Based upon %d simulations and with %.2lf%% confidence:\n\n", REPLICAS_NUM, 100.0 * LOC);
  else if(mode == infinite_horizon) printf("Based on a simulation split into %d batches and with %.2lf%% confidence:\n\n", BATCH_NUM, 100.0 * LOC);

  for(int k=0; k<NODES; k++){
    printf("Node %d:\n", k+1);
    printf("    avg interarrival     = %10.6lf +/- %9.6lf\n", result->interarrival[k][mean], result->interarrival[k][interval]);
    printf("    avg wait             = %10.6lf +/- %9.6lf\n", result->wait[k][mean], result->wait[k][interval]);
    printf("    avg delay            = %10.6lf +/- %9.6lf\n", result->delay[k][mean], result->delay[k][interval]);
    printf("    avg service          = %10.6lf +/- %9.6lf\n", result->service[k][mean], result->service[k][interval]);
    printf("    avg # in node        = %10.6lf +/- %9.6lf\n", result->Ns[k][mean], result->Ns[k][interval]);
    printf("    avg # in queue       = %10.6lf +/- %9.6lf\n", result->Nq[k][mean], result->Nq[k][interval]);
    printf("    avg utilizzation     = %10.6lf +/- %9.6lf\n", result->utilization[k][mean], result->utilization[k][interval]);
    printf("    ploss                = %8.4lf %% +/- %7.4lf %%\n", 100 * result->ploss[k][mean], 100 * result->ploss[k][interval]);
    printf("\n");
  }
  printf("Average max response time = %7.3lf s +/- %6.3f s\n", result->avg_max_wait[mean], result->avg_max_wait[interval]);
}

/**
* Print statistic result of the improved simulation
**/
void print_improved_statistic_result(statistic_analysis *result, statistic_analysis *priority_result, double *priority_perc, int mode){
  int k;
  
  if(mode == finite_horizon) printf("Based on %d simulations and with %.2lf%% confidence:\n\n", REPLICAS_NUM, 100.0 * LOC);
  else if(mode == infinite_horizon) printf("Based on a simulation splitted into %d batches and with %.2lf%% confidence:\n\n", BATCH_NUM, 100.0 * LOC);
  else exit(0);

  for(k=0; k<NODES-1; k++){
    printf("Node %d:\n", k+1);
    printf("    avg interarrival     = %10.6lf +/- %9.6lf\n", result->interarrival[k][mean], result->interarrival[k][interval]);
    printf("    avg wait             = %10.6lf +/- %9.6lf\n", result->wait[k][mean], result->wait[k][interval]);
    printf("    avg delay            = %10.6lf +/- %9.6lf\n", result->delay[k][mean], result->delay[k][interval]);
    printf("    avg service          = %10.6lf +/- %9.6lf\n", result->service[k][mean], result->service[k][interval]);
    printf("    avg # in node        = %10.6lf +/- %9.6lf\n", result->Ns[k][mean], result->Ns[k][interval]);
    printf("    avg # in queue       = %10.6lf +/- %9.6lf\n", result->Nq[k][mean], result->Nq[k][interval]);
    printf("    avg utilizzation     = %10.6lf +/- %9.6lf\n", result->utilization[k][mean], result->utilization[k][interval]);
    printf("    ploss                = %8.4lf %% +/- %7.4lf %%\n", 100 * result->ploss[k][mean], 100 * result->ploss[k][interval]);
    printf("\n");
  }
  printf("Node %d:\n", k+1);
  for(int i=0; i<PRIORITY_CLASSES; i++){
    printf(" class[%d] job percentage = %6.2lf %%\n", i+1, 100 * priority_perc[i]);
  }
  printf("\n");
  printf("    avg interarrival     = %10.6lf +/- %9.6lf\n", result->interarrival[k][mean], result->interarrival[k][interval]);
  for(int i=0; i<PRIORITY_CLASSES; i++){
    printf("        class[%d]         = %10.6lf +/- %9.6lf\n", i+1, priority_result->interarrival[i][mean], priority_result->interarrival[i][interval]);
  }
  printf("    avg wait             = %10.6lf +/- %9.6lf\n", result->wait[k][mean], result->wait[k][interval]);
  for(int i=0; i<PRIORITY_CLASSES; i++){
    printf("        class[%d]         = %10.6lf +/- %9.6lf\n", i+1, priority_result->wait[i][mean], priority_result->wait[i][interval]);
  }
  printf("    avg delay            = %10.6lf +/- %9.6lf\n", result->delay[k][mean], result->delay[k][interval]);
  for(int i=0; i<PRIORITY_CLASSES; i++){
    printf("        class[%d]         = %10.6lf +/- %9.6lf\n", i+1, priority_result->delay[i][mean], priority_result->delay[i][interval]);
  }
  printf("    avg service          = %10.6lf +/- %9.6lf\n", result->service[k][mean], result->service[k][interval]);
  for(int i=0; i<PRIORITY_CLASSES; i++){
    printf("        class[%d]         = %10.6lf +/- %9.6lf\n", i+1, priority_result->service[i][mean], priority_result->service[i][interval]);
  }
  printf("    avg # in node        = %10.6lf +/- %9.6lf\n", result->Ns[k][mean], result->Ns[k][interval]);
  for(int i=0; i<PRIORITY_CLASSES; i++){
    printf("        class[%d]         = %10.6lf +/- %9.6lf\n", i+1, priority_result->Ns[i][mean], priority_result->Ns[i][interval]);
  }
  printf("    avg # in queue       = %10.6lf +/- %9.6lf\n", result->Nq[k][mean], result->Nq[k][interval]);
  for(int i=0; i<PRIORITY_CLASSES; i++){
    printf("        class[%d]         = %10.6lf +/- %9.6lf\n", i+1, priority_result->Nq[i][mean], priority_result->Nq[i][interval]);
  }
  printf("    avg utilizzation     = %10.6lf +/- %9.6lf\n", result->utilization[k][mean], result->utilization[k][interval]);
  for(int i=0; i<PRIORITY_CLASSES; i++){
    printf("        class[%d]         = %10.6lf +/- %9.6lf\n", i+1, priority_result->utilization[i][mean], priority_result->utilization[i][interval]);
  }
  printf("    ploss                = %8.4lf %% +/- %7.4lf %%\n", 100 * result->ploss[k][mean], 100 * result->ploss[k][interval]);
  printf("\n");
  
  printf("Average max response time = %7.3lf s +/- %6.3lf s\n", result->avg_max_wait[mean], result->avg_max_wait[interval]);
  for(int i=0; i<PRIORITY_CLASSES; i++){
    printf("        class[%d]          = %7.3lf s +/- %6.3lf s\n", i+1, priority_result->priority_avg_max_wait[i][mean], priority_result->priority_avg_max_wait[i][interval]);
  }
}

/**
* Save statistic result of the base/resized simulation
**/
void save_to_csv(statistic_analysis *result, project_phase phase, int seed, int mode){
  char title[128];
  char filename[128];

  if(mode == finite_horizon){
    snprintf(title, 55, "Based on %d simulations and with %.2lf%% confidence;\n\n", REPLICAS_NUM, 100.0 * LOC);
    switch(phase){
      case base:
        snprintf(filename, 44, "analysis//transient//base_transient_%03d.csv", seed);
        break;
      case resized:
        snprintf(filename, 47, "analysis//transient//resized_transient_%03d.csv", seed);
        break;
      case improved:
        snprintf(filename, 48, "analysis//transient//improved_transient_%03d.csv", seed);
        break;
      default:
        snprintf(filename, 39, "analysis//transient//transient_%03d.csv", seed);
        break;
    }
  }
  else if(mode == infinite_horizon){
    snprintf(title, 78, "Based on a simulation splitted into %d batches and with %.2lf%% confidence;\n\n", BATCH_NUM, 100.0 * LOC);
    switch(phase){
      case base:
        snprintf(filename, 50, "analysis//steady_state//base_steady_state_%03d.csv", seed);
        break;
      case resized:
        snprintf(filename, 53, "analysis//steady_state//resized_steady_state_%03d.csv", seed);
        break;
      case improved:
        snprintf(filename, 54, "analysis//steady_state//improved_steady_state_%03d.csv", seed);
        break;
      default:
        snprintf(filename, 45, "analysis//steady_state//steady_state_%03d.csv", seed);
        break;
    }
  }
  else exit(0);

  FILE *csv = fopen(filename, "w");
  fputs(title, csv);

  for(int k=0; k<NODES; k++){
    fprintf(csv,"NODE %d;mean;;interval;\n", k+1);
    fprintf(csv,"avg interarrival;%lf;+/-;%lf;\n", result->interarrival[k][mean], result->interarrival[k][interval]);
    fprintf(csv,"avg wait;%lf;+/-;%lf;\n", result->wait[k][mean], result->wait[k][interval]);
    fprintf(csv,"avg delay;%lf;+/-;%lf;\n", result->delay[k][mean], result->delay[k][interval]);
    fprintf(csv,"avg service;%lf;+/-;%lf;\n", result->service[k][mean], result->service[k][interval]);
    fprintf(csv,"avg # in node;%lf;+/-;%lf;\n", result->Ns[k][mean], result->Ns[k][interval]);
    fprintf(csv,"avg # in queue;%lf;+/-;%lf;\n", result->Nq[k][mean], result->Nq[k][interval]);
    fprintf(csv,"avg utilizzation;%lf;+/-;%lf;\n", result->utilization[k][mean], result->utilization[k][interval]);
    fprintf(csv,"ploss;%.4lf%%;+/-;%.4lf%%;\n", 100 * result->ploss[k][mean], 100 * result->ploss[k][interval]);
    fprintf(csv,"\n");
  }
  fprintf(csv,"Average max response time:; %.4lf;+/-;%.4lf;\n", result->avg_max_wait[mean], result->avg_max_wait[interval]);
  fclose(csv);
}

/**
* Save statistic result of the improved simulation
**/
void save_improved_to_csv(statistic_analysis *result, statistic_analysis *priority_result, project_phase phase, int seed, int mode){
  char title[128];
  char filename[128];
  int k;
  if(mode == finite_horizon && phase == improved) {
    snprintf(title, 55, "Based on %d simulations and with %.2lf%% confidence;\n\n", REPLICAS_NUM, 100.0 * LOC);
    snprintf(filename, 48, "analysis//transient//improved_transient_%03d.csv", seed);
  }
  else if(mode == infinite_horizon && phase == improved) {
    snprintf(title, 78, "Based on a simulation splitted into %d batches and with %.2lf%% confidence;\n\n", BATCH_NUM, 100.0 * LOC);
    snprintf(filename, 54, "analysis//steady_state//improved_steady_state_%03d.csv", seed);
  }
  else exit(0);

  FILE *csv = fopen(filename, "w");
  fputs(title, csv);

  for(k=0; k<NODES-1; k++){
    fprintf(csv, "NODE %d;mean;;interval;\n", k+1);
    fprintf(csv, "avg interarrival;%lf;+/-;%lf;\n", result->interarrival[k][mean], result->interarrival[k][interval]);
    fprintf(csv, "avg wait;%lf;+/-;%lf;\n", result->wait[k][mean], result->wait[k][interval]);
    fprintf(csv, "avg delay;%lf;+/-;%lf;\n", result->delay[k][mean], result->delay[k][interval]);
    fprintf(csv, "avg service;%lf;+/-;%lf;\n", result->service[k][mean], result->service[k][interval]);
    fprintf(csv, "avg # in node;%lf;+/-;%lf;\n", result->Ns[k][mean], result->Ns[k][interval]);
    fprintf(csv, "avg # in queue;%lf;+/-;%lf;\n", result->Nq[k][mean], result->Nq[k][interval]);
    fprintf(csv, "avg utilizzation;%lf;+/-;%lf;\n", result->utilization[k][mean], result->utilization[k][interval]);
    fprintf(csv, "ploss;%.4lf%%;+/-;%.4lf%%;\n", 100 * result->ploss[k][mean], 100 * result->ploss[k][interval]);
    fprintf(csv, "\n");
  }
  fprintf(csv, "NODE %d;mean;;interval\n", k+1);
  fprintf(csv, "avg interarrival;%lf;+/-;%lf;\n", result->interarrival[k][mean], result->interarrival[k][interval]);
  for(int i=0; i<PRIORITY_CLASSES; i++){
    fprintf(csv, "class[%d];%lf;+/-;%lf;\n", i+1, priority_result->interarrival[i][mean], priority_result->interarrival[i][interval]);
  }
  fprintf(csv, "avg wait;%lf;+/-;%lf;\n", result->wait[k][mean], result->wait[k][interval]);
  for(int i=0; i<PRIORITY_CLASSES; i++){
    fprintf(csv, "class[%d];%lf;+/-;%lf;\n", i+1, priority_result->wait[i][mean], priority_result->wait[i][interval]);
  }
  fprintf(csv, "avg delay;%lf;+/-;%lf;\n", result->delay[k][mean], result->delay[k][interval]);
  for(int i=0; i<PRIORITY_CLASSES; i++){
    fprintf(csv, "class[%d];%lf;+/-;%lf;\n", i+1, priority_result->delay[i][mean], priority_result->delay[i][interval]);
  }
  fprintf(csv, "avg service;%lf;+/-;%lf;\n", result->service[k][mean], result->service[k][interval]);
  for(int i=0; i<PRIORITY_CLASSES; i++){
    fprintf(csv, "class[%d];%lf;+/-;%lf;\n", i+1, priority_result->service[i][mean], priority_result->service[i][interval]);
  }
  fprintf(csv, "avg # in node;%lf;+/-;%lf;\n", result->Ns[k][mean], result->Ns[k][interval]);
  for(int i=0; i<PRIORITY_CLASSES; i++){
    fprintf(csv, "class[%d];%lf;+/-;%lf;\n", i+1, priority_result->Ns[i][mean], priority_result->Ns[i][interval]);
  }
  fprintf(csv, "avg # in queue;%lf;+/-;%lf;\n", result->Nq[k][mean], result->Nq[k][interval]);
  for(int i=0; i<PRIORITY_CLASSES; i++){
    fprintf(csv, "class[%d];%lf;+/-;%lf;\n", i+1, priority_result->Nq[i][mean], priority_result->Nq[i][interval]);
  }
  fprintf(csv, "avg utilizzation;%lf;+/-;%lf;\n", result->utilization[k][mean], result->utilization[k][interval]);
  for(int i=0; i<PRIORITY_CLASSES; i++){
    fprintf(csv, "class[%d];%lf;+/-;%lf;\n", i+1, priority_result->utilization[i][mean], priority_result->utilization[i][interval]);
  }
  fprintf(csv, "ploss;%.4lf%%;+/-;%.4lf%%;\n", 100 * result->ploss[k][mean], 100 * result->ploss[k][interval]);
  fprintf(csv, "\n");

  fprintf(csv,"Average max response time:; %.4lf;+/-;%.4lf;\n", result->avg_max_wait[mean], result->avg_max_wait[interval]);
  for(int i=0; i<PRIORITY_CLASSES; i++){
    fprintf(csv, "class[%d]; %.4lf;+/-;%.4lf;\n", i+1, priority_result->priority_avg_max_wait[i][mean], priority_result->priority_avg_max_wait[i][interval]);
  }


  fclose(csv);
}

/**
* Build, update and complete the progress bar
**/
void loading_bar(double progress){
  int i=0, percentage = (int)(100*progress);

  if(progress == 1){
    printf("\r\33[2K\033[A\33[2K");
  }
  else{
    printf("\rProgress: %2d%% [", percentage);
    while(i<percentage){
      printf("#");
      i++;
    }
    while(i<100){
      printf("_");
      i++;
    }
    printf("]");
  }
  fflush(stdout);
}