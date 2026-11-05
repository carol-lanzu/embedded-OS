/**
 * ============================================================================
 * @file    adc.h
 * @brief   Driver do Conversor Analogico-Digital (ADC) de 10 bits para dsPIC/PIC24.
 * @details Este arquivo define as constantes configuraveis, tipos de dados,
 *          codigos de erro e as interfaces publicas para inicializacao e leitura
 *          sincrona (polling) do modulo ADC (AD1).
 * ============================================================================
 */

#ifndef ADC_H
#define ADC_H

#include <stdint.h>
#include <stdbool.h>

/* ============================================================================
 * PARAMETROS CONFIGURAVEIS DO CONVERSOR AD
 * Ajuste estas constantes conforme o cristal/oscilador do seu sistema (FCY),
 * os canais analogicos utilizados na sua placa e a velocidade de amostragem.
 * ============================================================================ */

/**
 * @brief Frequencia de instrucao da CPU (FCY).
 * O valor padrao do projeto AdidOS e 4 MHz (FCY = 4000000UL, TCY = 250 ns).
 * Ajuste este valor se o seu oscilador ou multiplicador PLL for diferente.
 */
#ifndef FCY
#define FCY                         4000000UL
#endif

/**
 * @brief Mascara de configuracao dos pinos em AD1PCFG.
 * No dsPIC/PIC24, cada bit corresponde a uma entrada analogica ANx:
 *   - Bit = 0: Pino configurado como ENTRADA ANALOGICA (buffer digital desativado).
 *   - Bit = 1: Pino configurado como I/O DIGITAL (buffer digital ativado).
 *
 * Valor padrao: 0xFFDF (Bit 5 em '0' -> AN5 como analogico; AN0..AN4 e AN6..AN15 digitais).
 * Nota: Na placa Explorer 16 e nas simulacoes padrao do Proteus para PIC24FJ128GA010,
 * o potenciometro de teste encontra-se conectado ao canal AN5 (pino RB5).
 * Para habilitar outros pinos (ex: AN0 = 0xFFFE, AN0..AN3 = 0xFFF0), ajuste aqui.
 */
#ifndef ADC_PCFG_CONFIG
#define ADC_PCFG_CONFIG             0xFFDF
#endif

/**
 * @brief Divisor do clock de conversao (ADCS) no registrador AD1CON3.
 * O periodo de conversao TAD e dado por: TAD = TCY * (ADCS + 1).
 * Para FCY = 4 MHz (TCY = 250 ns):
 *   ADCS = 4  -->  TAD = 250 ns * (4 + 1) = 1.25 us.
 * O datasheet do PIC24F/dsPIC exige TAD minimo de 75 ns (recomendado >= 150 ns).
 * Um TAD de 1.25 us e estavel, seguro e garante conversao com precisao integral.
 */
#ifndef ADC_ADCS_CONFIG
#define ADC_ADCS_CONFIG             4
#endif

/**
 * @brief Ciclos de espera para aquisicao e carga do capacitor de amostragem (Chold).
 * Durante o modo manual (SAMP = 1), o circuito de amostragem precisa de um tempo
 * minimo de aquisicao (T_samp) para que o capacitor interno Chold atinja a tensao
 * da entrada externa antes de iniciar a conversao SAR.
 * Um loop de 50 iteracoes a 4 MHz fornece aproximadamente 15 a 25 us, atendendo
 * com ampla margem a impedancia maxima de fonte recomendada (2.5 kOhms).
 */
#ifndef ADC_SAMPLE_TIME_CYCLES
#define ADC_SAMPLE_TIME_CYCLES      50
#endif

/**
 * @brief Limite maximo de iteracoes para o timeout da conversao.
 * Protecao de tempo de espera que impede que adc_read() fique presa
 * indefinidamente em laco caso o bit DONE nao seja setado pelo hardware
 * (por exemplo, falha de clock, modulo desalimentado ou registrador travado).
 */
#ifndef ADC_TIMEOUT_LIMIT
#define ADC_TIMEOUT_LIMIT           20000UL
#endif

/**
 * @brief Canal maximo suportado pelo multiplexador MUX A (AN0 a AN15).
 */
#define ADC_MAX_CHANNEL             15

/* ============================================================================
 * CODIGOS DE RETORNO E ERRO DA FUNCAO adc_read
 * Valores >= 0 (0 a 1023) indicam conversoes bem-sucedidas em resolucao de 10 bits.
 * Valores negativos indicam falhas de operacao devidamente identificadas.
 * ============================================================================ */

/** @brief Conversao nao terminou dentro do tempo limite estipulado (estouro de timeout). */
#define ADC_ERROR_TIMEOUT           (-1)

/** @brief Canal informado e invalido (maior que ADC_MAX_CHANNEL). */
#define ADC_ERROR_INVALID_CHANNEL   (-2)

/** @brief Modulo ADC nao esta habilitado (ADON == 0). */
#define ADC_ERROR_NOT_ENABLED       (-3)

/* ============================================================================
 * PROTOTIPOS DAS FUNCOES PUBLICAS
 * ============================================================================ */

/**
 * @brief Inicializa e configura o Conversor Analogico-Digital (ADC1).
 * @details Executa a sequencia canonica de configuracao da familia Microchip:
 *          1. Define os pinos analogicos no registrador AD1PCFG e no TRISB.
 *          2. Configura o multiplexador de canais no registrador AD1CHS.
 *          3. Ajusta a temporizacao (TAD e fonte de clock) no registrador AD1CON3.
 *          4. Configura os modos de referencia e amostragem em AD1CON2 e AD1CON1.
 *          5. Liga o modulo ao final (AD1CON1bits.ADON = 1).
 */
void adc_init(void);

/**
 * @brief Realiza a leitura sincrona e bloqueante de um canal analogico por polling.
 * @details Dispara a amostragem do canal indicado, aguarda o tempo de aquisicao,
 *          encerra a amostragem limpando o bit SAMP (disparando a conversao),
 *          aguarda a conclusao pelo bit DONE de AD1CON1 com protecao de timeout
 *          e retorna o valor digitalizado de 10 bits obtido em ADC1BUF0.
 *
 * @param channel Indice do canal analogico desejado (0 para AN0 ate 15 para AN15).
 * @return int16_t Valor digitalizado entre 0 e 1023 (10 bits) se sucesso,
 *                 ou codigo de erro negativo (ADC_ERROR_TIMEOUT, ADC_ERROR_INVALID_CHANNEL).
 */
int16_t adc_read(uint8_t channel);

#endif /* ADC_H */
