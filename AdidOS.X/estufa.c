/**
 * ============================================================================
 * @file    estufa.c
 * @brief   Implementacao da Mini Estufa Inteligente de Skincare (Parte 2)
 * @details Este modulo integra as bibliotecas desenvolvidas na Parte 1:
 *          - [Bloco A] mem.c / mem.h: Alocacao dinamica com coalescing (os_malloc/os_free).
 *          - [Bloco B] scheduler.c / scheduler.h: Escalonamento por Prioridade + Round-Robin.
 *          - [Bloco E] adc.c / adc.h e pwm.c / pwm.h: API de E/S de Hardware (ADC1 e OC1).
 *          - [IPC]     sync.c / message.c: Mutex com PIP, Semaforos e Filas de Mensagens.
 * ============================================================================
 */

#include "estufa.h"
#include "kernel.h"
#include <xc.h>

/* ============================================================================
 * DEFINICAO DAS VARIAVEIS GLOBAIS DE SUPORTE E SINCRONIZACAO
 * ============================================================================ */

/* Ponteiro para a estrutura de status alocada dinamicamente via os_malloc (Bloco A) */
estufa_status_t *p_status_estufa = 0;

/* Recursos de sincronizacao e IPC */
mutex_t         mutex_status;     /* Mutex com Heranca de Prioridade (PIP) */
sem_t           sem_adc_done;     /* Semaforo para sincronizar fim do ciclo de amostragem */
sem_t           sem_alerta;       /* Semaforo de disparo para a tarefa de alta prioridade */
m_queue_t       fila_alertas;     /* Fila de mensagens para a tarefa gerente */
pwm_driver_t    driver_pwm;       /* Estrutura de controle do modulo PWM OC1 (Bloco E) */

/* ============================================================================
 * INICIALIZACAO DA APLICACAO (estufa_init)
 * ============================================================================ */

void estufa_init(void)
{
    /* ------------------------------------------------------------------------
     * 1. INICIALIZACAO DA API DE E/S (Bloco E)
     * ------------------------------------------------------------------------ */
    /* Configura o Conversor AD (AN5/RB5 como entrada analogica, amostragem manual) */
    adc_init();

    /* Configura o PWM no pino RD0/OC1 para frequencia de 1 kHz */
    pwm_config(&driver_pwm, ESTUFA_PWM_FREQ);
    pwm_start(&driver_pwm);
    pwm_set_duty(&driver_pwm, 0);   /* Cooler inicialmente desligado (0% duty) */

    /* ------------------------------------------------------------------------
     * 2. INICIALIZACAO DOS MECANISMOS DE SINCRONIZACAO E IPC
     * ------------------------------------------------------------------------ */
    mutex_init(&mutex_status);
    sem_init(&sem_adc_done, 0);     /* Inicializado em 0 (espera ciclo de leitura) */
    sem_init(&sem_alerta, 0);       /* Inicializado em 0 (espera evento de emergencia) */
    m_queue_init(&fila_alertas);    /* Fila circular sincronizada por semaforos */

    /* ------------------------------------------------------------------------
     * 3. ALOCACAO DINAMICA COM TESTE DE COALESCING (Bloco A)
     * ------------------------------------------------------------------------
     * Demonstra a utilizacao do gerenciador de memoria dinamica do proprio RTOS:
     * - Aloca o bloco principal do status compartilhado.
     * - Valida a coalescencia de blocos adjacentes livres alocando dois buffers
     *   temporarios e liberando-os para fusao contigua no heap.
     */
    p_status_estufa = (estufa_status_t *)os_malloc(sizeof(estufa_status_t));

    /* Validacao de coalescing: aloca 2 blocos contiguos e libera-os */
    void *bloco_teste1 = os_malloc(32);
    void *bloco_teste2 = os_malloc(32);
    os_free(bloco_teste1);
    os_free(bloco_teste2); /* Fusao automatica dos dois blocos contiguos livres */

    /* Inicializa os valores padrao do painel de status compartilhado */
    if (p_status_estufa != 0) {
        p_status_estufa->adc_raw        = 0;
        p_status_estufa->temp_celsius   = 20;
        p_status_estufa->pwm_duty       = 0;
        p_status_estufa->ultimo_monitor = '-';
        p_status_estufa->alerta_critico = 0;
    }

    /* Mensagem de inicializacao no Virtual Terminal (UART a 9600 bps) */
    UART_write_new_line("==============================================");
    UART_write_new_line(" MINI ESTUFA INTELIGENTE DE SKINCARE - AdidOS ");
    UART_write_new_line(" Blocos Ativos: A (Heap), B (Prior+RR), E (E/S) ");
    UART_write_new_line("==============================================");
}

/* ============================================================================
 * [BLOCO B] TAREFAS DE MONITORAMENTO COM ROUND-ROBIN (TaskMonitorA / TaskMonitorB)
 * ============================================================================
 * Ambas possuem a mesma prioridade (PRIORITY_MED = 5).
 * O escalonador prior_scheduler alterna o tempo de CPU entre elas de forma
 * estrita via Round-Robin, garantindo amostragem cooperativa e redundante.
 */

void TaskMonitorA(void)
{
    int16_t  leitura_adc;
    uint8_t  temperatura;

    while (1) {
        /* [Bloco E] Leitura sincronizada da entrada analogica AN5 (potenciometro) */
        leitura_adc = adc_read(ESTUFA_ADC_CHANNEL);
        
        if (leitura_adc >= 0) {
            /* Converte leitura (0 a 1023) para escala de 0 a 50 graus Celsius */
            temperatura = (uint8_t)(((uint32_t)leitura_adc * TEMP_MAX_ESCALA) / 1023UL);

            /* Sincronizacao via Semaforo: sinaliza conclusao de ciclo ADC */
            sem_post(&sem_adc_done);

            /* Acesso Exclusivo ao Status Compartilhado (Mutex com PIP) */
            mutex_lock(&mutex_status);
            p_status_estufa->adc_raw        = (uint16_t)leitura_adc;
            p_status_estufa->temp_celsius   = temperatura;
            p_status_estufa->ultimo_monitor = 'A';
            mutex_unlock(&mutex_status);

            /* Avaliacao de Limiares e Enfileiramento de Mensagens na Fila */
            if (temperatura >= TEMP_CRITICA) {
                /* Dispara o semaforo da tarefa de alta prioridade (TaskAlertaCritico) */
                sem_post(&sem_alerta);
                m_queue_write(&fila_alertas, MSG_TEMP_CRITICAL);
            } else if (temperatura > TEMP_MAX_IDEAL) {
                m_queue_write(&fila_alertas, MSG_TEMP_WARM);
            } else {
                m_queue_write(&fila_alertas, MSG_TEMP_NORMAL);
            }
        }

        /* Pausa temporizada: cede a CPU para a outra tarefa do mesmo nivel (Round-Robin) */
        task_delay(ESTUFA_SAMPLE_DELAY_MS);
    }
}

void TaskMonitorB(void)
{
    int16_t  leitura_adc;
    uint8_t  temperatura;

    while (1) {
        /* [Bloco E] Leitura redundante no mesmo canal analogico AN5 */
        leitura_adc = adc_read(ESTUFA_ADC_CHANNEL);
        
        if (leitura_adc >= 0) {
            temperatura = (uint8_t)(((uint32_t)leitura_adc * TEMP_MAX_ESCALA) / 1023UL);

            /* Sincronizacao via Semaforo */
            sem_post(&sem_adc_done);

            /* Acesso Exclusivo ao Status Compartilhado (Mutex com PIP) */
            mutex_lock(&mutex_status);
            p_status_estufa->adc_raw        = (uint16_t)leitura_adc;
            p_status_estufa->temp_celsius   = temperatura;
            p_status_estufa->ultimo_monitor = 'B';
            mutex_unlock(&mutex_status);

            /* Enfileiramento de Mensagens */
            if (temperatura >= TEMP_CRITICA) {
                sem_post(&sem_alerta);
                m_queue_write(&fila_alertas, MSG_TEMP_CRITICAL);
            } else if (temperatura > TEMP_MAX_IDEAL) {
                m_queue_write(&fila_alertas, MSG_TEMP_WARM);
            } else {
                m_queue_write(&fila_alertas, MSG_TEMP_NORMAL);
            }
        }

        /* Pausa temporizada para alternancia Round-Robin com Monitor A */
        task_delay(ESTUFA_SAMPLE_DELAY_MS);
    }
}

/* ============================================================================
 * TAREFA GERENTE (TaskGerente)
 * ============================================================================
 * Prioridade: BAIXA (PRIORITY_LOW = 1).
 * Responsabilidade:
 * - Consumir mensagens da fila enviadas pelos monitores.
 * - Ajustar o cooler (PWM) proporcionalmente a temperatura dos cosmeticos.
 * - Adquirir o Mutex para atualizar o status e exibir o painel no Virtual Terminal.
 * - Demonstrar o PIP: enquanto estiver com o mutex, se TaskAlertaCritico acordar,
 *   a prioridade desta tarefa sera temporariamente elevada pelo RTOS.
 */

void TaskGerente(void)
{
    char    msg_alerta;
    uint8_t duty_calculado;
    uint8_t temp_atual;
    uint8_t monitor_origem;

    while (1) {
        /* Leitura bloqueante da fila de mensagens (aguarda evento de monitoramento) */
        m_queue_read(&fila_alertas, &msg_alerta);

        /* Seção Crítica protegida por Mutex (pode sofrer heranca de prioridade) */
        mutex_lock(&mutex_status);

        temp_atual     = p_status_estufa->temp_celsius;
        monitor_origem = p_status_estufa->ultimo_monitor;

        /* Logica de atuacao proporcional no cooler via PWM (Bloco E) */
        if (msg_alerta == MSG_TEMP_CRITICAL) {
            duty_calculado = 100;
        } else if (msg_alerta == MSG_TEMP_WARM) {
            /* Modulacao progressiva: de 30% a 80% entre TEMP_MAX_IDEAL e TEMP_CRITICA */
            duty_calculado = 40 + (uint8_t)((temp_atual - TEMP_MAX_IDEAL) * 3);
            if (duty_calculado > 90) duty_calculado = 90;
        } else {
            /* Temperatura ideal mantida: refrigeracao passiva ou minima */
            duty_calculado = 0;
        }

        /* [Bloco E] Atualiza o ciclo de trabalho do PWM */
        pwm_set_duty(&driver_pwm, duty_calculado);
        p_status_estufa->pwm_duty = duty_calculado;

        /* Atualizacao do painel no Virtual Terminal (UART) */
        UART_write_string("[GERENTE] Mon: ");
        UART_write_char(monitor_origem);
        UART_write_string(" | Temp: ");
        UART_write_int((int)temp_atual);
        UART_write_string(" C | PWM: ");
        UART_write_int((int)duty_calculado);
        UART_write_string("% | ");

        if (msg_alerta == MSG_TEMP_NORMAL) {
            UART_write_new_line("Status: Temperatura ideal mantida.");
        } else if (msg_alerta == MSG_TEMP_WARM) {
            UART_write_new_line("Status: Ajustando cooler...");
        } else {
            UART_write_new_line("Status: ALERTA CRITICO!");
        }

        /* Libera o mutex: se houve heranca de prioridade, a prioridade retorna a BAIXA */
        mutex_unlock(&mutex_status);

        /* Pausa entre despachos */
        task_delay(20);
    }
}

/* ============================================================================
 * TAREFA DE ALERTA CRITICO (TaskAlertaCritico)
 * ============================================================================
 * Prioridade: ALTA (PRIORITY_HIGH = 10).
 * Responsabilidade:
 * - Permanece bloqueada no semaforo sem_alerta.
 * - Desperta imediatamente quando a temperatura dos cosmeticos atinge nivel critico.
 * - Tenta adquirir o Mutex: se TaskGerente estiver na secao critica, o PIP
 *   eleva a prioridade de TaskGerente para 10, impedindo starvation.
 * - Forca atuacao imediata do cooler em 100% e emite aviso de emergencia.
 */

void TaskAlertaCritico(void)
{
    while (1) {
        /* Bloqueia aguardando semaforo de emergencia */
        sem_wait(&sem_alerta);

        /* Requisita o Mutex: ponto central de demonstracao do PIP */
        mutex_lock(&mutex_status);

        p_status_estufa->alerta_critico = 1;

        /* [Bloco E] Potencia maxima imediata no cooler */
        pwm_set_duty(&driver_pwm, 100);
        p_status_estufa->pwm_duty = 100;

        UART_write_new_line(">>> [EMERGENCIA - TaskAlertaCritico] Superaquecimento! <<<");
        UART_write_string(">>> Resfriamento MAXIMO acionado (PWM 100%) - Temp: ");
        UART_write_int((int)p_status_estufa->temp_celsius);
        UART_write_new_line(" C <<<");

        p_status_estufa->alerta_critico = 0;

        mutex_unlock(&mutex_status);

        /* Evita re-disparos continuos instantaneos */
        task_delay(50);
    }
}
