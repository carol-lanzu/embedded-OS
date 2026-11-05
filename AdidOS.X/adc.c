#include <xc.h>
#include "adc.h"

void adc_init(void)
{
    /* Garante que o modulo esteja desligado durante a escrita dos registradores */
    AD1CON1bits.ADON = 0;

    /* -------------------------------------------------------------------------
     * PASSO 1: Configuracao dos Pinos Analogicos (AD1PCFG e TRISB)
     * -------------------------------------------------------------------------
     * No registrador AD1PCFG:
     *   Bit = 0: Pino configurado em modo ANALOGICO.
     *            O buffer de entrada digital schmitt-trigger e desligado,
     *            evitando consumo de corrente parasita quando a tensao analogica
     *            estiver em niveis intermediarios (entre 0 e VDD).
     *   Bit = 1: Pino configurado em modo DIGITAL (I/O convencional).
     *
     * ADC_PCFG_CONFIG padrao = 0xFFDF (Bit 5 em 0 -> pino AN5/RB5 como analogico).
     */
    AD1PCFG = ADC_PCFG_CONFIG;

    /* Configura o pino fisico correspondente como entrada no registrador TRISB.
     * Todo pino utilizado como entrada analogica deve obrigatoriamente estar
     * configurado como entrada digital (TRIS = 1) para que a saida digital
     * fique em alta impedancia e nao force niveis de tensao sobre o sensor.
     * Os bits com valor 0 em AD1PCFG (entradas analogicas) sao setados em TRISB. */
    TRISB |= (~ADC_PCFG_CONFIG);

    /* -------------------------------------------------------------------------
     * PASSO 2: Selecao de Canal de Entrada (AD1CHS)
     * -------------------------------------------------------------------------
     * AD1CHS controla a conexao das entradas analogicas ao conversor:
     *   - CH0NB = 0: Entrada negativa do MUX B ligada a VREFL (VSS / Terra).
     *   - CH0SB = 0: Entrada positiva do MUX B ligada a AN0 (padrao inicial).
     *   - CH0NA = 0: Entrada negativa do MUX A ligada a VREFL (VSS / Terra).
     *                Configura a medicao como unipolar / single-ended (0V a VDD).
     *   - CH0SA = 0: Entrada positiva do MUX A ligada a AN0 inicialmente.
     *                O canal e atualizado dinamicamente em cada chamada de adc_read().
     */
    AD1CHS = 0x0000;

    /* -------------------------------------------------------------------------
     * PASSO 3: Temporizacao e Clock de Conversao (AD1CON3)
     * -------------------------------------------------------------------------
     * Configura o periodo de clock TAD e o tempo de amostragem automatica:
     *   - ADRC = 0: Clock de conversao derivado do clock de instrucao da CPU (TCY).
     *               Garante sincronismo perfeito com o barramento do processador.
     *               (Se ADRC=1, utilizaria o oscilador RC interno FRC do ADC).
     *   - SAMC = 00000 (0 TAD): Tempo de amostragem automatica desativado,
     *               pois a amostragem e manual por software atraves do bit SAMP.
     *   - ADCS = ADC_ADCS_CONFIG: Divisor de clock.
     *               TAD = TCY * (ADCS + 1)
     *               Para FCY = 4 MHz (TCY = 250 ns), ADCS = 4 gera TAD = 1.25 us.
     *               O valor atende com seguranca a restricao do datasheet
     *               (TAD minimo de 75 ns para o PIC24F/dsPIC).
     */
    AD1CON3 = 0x0000;
    AD1CON3bits.ADRC = 0;               /* Clock de conversao sincronizado com TCY */
    AD1CON3bits.SAMC = 0;               /* Auto-amostragem nao utilizada (modo manual) */
    AD1CON3bits.ADCS = ADC_ADCS_CONFIG; /* TAD = TCY * (ADCS + 1) */

    /* -------------------------------------------------------------------------
     * PASSO 4: Modos de Referencia e MUX (AD1CON2)
     * -------------------------------------------------------------------------
     *   - VCFG = 000: Referencias de tensao internas:
     *                 VREF+ = VDD (3.3V) e VREF- = VSS (0V).
     *                 Nao requer pinos de referencia externos adicionais.
     *   - CSCNA = 0: Nao efetua varredura automatica de entradas (leitura direta).
     *   - BUFS = 0: Somente leitura (status do buffer no modo particionado).
     *   - SMPI = 0000: Incrementa o buffer a cada 1 conversao concluida.
     *   - BUFM = 0: Buffer de saida configurado como 1 array continuo de 16 palavras
     *               (ADC1BUF0 a ADC1BUFF).
     *   - ALTS = 0: Utiliza sempre a configuracao do MUX A (nao alterna com MUX B).
     */
    AD1CON2 = 0x0000;
    AD1CON2bits.VCFG  = 0b000;          /* VREF+ = VDD, VREF- = VSS */
    AD1CON2bits.CSCNA = 0;              /* Desabilita varredura automatica de canais */
    AD1CON2bits.SMPI  = 0b0000;         /* Converte 1 canal por ciclo */
    AD1CON2bits.BUFM  = 0;              /* Buffer em modo unico de 16 palavras */
    AD1CON2bits.ALTS  = 0;              /* Sempre utiliza o MUX A */

    /* -------------------------------------------------------------------------
     * PASSO 5: Modos de Amostragem, Disparo e Saida (AD1CON1)
     * -------------------------------------------------------------------------
     *   - FORM = 00: Formato de saida em numero Inteiro Nao-Assinado (0 a 1023).
     *                Ideal para conversao direta de grandezas proporcionais.
     *   - SSRC = 000: Modo de Disparo Manual de Conversao.
     *                 A amostragem e finalizada e a conversao e disparada
     *                 quando o software limpa o bit SAMP (SAMP = 0).
     *   - ASAM = 0: Amostragem manual iniciada pelo software.
     *               O modulo aguarda que o bit SAMP seja colocado em '1'.
     *   - ADSIDL = 0: O modulo ADC continua operando normalmente durante
     *                 o modo de economia de energia Idle da CPU.
     */
    AD1CON1 = 0x0000;
    AD1CON1bits.FORM   = 0b00;          /* Inteiro nao-assinado (0 a 1023) */
    AD1CON1bits.SSRC   = 0b000;         /* Fim de amostragem manual via bit SAMP */
    AD1CON1bits.ASAM   = 0;             /* Amostragem inicia sob comando (SAMP = 1) */
    AD1CON1bits.ADSIDL = 0;             /* Opera normalmente em modo Idle */

    /* -------------------------------------------------------------------------
     * PASSO 6: Habilitacao do Modulo (AD1CON1bits.ADON = 1)
     * -------------------------------------------------------------------------
     * Liga os circuitos analogicos internos do conversor (amplificadores,
     * comparadores e escada resistiva do DAC SAR). O modulo fica pronto para
     * receber solicitacoes de amostragem e conversao.
     */
    AD1CON1bits.ADON = 1;
}

/**
 * @brief Realiza a leitura sincrona e bloqueante de um canal analogico por polling.
 * @param channel Indice do canal (0 a 15).
 * @return int16_t Valor de 10 bits lido (0 a 1023) ou codigo de erro negativo.
 */
int16_t adc_read(uint8_t channel)
{
    uint32_t timeout;
    volatile uint16_t sample_delay;

    /* 1. Valida se o canal fornecido esta dentro do limite suportado (AN0 a AN15) */
    if (channel > ADC_MAX_CHANNEL) {
        return ADC_ERROR_INVALID_CHANNEL;
    }

    /* 2. Verifica se o modulo ADC esta devidamente energizado e habilitado */
    if (!AD1CON1bits.ADON) {
        return ADC_ERROR_NOT_ENABLED;
    }

    /* 3. Seleciona o canal analogico positivo no multiplexador MUX A (AD1CHS) */
    AD1CHSbits.CH0SA = channel;

    /* 4. Limpa o flag DONE antes de disparar a nova conversao.
     *    Isso impede que uma leitura residual da conversao anterior seja
     *    interpretada como conclusao imediata da nova conversao. */
    AD1CON1bits.DONE = 0;

    /* 5. Inicia a fase de amostragem no circuito analogico:
     *    Ao setar SAMP = 1, a chave interna conecta o pino analogico selecionado
     *    ao capacitor de retencao de carga (Chold). */
    AD1CON1bits.SAMP = 1;

    /* 6. Aguarda o tempo minimo de aquisicao / carga do capacitor (T_samp).
     *    O capacitor precisa atingir o potencial eletrico do sinal de entrada
     *    com erro inferior a 1/2 LSB antes de a conversao ser iniciada. */
    for (sample_delay = 0; sample_delay < ADC_SAMPLE_TIME_CYCLES; sample_delay++) {
        /* Retardo calibrado por software para o tempo de aquisicao */
    }

    /* 7. Dispara a conversao A/D:
     *    Encerra a fase de amostragem limpando o bit SAMP (SAMP = 0).
     *    No modo manual (SSRC = 0b000), a transicao de 1 para 0 em SAMP
     *    desconecta o capacitor do pino e inicia a conversao pelo metodo SAR. */
    AD1CON1bits.SAMP = 0;

    /* 8. Aguarda o termino da conversao por polling no bit DONE.
     *    O hardware seta DONE = 1 assim que a conversao dos 10 bits e finalizada
     *    e o resultado e transferido para o buffer ADC1BUF0.
     *    A protecao de timeout evita travamento em caso de falha de hardware. */
    timeout = ADC_TIMEOUT_LIMIT;
    while (!AD1CON1bits.DONE) {
        if (--timeout == 0) {
            return ADC_ERROR_TIMEOUT; /* Erro: limite de espera excedido */
        }
    }

    /* 9. Retorna o resultado digitalizado presente no registrador de buffer ADC1BUF0.
     *    Como configurado em FORM = 00, o valor e um inteiro de 10 bits (0 a 1023). */
    return (int16_t)ADC1BUF0;
}
