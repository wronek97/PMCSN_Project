#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "rngs.h"
#include "rvgs.h"

#define START         0.0              /* initial time                   */
#define STOP      200000.0              /* terminal (close the door) time */
#define INFINITY   (100.0 * STOP)      /* must be much larger than STOP  */
#define PROCESSABLE_JOBS  999999

double Min(double a, double c)
{ 
  if (a < c)
    return (a);
  else
    return (c);
} 

double GetArrival()
{
  static double arrival = START;

  SelectStream(0); 
  arrival += Exponential(1.0/2);
  return arrival;
} 

double GetService()
{
  SelectStream(1);
  return Exponential(1.0/3);
}  

int main(void)
{
  struct {
    double arrival;                 /* next arrival time                   */
    double completion;              /* next completion time                */
    double current;                 /* current time                        */
    double next;                    /* next (most imminent) event time     */
    double last;                    /* last arrival time                   */
  } t;
  struct {
    double node;                    /* time integrated number in the node  */
    double queue;                   /* time integrated number in the queue */
    double service;                 /* time integrated number in service   */
  } area      = {0.0, 0.0, 0.0};
  long index  = 0;                  /* used to count departed jobs         */
  long number = 0;                  /* jobs in the node                  */
  long rejected = 0;
  long queue_len = 2;

  PlantSeeds(13);
  t.current    = START;           /* set the clock                         */
  t.arrival    = GetArrival();    /* schedule the first arrival            */
  t.completion = INFINITY;        /* the first event can't be a completion */

  while((t.arrival < STOP || number > 0) && index < PROCESSABLE_JOBS){
    t.next = Min(t.arrival, t.completion);            /* next event time   */
    if (number > 0) {                                 /* update integrals  */
      area.node    += (t.next - t.current) * number;
      area.queue   += (t.next - t.current) * (number - 1);
      area.service += (t.next - t.current);
    }
    t.current = t.next;                    /* advance the clock */

    if(t.current == t.arrival){               /* process an arrival */
      if(number <= queue_len){
        number++;
        t.last = t.current;
        if(number == 1){
          t.completion = t.current + GetService();
        }
      }
      else{
        rejected++;
      }

      t.arrival = GetArrival();
      if(t.arrival > STOP){
        t.arrival   = INFINITY;
      }
    }
    else{                                        /* process a completion */
      index++;
      number--;
      if(number > 0){
        t.completion = t.current + GetService();
      }
      else{
        t.completion = INFINITY;
      }
    }
  } 

  printf("\nfor %ld jobs processed on %ld arrived\n", index, index + rejected);
  printf("   average interarrival time = %.4lf\n", t.last / index);
  printf("   average wait ............ = %.4lf\n", area.node / index);
  printf("   average delay ........... = %.4lf\n", area.queue / index);
  printf("   average service time .... = %.4lf\n", area.service / index);
  printf("   average # in the node ... = %.4lf\n", area.node / t.current);
  printf("   average # in the queue .. = %.4lf\n", area.queue / t.current);
  printf("   utilization ............. = %.4lf\n", area.service / t.current);
  printf("   ploss ................... = %.3lf%%\n", (double) rejected * 100 / (rejected + index));

  return (0);
}
