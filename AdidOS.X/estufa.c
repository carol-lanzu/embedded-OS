#include "estufa.h"
#include "kernel.h"
#include <xc.h>

estufa_status_t *p_status_estufa = 0;

mutex_t         mutex_status;     // Mutex com Heranca de Prioridade (PIP) 
sem_t           sem_adc_done;     // Semaforo para sincronizar fim do ciclo de amostragem 
sem_t           sem_alerta;       // Semaforo de disparo para a tarefa de alta prioridade 
m_queue_t       fila_alertas;     // Fila de mensagens para a tarefa gerente 
pwm_driver_t    driver_pwm;       // Estrutura de controle do modulo PWM OC1 (Bloco E) 

void estufa_init(void)
{
    adc_init();

    pwm_config(&driver_pwm, ESTUFA_PWM_FREQ);
    pwm_start(&driver_pwm);
    pwm_set_duty(&driver_pwm, 0);   

    mutex_init(&mutex_status);
    sem_init(&sem_adc_done, 0);    
    sem_init(&sem_alerta, 0);       
    m_queue_init(&fila_alertas);    

    //Demonstra a utilizacao do gerenciador de memoria dinamica do proprio RTOS
    
    p_status_estufa = (estufa_status_t *)os_malloc(sizeof(estufa_status_t));

    void *bloco_teste1 = os_malloc(32);
    void *bloco_teste2 = os_malloc(32);
    os_free(bloco_teste1);
    os_free(bloco_teste2);

    //Inicializa os valores padrao do painel de status compartilhado
    if (p_status_estufa != 0) {
        p_status_estufa->adc_raw        = 0;
        p_status_estufa->temp_celsius   = 20;
        p_status_estufa->pwm_duty       = 0;
        p_status_estufa->ultimo_monitor = '-';
        p_status_estufa->alerta_critico = 0;
    }

    //Mensagem de inicializacao no Virtual Terminal 
    UART_write_new_line("==============================================");
    UART_write_new_line(" MINI ESTUFA INTELIGENTE DE SKINCARE - AdidOS ");
    UART_write_new_line(" Blocos Ativos: A (Heap), B (Prior+RR), E (E/S) ");
    UART_write_new_line("==============================================");
}

//Tarefas com round-robin (TaskMonitorA / TaskMonitorB)

void TaskMonitorA(void)
{
    int16_t  leitura_adc;
    uint8_t  temperatura;

    while (1) {
        leitura_adc = adc_read(ESTUFA_ADC_CHANNEL);
        
        if (leitura_adc >= 0) {
            //Converte leitura (0 a 1023) para escala de 0 a 50 graus Celsius 
            temperatura = (uint8_t)(((uint32_t)leitura_adc * TEMP_MAX_ESCALA) / 1023UL);

            //Sincronizacao via Semaforo: sinaliza conclusao de ciclo ADC
            sem_post(&sem_adc_done);

            //Acesso Exclusivo ao Status Compartilhado (Mutex com PIP) 
            mutex_lock(&mutex_status);
            p_status_estufa->adc_raw        = (uint16_t)leitura_adc;
            p_status_estufa->temp_celsius   = temperatura;
            p_status_estufa->ultimo_monitor = 'A';
            mutex_unlock(&mutex_status);

            //Avaliacao de Limiares e Enfileiramento de Mensagens na Fila
            if (temperatura >= TEMP_CRITICA) {
                //Dispara o semaforo da tarefa de alta prioridade (TaskAlertaCritico) 
                sem_post(&sem_alerta);
                m_queue_write(&fila_alertas, MSG_TEMP_CRITICAL);
            } else if (temperatura > TEMP_MAX_IDEAL) {
                m_queue_write(&fila_alertas, MSG_TEMP_WARM);
            } else {
                m_queue_write(&fila_alertas, MSG_TEMP_NORMAL);
            }
        }

        //Pausa temporizada: cede a CPU para a outra tarefa do mesmo nivel (Round-Robin) 
        task_delay(ESTUFA_SAMPLE_DELAY_MS);
    }
}

void TaskMonitorB(void)
{
    int16_t  leitura_adc;
    uint8_t  temperatura;

    while (1) {
        leitura_adc = adc_read(ESTUFA_ADC_CHANNEL);
        
        if (leitura_adc >= 0) {
            temperatura = (uint8_t)(((uint32_t)leitura_adc * TEMP_MAX_ESCALA) / 1023UL);

            sem_post(&sem_adc_done);

            mutex_lock(&mutex_status);
            p_status_estufa->adc_raw        = (uint16_t)leitura_adc;
            p_status_estufa->temp_celsius   = temperatura;
            p_status_estufa->ultimo_monitor = 'B';
            mutex_unlock(&mutex_status);

            if (temperatura >= TEMP_CRITICA) {
                sem_post(&sem_alerta);
                m_queue_write(&fila_alertas, MSG_TEMP_CRITICAL);
            } else if (temperatura > TEMP_MAX_IDEAL) {
                m_queue_write(&fila_alertas, MSG_TEMP_WARM);
            } else {
                m_queue_write(&fila_alertas, MSG_TEMP_NORMAL);
            }
        }

        task_delay(ESTUFA_SAMPLE_DELAY_MS);
    }
}

void TaskGerente(void)
{
    char    msg_alerta;
    uint8_t duty_calculado;
    uint8_t temp_atual;
    uint8_t monitor_origem;

    while (1) {
        m_queue_read(&fila_alertas, &msg_alerta);

        mutex_lock(&mutex_status);

        temp_atual     = p_status_estufa->temp_celsius;
        monitor_origem = p_status_estufa->ultimo_monitor;

        //Logica de atuacao proporcional no cooler via PWM 
        if (msg_alerta == MSG_TEMP_CRITICAL) {
            duty_calculado = 100;
        } else if (msg_alerta == MSG_TEMP_WARM) {
            //Forca de 30 a 80% do motor
            duty_calculado = 40 + (uint8_t)((temp_atual - TEMP_MAX_IDEAL) * 3);
            if (duty_calculado > 90) duty_calculado = 90;
        } else {
            //Mantem a temperatura
            duty_calculado = 0;
        }

        //Atualiza o ciclo de trabalho do PWM
        pwm_set_duty(&driver_pwm, duty_calculado);
        p_status_estufa->pwm_duty = duty_calculado;

        //Atualizacao do painel no Virtual Terminal (UART) 
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

        mutex_unlock(&mutex_status);

        task_delay(20);
    }
}

void TaskAlertaCritico(void)
{
    while (1) {
        //Bloqueia aguardando semaforo de emergencia
        sem_wait(&sem_alerta);

        mutex_lock(&mutex_status);

        p_status_estufa->alerta_critico = 1;

        pwm_set_duty(&driver_pwm, 100);
        p_status_estufa->pwm_duty = 100;

        UART_write_new_line(">>> [EMERGENCIA - TaskAlertaCritico] Superaquecimento! <<<");
        UART_write_string(">>> Resfriamento MAXIMO acionado (PWM 100%) - Temp: ");
        UART_write_int((int)p_status_estufa->temp_celsius);
        UART_write_new_line(" C <<<");

        p_status_estufa->alerta_critico = 0;

        mutex_unlock(&mutex_status);

        // Evita re-disparos continuos instantaneos
        task_delay(50);
    }
}
