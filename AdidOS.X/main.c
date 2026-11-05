/**
 * ============================================================================
 * @file    main.c
 * @brief   Ponto de Entrada da Aplicacao: Mini Estufa Inteligente de Skincare
 * @details Inicializa o kernel AdidOS, configura a aplicacao integrada da Estufa
 *          e registra as 4 tarefas de validacao da Parte 2 do trabalho:
 *          - TaskMonitorA     (Prioridade MEDIA = 5) -> Round-Robin
 *          - TaskMonitorB     (Prioridade MEDIA = 5) -> Round-Robin
 *          - TaskGerente      (Prioridade BAIXA = 1) -> Consumo de fila / Mutex
 *          - TaskAlertaCritico(Prioridade ALTA  = 10)-> Heranca de Prioridade (PIP)
 * ============================================================================
 */

#include "kernel.h"
#include "estufa.h"

int main(void)
{
    /* 1. Inicializa o hardware basico e as estruturas do SO */
    os_config();

    /* 2. Inicializa os perifericos (ADC/PWM) e estruturas dinamicas da Estufa (Blocos A e E) */
    estufa_init();

    /* ------------------------------------------------------------------------
     * 3. CRIACAO DAS TAREFAS DA APLICACAO (Bloco B)
     * ------------------------------------------------------------------------
     * Convencao de prioridades: Maior valor numerico = Maior prioridade.
     * TaskMonitorA e TaskMonitorB possuem mesma prioridade (PRIORITY_MED = 5)
     * demonstrando a alternancia por Round-Robin.
     * TaskGerente possui prioridade BAIXA (PRIORITY_LOW = 1).
     * TaskAlertaCritico possui prioridade ALTA (PRIORITY_HIGH = 10) para PIP.
     */
    create_task(TaskMonitorA,      PRIORITY_MED);
    create_task(TaskMonitorB,      PRIORITY_MED);
    create_task(TaskGerente,       PRIORITY_LOW);
    create_task(TaskAlertaCritico, PRIORITY_HIGH);

    /* 4. Dispara o escalonador preemptivo baseado em prioridades */
    os_start_scheduler();

    /* Loop de seguranca caso o escalonador retorne */
    while (1);

    return 0;
}
