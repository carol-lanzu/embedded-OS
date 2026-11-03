#include <builtins.h>

#include "scheduler.h"
#include "os_config.h"
#include "types.h"
#include "kernel.h"

extern queue_t ReadyQueue;
extern uint8_t task_running;
extern uint16_t *task_running_tos;


uint8_t scheduler()
{
#if DEFAULT_SCHEDULER == RR_SCHEDULER
    return rr_scheduler();
#else
    return prior_scheduler();
#endif    
}

uint8_t rr_scheduler()
{
    uint8_t next_task = task_running, idle_count = 0;
    
    do {
        //next_task = (next_task+1) % ReadyQueue.queue_size;
        next_task = __builtin_modsd(next_task+1, ReadyQueue.queue_size);
        
        if (ReadyQueue.tasks[next_task].task_func == idle) {
            idle_count++;
            if (idle_count >= 2) {
                return 0;
            }
        }
        
    } while (ReadyQueue.tasks[next_task].task_state != READY ||
             ReadyQueue.tasks[next_task].task_func == idle);
    
    return next_task;
}

/*
 * prior_scheduler:
 * Escalonador preemptivo por prioridade com alternancia Round-Robin em empates.
 * 
 * Regras:
 * 1. Seleciona a tarefa em estado READY com a maior prioridade (numero maior).
 * 2. Se houver mais de uma tarefa READY com a mesma prioridade maxima,
 *    aplica Round-Robin a partir de (task_running + 1) para alternar entre elas,
 *    garantindo que nenhuma monopolize a CPU.
 * 3. Se nenhuma tarefa de usuario estiver pronta, seleciona a tarefa idle (indice 0).
 */
uint8_t prior_scheduler()
{
    uint8_t max_prior = 0;
    uint8_t found = 0;
    uint8_t next_task = 0;
    uint8_t i;

    /* Etapa 1: Encontrar a maior prioridade entre todas as tarefas de usuario em READY */
    for (i = 0; i < ReadyQueue.queue_size; i++) {
        if (ReadyQueue.tasks[i].task_func != idle &&
            ReadyQueue.tasks[i].task_state == READY) {
            
            if (!found || ReadyQueue.tasks[i].task_prior > max_prior) {
                max_prior = ReadyQueue.tasks[i].task_prior;
                found = 1;
            }
        }
    }

    /* Se nenhuma tarefa de usuario estiver em READY, retorna a tarefa idle (indice 0) */
    if (!found) {
        return 0;
    }

    /*
     * Etapa 2: Selecao por Round-Robin entre as tarefas READY de maior prioridade.
     * A busca circular inicia em (task_running + 1) e percorre ate queue_size posicoes.
     */
    for (i = 1; i <= ReadyQueue.queue_size; i++) {
#if defined(__XC16__) || defined(__dsPIC30F__) || defined(__dsPIC33F__) || defined(__dsPIC33E__)
        uint8_t candidate = (uint8_t)__builtin_modsd(task_running + i, ReadyQueue.queue_size); //rotação circular da fila
#else
        uint8_t candidate = (uint8_t)((task_running + i) % ReadyQueue.queue_size);
#endif

        if (candidate != 0 &&
            ReadyQueue.tasks[candidate].task_func != idle &&
            ReadyQueue.tasks[candidate].task_state == READY &&
            ReadyQueue.tasks[candidate].task_prior == max_prior) {
            next_task = candidate;
            break;
        }
    }

    return next_task;
}
