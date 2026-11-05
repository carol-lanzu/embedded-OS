#ifndef ESTUFA_H
#define ESTUFA_H

#include <stdint.h>
#include <stdbool.h>
#include "types.h"
#include "sync.h"
#include "message.h"
#include "adc.h"
#include "pwm.h"
#include "uart.h"
#include "mem.h"

#define ESTUFA_ADC_CHANNEL          5       //AN5 = RB5 (Pino 20) 

//Limiares de Temperatura (em graus Celsius) para cosmeticos sensiveis 
#define TEMP_MIN_IDEAL              12      
#define TEMP_MAX_IDEAL              22      
#define TEMP_ALERTA_MODERADO        28     
#define TEMP_CRITICA                36      
#define TEMP_MAX_ESCALA             50      

//Frequencia de operacao do PWM do cooler/termoeletrico (em Hz) 
#define ESTUFA_PWM_FREQ             1000    // 1 kHz 

//Periodo de amostragem/atraso das tarefas de monitoramento (em ticks/ms) 
#define ESTUFA_SAMPLE_DELAY_MS      30      // Delay para alternancia Round-Robin 

// Codigos de mensagens para a fila m_queue 
#define MSG_TEMP_NORMAL             'N'     // Temperatura dentro da faixa ideal 
#define MSG_TEMP_WARM               'W'     // Temperatura elevada (precisa cooler) 
#define MSG_TEMP_CRITICAL           'C'     // Temperatura critica de emergencia 

//ESTRUTURA DE STATUS COMPARTILHADO (Protegida por Mutex)

typedef struct {
    uint16_t adc_raw;           // Leitura digital bruta do ADC (0 - 1023) 
    uint8_t  temp_celsius;       
    uint8_t  pwm_duty;          // Ciclo de trabalho do cooler (0 a 100%) 
    uint8_t  ultimo_monitor;    // Identificador: 'A' (Monitor A) ou 'B' (Monitor B) 
    uint8_t  alerta_critico;    // 1 se alarme critico ativo, 0 caso normal 
} estufa_status_t;

//DECLARACAO DOS RECURSOS GLOBAIS DE SINCRONIZACAO E COMUNICACAO

extern estufa_status_t *p_status_estufa; // Alocado dinamicamente no heap  
extern mutex_t         mutex_status;     // Mutex com Heranca de Prioridade (PIP) 
extern sem_t           sem_adc_done;     // Semaforo de conclusao de ciclo ADC 
extern sem_t           sem_alerta;       // Semaforo de disparo para TaskAlertaCritico 
extern m_queue_t       fila_alertas;     // Fila de mensagens para TaskGerente 
extern pwm_driver_t    driver_pwm;       // Estrutura do driver PWM 

//PROTOTIPOS DAS FUNCOES E TAREFAS DA APLICACAO

//Inicializacao do sistema e dos perifericos 
void estufa_init(void);

//Tarefas do RTOS 
void TaskMonitorA(void);        //Prioridade MEDIA: Round-Robin com Monitor B 
void TaskMonitorB(void);        //Prioridade MEDIA: Round-Robin com Monitor A 
void TaskGerente(void);         //Prioridade BAIXA: Controle PWM e painel UART 
void TaskAlertaCritico(void);   //Prioridade ALTA: Alarme rapido e teste PIP 

#endif 
