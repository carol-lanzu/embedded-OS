#include "sync.h"
#include "int.h"
#include <xc.h>

// Variaveis globais do kernel
extern queue_t ReadyQueue;
extern uint8_t task_running;
extern uint16_t *task_running_tos;

/* ============================================================================
 * SEMAFOROS CONTADORES
 * ============================================================================ */

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
        // Bloqueia a tarefa corrente
        s->sem_queue[s->s_queue_input] = task_running;
        s->s_queue_input = (uint8_t)__builtin_modsd(s->s_queue_input + 1, MAX_TASKS);
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
        s->s_queue_output = (uint8_t)__builtin_modsd(s->s_queue_output + 1, MAX_TASKS);
        ReadyQueue.tasks[task_running].task_state = READY;
        CONTEXT_SWITCH(); 
    }
    
    __builtin_enable_interrupts();    
}

/* ============================================================================
 * MUTEX COM PROTOCOLO DE HERANCA DE PRIORIDADE (PIP)
 * ============================================================================ */

/**
 * @brief Inicializa o mutex com estado livre e sem proprietario.
 */
void mutex_init(mutex_t *m)
{
    m->lock       = 0;
    m->owner      = 255;
    m->orig_prior = 0;
    m->wait_count = 0;
    m->q_in       = 0;
    m->q_out      = 0;
}

/**
 * @brief Adquire o mutex ou bloqueia aplicando Heranca de Prioridade.
 * Se o mutex ja pertencer a outra tarefa e a tarefa solicitante tiver
 * maior prioridade, a prioridade da tarefa detentora e temporariamente
 * elevada para evitar a inversao de prioridade por tarefas intermediarias.
 */
void mutex_lock(mutex_t *m)
{
    __builtin_disable_interrupts();
    
    if (m->lock == 0) {
        // Mutex livre: concede imediatamente a tarefa corrente
        m->lock       = 1;
        m->owner      = task_running;
        m->orig_prior = ReadyQueue.tasks[task_running].task_prior;
    } else {
        // Mutex ocupado: verifica necessidade de Heranca de Prioridade (PIP)
        uint8_t owner = m->owner;
        
        if (ReadyQueue.tasks[task_running].task_prior > ReadyQueue.tasks[owner].task_prior) {
            // Eleva temporariamente a prioridade da tarefa detentora
            ReadyQueue.tasks[owner].task_prior = ReadyQueue.tasks[task_running].task_prior;
        }
        
        // Enfileira a tarefa corrente na fila de espera do mutex
        m->wait_queue[m->q_in] = task_running;
        m->q_in = (uint8_t)__builtin_modsd(m->q_in + 1, MAX_TASKS);
        m->wait_count++;
        
        // Bloqueia e aciona a troca de contexto
        ReadyQueue.tasks[task_running].task_state = BLOCKED;
        CONTEXT_SWITCH();
    }
    
    __builtin_enable_interrupts();
}

/**
 * @brief Libera o mutex, restaura a prioridade original e acorda a proxima tarefa.
 */
void mutex_unlock(mutex_t *m)
{
    __builtin_disable_interrupts();
    
    if (m->lock == 1 && m->owner == task_running) {
        // Restaura a prioridade original da tarefa que possuia o mutex
        ReadyQueue.tasks[task_running].task_prior = m->orig_prior;
        
        if (m->wait_count > 0) {
            // Transfere a posse do mutex para a primeira tarefa da fila de espera
            uint8_t next_task = m->wait_queue[m->q_out];
            m->q_out = (uint8_t)__builtin_modsd(m->q_out + 1, MAX_TASKS);
            m->wait_count--;
            
            m->owner      = next_task;
            m->orig_prior = ReadyQueue.tasks[next_task].task_prior;
            
            // Desbloqueia a tarefa contemplada
            ReadyQueue.tasks[next_task].task_state = READY;
            
            // Cede a CPU para que o escalonador execute a tarefa de maior prioridade
            ReadyQueue.tasks[task_running].task_state = READY;
            CONTEXT_SWITCH();
        } else {
            // Fila vazia: libera totalmente o mutex
            m->lock  = 0;
            m->owner = 255;
        }
    }
    
    __builtin_enable_interrupts();
}
