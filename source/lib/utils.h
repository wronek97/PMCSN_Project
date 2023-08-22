#include "utils.c"

node_id SwitchNode(double*, node_id);
int SelectPriorityClass(int, double*);
int SelectServer(node_stats);
event* GenerateEvent(event_type, node_id, int, double);
void InsertEvent(event**, event*);
event* ExtractEvent(event**);
job* GenerateJob(double, double, int);
void InsertJob(job**, job*);
void InsertPriorityJob(job**, job*);
job* ExtractJob(job**);

void extract_analysis(analysis*, node_stats*, time_integrated*, int*, double, double*);
void extract_priority_analysis(analysis*, node_stats*, time_integrated*, int, double, double*);

void extract_statistic_analysis(analysis**, statistic_analysis*, int);
void extract_priority_statistic_analysis(analysis**, analysis**, statistic_analysis*, int);

void print_replica(analysis*, int*);
void print_statistic_result(statistic_analysis*, int);
void print_improved_statistic_result(statistic_analysis*, statistic_analysis*, double*, int);

void save_to_csv(statistic_analysis*, project_topology, int, int);
void save_improved_to_csv(statistic_analysis*, statistic_analysis*, project_topology, int, int);

void reset_stats(node_stats*, time_integrated*, double*);
void reset_priority_stats(node_stats*, time_integrated*);
void loading_bar(double);