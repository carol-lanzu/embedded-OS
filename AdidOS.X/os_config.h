#ifndef OS_CONFIG_H
#define OS_CONFIG_H

#define FCY                 4000000UL

#define ON                  1
#define OFF                 0

#define MAX_TASKS           5
#define MAX_STACK_SIZE      128
#define SIZE_MESSAGE_QUEUE  4
#define HEAP_SIZE           1024

#define RR_SCHEDULER        1
#define PRIOR_SCHEDULER     2

/*
 * ============================================================================
 * Convencao de Prioridades do AdidOS:
 * Numeros MAIORES representam MAIOR prioridade de execucao.
 * - PRIORITY_IDLE (0)  : Menor prioridade do sistema, reservada a tarefa idle.
 * - PRIORITY_LOW  (1)  : Baixa prioridade de usuario.
 * - PRIORITY_MED  (5)  : Prioridade media padrao de usuario.
 * - PRIORITY_HIGH (10) : Alta prioridade de usuario.
 * - PRIORITY_MAX  (255): Limite maximo suportado (uint8_t).
 * ============================================================================
 */
#define PRIORITY_IDLE       0
#define PRIORITY_LOW        1
#define PRIORITY_MED        5
#define PRIORITY_HIGH       10
#define PRIORITY_MAX        255

#define DEFAULT_SCHEDULER   PRIOR_SCHEDULER

#define DYNAMIC_ALLOCATION  ON


#endif /* OS_CONFIG_H */
