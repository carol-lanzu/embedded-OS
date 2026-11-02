#ifndef OS_CONFIG_H
#define	OS_CONFIG_H

#define FCY                 4000000UL

#define ON                  1
#define OFF                 0

#define MAX_TASKS           5
#define MAX_STACK_SIZE      128
#define SIZE_MESSAGE_QUEUE  4
#define HEAP_SIZE           1024

#define RR_SCHEDULER        1
#define PRIOR_SCHEDULER     2

#define DEFAULT_SCHEDULER   RR_SCHEDULER

#define DYNAMIC_ALLOCATION  ON


#endif	/* OS_CONFIG_H */

