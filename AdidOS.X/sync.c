#include "sync.h"
#include "int.h"
#include <xc.h>


// Variáveis globais
extern queue_t ReadyQueue;
extern uint8_t task_running;
extern uint16_t *task_running_tos;


void sem_init(sem_t *s, uint8_t value)
{
    s->sem_count        = value;
    s->s_queue_input    = 0;
    s->s_queue_output   = 0;
}

void sem_wait(sem_t *s)
{
    __builtin_disable_interrupts();
    
    s->sem_count--;
    
    if (s->sem_count < 0) {
        // Bloqueia a tarefa
        s->sem_queue[s->s_queue_input] = task_running;
        s->s_queue_input = __builtin_modsd(s->s_queue_input+1, MAX_TASKS);
        //s->s_queue_input = (s->s_queue_input+1) % MAX_TASKS;
        ReadyQueue.tasks[task_running].task_state = BLOCKED;
        CONTEXT_SWITCH();        
    }
    
    __builtin_enable_interrupts();    
}

void sem_post(sem_t *s)
{
    __builtin_disable_interrupts();
    
    s->sem_count++;
    
    if (s->sem_count <= 0) {
        ReadyQueue.tasks[s->sem_queue[s->s_queue_output]].task_state = READY;
        s->s_queue_output = __builtin_modsd(s->s_queue_output+1, MAX_TASKS);
        //s->s_queue_output = (s->s_queue_output+1) % MAX_TASKS;
        ReadyQueue.tasks[task_running].task_state = READY;
        CONTEXT_SWITCH(); 
    }
    
    __builtin_enable_interrupts();    
}


void mutex_init(mutex_t *m)
{
    m->locked       = 0;
    m->owner        = 0;
    m->wq_input     = 0;
    m->wq_output    = 0;
    m->wq_count     = 0;
}

void mutex_lock(mutex_t *m)
{
    __builtin_disable_interrupts();

    if (m->locked == 0) {
        m->locked = 1;
        m->owner  = task_running;
    } else {
        // Herança de prioridade (valor maior = prioridade maior)
        if (ReadyQueue.tasks[m->owner].task_prior < ReadyQueue.tasks[task_running].task_prior) {
            ReadyQueue.tasks[m->owner].task_prior = ReadyQueue.tasks[task_running].task_prior;
        }

        // Bloqueia a tarefa
        m->wait_queue[m->wq_input] = task_running;
        m->wq_input = __builtin_modsd(m->wq_input+1, MAX_TASKS);
        m->wq_count++;
        ReadyQueue.tasks[task_running].task_state = BLOCKED;
        CONTEXT_SWITCH();
    }

    __builtin_enable_interrupts();
}

void mutex_unlock(mutex_t *m)
{
    uint8_t next;

    __builtin_disable_interrupts();

    // Só o dono pode liberar
    if (m->locked == 0 || m->owner != task_running) {
        __builtin_enable_interrupts();
        return;
    }

    // Limitação: volta direto para a prioridade base (não trata tarefa com dois mutex)
    ReadyQueue.tasks[task_running].task_prior = ReadyQueue.tasks[task_running].task_base_prior;

    if (m->wq_count > 0) {
        // O mutex passa direto para o primeiro da fila
        next = m->wait_queue[m->wq_output];
        m->wq_output = __builtin_modsd(m->wq_output+1, MAX_TASKS);
        m->wq_count--;
        m->owner = next;
        ReadyQueue.tasks[next].task_state = READY;
        ReadyQueue.tasks[task_running].task_state = READY;
        CONTEXT_SWITCH();
    } else {
        m->locked = 0;
    }

    __builtin_enable_interrupts();
}
