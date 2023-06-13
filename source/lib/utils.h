#include "utils.c"

node_id SwitchNode(double*, node_id);
int SelectPriorityClass(int, double*);
int SelectServer(node_stats);
event* GenerateEvent(event_type, node_id, int, double);
void InsertEvent(event**, event*);
event* ExtractEvent(event**);
job* GenerateJob(double, double, int);
void InsertJob(job**, job*);
void InsertJob_priority(job**, job*);
job* ExtractJob(job**);

void extract_analysis(analysis*, node_stats*, time_integrated*, int*, double, double*);
void extract_priority_analysis(analysis*, node_stats*, time_integrated*, int, double, double*);

void extract_statistic_analysis(analysis**, statistic_analysis*, int);
void extract_priority_statistic_analysis(analysis**, analysis**, statistic_analysis*, int);

void print_replica(analysis*, int*);
void print_statistic_result(statistic_analysis*, int);
void print_priority_statistic_result(statistic_analysis*, statistic_analysis*, double*, int);

void save_to_csv(statistic_analysis*, project_phase, int, int);
void save_priority_to_csv(statistic_analysis*, statistic_analysis*, project_phase, int, int);

void reset_stats(node_stats*, time_integrated*, double*);
void loading_bar(double);