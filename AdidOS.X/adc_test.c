/**
 * ============================================================================
 * @file    adc_test.c
 * @brief   Modulo de teste para validacao do driver de Conversor AD (ADC1).
 * @details Implementa uma tarefa dedicada (compativel com o RTOS AdidOS e
 *          tambem executavel em modo standalone/bare-metal) que inicializa o
 *          driver com adc_init() e, periodicamente em loop, efetua leituras
 *          do canal analogico selecionado com adc_read(), atualizando uma
 *          variavel global observavel no depurador e transmitindo o resultado
 *          formatado via UART.
 *
 * ============================================================================
 * ROTEIRO DE VERIFICACAO E TESTE (PROTEUS E PLACA REAL)
 * ============================================================================
 *
 * 1. FUNDAMENTO TEORICO DO CONVERSOR (PROPORCAO ESPERADA):
 * ----------------------------------------------------------------------------
 * O Conversor AD da familia dsPIC/PIC24 opera com resolucao de 10 bits:
 *   - Numero de niveis discretos: 2^10 = 1024 niveis (faixa de 0 a 1023).
 *   - Tensao de referencia superior (VREF+): VDD = 3.3V (ou 5.0V conforme a placa).
 *   - Tensao de referencia inferior (VREF-): VSS = 0.0V (Terra).
 *   - Resolucao por LSB (Least Significant Bit):
 *       LSB = VDD / 1024 ~= 3.3V / 1024 ~= 3.22 mV / contagem.
 *
 * A relacao matematica entre a tensao de entrada aplicada (Vin) e o valor lido e:
 *       Valor_Digital = (Vin / VDD) * 1023
 *
 * Tabela de Proporcao Teorica (para VDD = 3.3V):
 *   +-----------------+---------------------+-------------------------------+
 *   | Tensao Vin (V)  | Proporcao de VDD    | Leitura Digital Esperada      |
 *   +-----------------+---------------------+-------------------------------+
 *   |     0.00 V      |   0.0% (GND)        |    0  (tolerancia +- 2 LSB)   |
 *   |     0.825 V     |  25.0% (1/4 VDD)    |  256  (tolerancia +- 3 LSB)   |
 *   |     1.650 V     |  50.0% (1/2 VDD)    |  512  (tolerancia +- 3 LSB)   |
 *   |     2.475 V     |  75.0% (3/4 VDD)    |  767  (tolerancia +- 3 LSB)   |
 *   |     3.300 V     | 100.0% (VDD)        | 1023  (tolerancia +- 2 LSB)   |
 *   +-----------------+---------------------+-------------------------------+
 *
 * ----------------------------------------------------------------------------
 * 2. PROCEDIMENTO DE CONFERENCIA NO SIMULADOR PROTEUS:
 * ----------------------------------------------------------------------------
 * a) Abra o projeto de simulacao 'Simulacao.pdsprj' no Proteus ISIS.
 * b) Verifique a conexao do pino analogico configurado (padrao: canal AN5 no
 *    pino RB5 / Pino 24 do PIC24FJ128GA010):
 *      - Conecte um potenciometro interativo ('POT-HG') entre o VDD (3.3V) e o
 *        VSS (GND).
 *      - Conecte o terminal central variavel (wiper) ao pino AN5/RB5.
 *      - Adicione um instrumento 'DC VOLTMETER' ao pino AN5 para medir a tensao
 *        real aplicada Vin.
 * c) Para observar a saida serial no Proteus:
 *      - No menu de instrumentos virtuais, insira um 'VIRTUAL TERMINAL'.
 *      - Conecte o pino RXD do Virtual Terminal ao pino TXD da UART1 do
 *        microcontrolador (pino RF3 no PIC24FJ128GA010).
 *      - Configure o Virtual Terminal com: Baud Rate = 9600, Data Bits = 8,
 *        Parity = None, Stop Bits = 1.
 * d) Inicie a simulacao (Play):
 *      - Com o potenciometro em 0%, o voltimetro marca 0.00V e o terminal
 *        exibira: "[ADC] Canal AN5 = 0 (0 mV)".
 *      - Com o potenciometro em 50%, o voltimetro marca 1.65V e o terminal
 *        exibira: "[ADC] Canal AN5 = 511..513 (~1650 mV)".
 *      - Com o potenciometro em 100%, o voltimetro marca 3.30V e o terminal
 *        exibira: "[ADC] Canal AN5 = 1023 (3300 mV)".
 * e) Inspecao por variavel: Durante a simulacao no Proteus, acesse
 *    Debug -> Watch Window e adicione o simbolo 'adc_last_value' para
 *    acompanhar a leitura sem depender da serial.
 *
 * ----------------------------------------------------------------------------
 * 3. PROCEDIMENTO DE CONFERENCIA NA PLACA REAL (EXPLORER 16 / HARDWARE dsPIC):
 * ----------------------------------------------------------------------------
 * a) Conecte a placa ao computador por meio de um cabo serial ou conversor
 *    USB-UART (baudrate de 9600 bps, 8N1).
 * b) Abra um software emulador de terminal serial (ex: PuTTY, Tera Term, Minicom
 *    ou o Data Visualizer do MPLAB X).
 * c) Na placa Explorer 16, o potenciometro rotativo R6 de 10k ja esta conectado
 *    diretamente ao canal AN5 (RB5). Caso use outra entrada ou fonte externa,
 *    conecte um gerador de tensao DC ajustavel entre 0V e 3.3V no pino desejado.
 *    ATENCAO: Nunca aplique tensao superior a VDD (3.3V) para evitar danos
 *    irreversiveis aos diodos de protecao ESD do microcontrolador.
 * d) Gire o potenciometro ou varie a fonte e observe as leituras continuas
 *    na tela do terminal. Meça a tensao no pino com um multimetro digital calibrado
 *    e confirme que a leitura segue exatamente a proporcao da tabela acima.
 * e) Depuracao no MPLAB X: conecte o gravador/depurador (PICkit / ICD),
 *    compile em modo Debug, abra a janela 'Watches' (Window -> Debugging -> Watches)
 *    e adicione a variavel 'adc_last_value'.
 * ============================================================================
 */

#include <xc.h>
#include <stdint.h>
#include "adc.h"
#include "uart.h"

/* Inclusao condicional do kernel do sistema operacional AdidOS, se presente */
#if defined(MAX_TASKS) || defined(OS_CONFIG_H)
#include "kernel.h"
#include "types.h"
#else
/* Compatibilidade caso o teste seja compilado isoladamente sem o SO */
typedef void TASK;
#endif

/* ============================================================================
 * CONFIGURACOES DO TESTE
 * ============================================================================ */

/**
 * @brief Canal analogico utilizado no teste.
 * O canal 5 (AN5) corresponde ao potenciometro da placa Explorer 16 e do Proteus.
 */
#define ADC_TEST_CHANNEL            5

/**
 * @brief Tensão de referência em milivolts (VDD = 3300 mV).
 */
#define ADC_VREF_MV                 3300UL

/* ============================================================================
 * VARIAVEIS OBSERVAVEIS NO DEPURADOR / SIMULADOR
 * ============================================================================ */

/**
 * @brief Variavel global contendo a ultima leitura digitalizada (0 a 1023).
 * Marcada como 'volatile' para evitar que o compilador a otimize, permitindo
 * sua inspecao em tempo real na janela Watches do MPLAB X e no Proteus.
 */
volatile int16_t adc_last_value = 0;

/**
 * @brief Tensao calculada correspondente em milivolts (0 a 3300 mV).
 */
volatile uint16_t adc_voltage_mv = 0;

/**
 * @brief Contador de conversoes realizadas com sucesso.
 */
volatile uint32_t adc_read_count = 0;

/**
 * @brief Contador de falhas (estouros de timeout ou canal invalido).
 */
volatile uint32_t adc_error_count = 0;

/* ============================================================================
 * TAREFA DE TESTE DEDICADA
 * ============================================================================ */

/**
 * @brief Tarefa de teste do Conversor AD.
 * @details Inicializa o hardware do ADC uma unica vez atraves de adc_init() e,
 *          em laco continuo, amostra e converte a tensao no canal ADC_TEST_CHANNEL,
 *          calcula a tensao em milivolts e publica os dados pela UART e pelas
 *          variaveis observaveis.
 */
TASK adc_test_task(void)
{
    /* 1. Inicializa o Conversor AD (configura AD1PCFG, AD1CHS, AD1CON3, AD1CON2, AD1CON1) */
    adc_init();

    /* 2. Inicializa a UART para transmissao de telemetria a 9600 bps */
    UART_init(9600);
    UART_write_new_line("--- [AdidOS] Teste do Driver ADC1 Iniciado ---");

    while (1) {
        /* Realiza a leitura sincrona bloqueante no canal selecionado */
        int16_t raw_value = adc_read(ADC_TEST_CHANNEL);

        if (raw_value >= 0) {
            /* Leitura valida (0 a 1023) */
            adc_last_value = raw_value;
            adc_read_count++;

            /* Converte a contagem digital para tensao em milivolts:
             * V_mv = (raw_value * 3300) / 1023 */
            adc_voltage_mv = (uint16_t)(((uint32_t)raw_value * ADC_VREF_MV) / 1023UL);

            /* Transmite pela UART formatado para visualizacao no terminal */
            UART_write_string("[ADC] Canal AN");
            UART_write_int(ADC_TEST_CHANNEL);
            UART_write_string(" = ");
            UART_write_int(adc_last_value);
            UART_write_string(" (Tensao: ");
            UART_write_int(adc_voltage_mv);
            UART_write_new_line(" mV)");
        } else {
            /* Ocorreu erro de leitura (timeout ou canal invalido) */
            adc_error_count++;
            UART_write_string("[ADC ERROR] Falha na conversao! Codigo: ");
            UART_write_int(raw_value);
            UART_write_new_line("");
        }

        /* Intervalo entre leituras:
         * Se estiver rodando sob o RTOS AdidOS, utiliza task_delay() para ceder
         * a CPU para outras tarefas do sistema (ex: tarefa_b ou outras).
         * Caso contrario, executa um retardo simples de polling por software. */
#if defined(MAX_TASKS) || defined(OS_CONFIG_H)
        task_delay(50); /* 50 ticks do SO */
#else
        {
            volatile uint32_t d;
            for (d = 0; d < 80000UL; d++) {
                /* Retardo software simples */
            }
        }
#endif
    }
}
