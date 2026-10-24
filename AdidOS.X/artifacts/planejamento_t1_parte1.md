# Planejamento - T1: Primeira Parte (Extensões da API do RTOS)

## 1. O que já foi feito

* **Driver de PWM (Item 6 - parcial):** arquivos [`pwm.h`](file:///home/carol-lanzu/MPLABProjects/embedded-OS/AdidOS.X/pwm.h) e [`pwm.c`](file:///home/carol-lanzu/MPLABProjects/embedded-OS/AdidOS.X/pwm.c) implementados com funções de inicialização, configuração de frequência, início, parada e ajuste de ciclo de trabalho (*duty cycle*).
* **Base do Gerenciador de Memória (Item 4 - base):** arquivos [`mem.h`](file:///home/carol-lanzu/MPLABProjects/embedded-OS/AdidOS.X/mem.h) e [`mem.c`](file:///home/carol-lanzu/MPLABProjects/embedded-OS/AdidOS.X/mem.c) com `os_heap_init`, `os_malloc` (com divisão de blocos) e liberação simples em `os_free`.
* **Esqueletos de funções e definições parciais:**
  * Assinaturas de `mutex_init`, `mutex_lock` e `mutex_unlock` declaradas em [`sync.h`](file:///home/carol-lanzu/MPLABProjects/embedded-OS/AdidOS.X/sync.h), com funções vazias em [`sync.c`](file:///home/carol-lanzu/MPLABProjects/embedded-OS/AdidOS.X/sync.c).
  * Declaração de `prior_scheduler` em [`scheduler.h`](file:///home/carol-lanzu/MPLABProjects/embedded-OS/AdidOS.X/scheduler.h) e função vazia em [`scheduler.c`](file:///home/carol-lanzu/MPLABProjects/embedded-OS/AdidOS.X/scheduler.c).
  * Campo `task_prior` presente na estrutura `tcb_t` em [`types.h`](file:///home/carol-lanzu/MPLABProjects/embedded-OS/AdidOS.X/types.h).
* **Infraestrutura pré-existente do RTOS:** troca de contexto, temporizador do tick, semáforos contadores, fila de mensagens, UART e escalonador Round-Robin funcional.

---

## 2. O que falta fazer

* **Item 1 - Mutex:**
  * Definir os campos da estrutura `mutex_t` em [`types.h`](file:///home/carol-lanzu/MPLABProjects/embedded-OS/AdidOS.X/types.h) (estado/ocupado, tarefa dona, fila de espera).
  * Implementar `mutex_init`, `mutex_lock` (com bloqueio da tarefa se ocupado) e `mutex_unlock` em [`sync.c`](file:///home/carol-lanzu/MPLABProjects/embedded-OS/AdidOS.X/sync.c).
* **Item 2 - Herança de Prioridade:**
  * Adicionar campo para salvar a prioridade original/base na `tcb_t` em [`types.h`](file:///home/carol-lanzu/MPLABProjects/embedded-OS/AdidOS.X/types.h).
  * Implementar elevação temporária da prioridade do dono do mutex quando uma tarefa de maior prioridade for bloqueada no `mutex_lock`.
  * Implementar restauração da prioridade original do dono ao liberar o mutex no `mutex_unlock`.
* **Item 3 - Escalonamento por Prioridade:**
  * Implementar a lógica de busca pela tarefa no estado `READY` de maior prioridade em `prior_scheduler()` dentro de [`scheduler.c`](file:///home/carol-lanzu/MPLABProjects/embedded-OS/AdidOS.X/scheduler.c).
  * Implementar alternância Round-Robin entre tarefas prontas que compartilham a mesma prioridade máxima.
  * Habilitar `DEFAULT_SCHEDULER PRIOR_SCHEDULER` em [`os_config.h`](file:///home/carol-lanzu/MPLABProjects/embedded-OS/AdidOS.X/os_config.h).
* **Item 4 - Gerenciamento de Memória (Coalescência de Blocos Livres):**
  * Implementar a fusão de blocos livres contíguos na função `os_free()` em [`mem.c`](file:///home/carol-lanzu/MPLABProjects/embedded-OS/AdidOS.X/mem.c).
* **Item 5 - Alocação Dinâmica de Estruturas Internas:**
  * Ativar `DYNAMIC_ALLOCATION ON` em [`os_config.h`](file:///home/carol-lanzu/MPLABProjects/embedded-OS/AdidOS.X/os_config.h).
  * Modificar pelo menos uma estrutura interna do RTOS alocada estaticamente para usar alocação dinâmica com `os_malloc`.
* **Item 6 - API de Entrada/Saída (ADC):**
  * Criar os arquivos `adc.h` e `adc.c`.
  * Implementar inicialização, configuração de pinos analógicos e função de leitura/conversão do conversor AD.

> [!NOTE]
> **Pontos não definidos no projeto (decisões em aberto):**
> * **Convenção de prioridade:** Não está explicitado se números maiores indicam maior ou menor prioridade (ex.: prioridade 0 como mais alta ou valor numérico maior como mais alta).
> * **Estrutura interna para alocação dinâmica:** O enunciado permite escolher qual estrutura tornar dinâmica (sugestões: pilha da tarefa, fila de mensagens ou TCBs). Nenhuma foi selecionada ainda.
> * **Configuração do periférico ADC:** O canal analógico padrão (ex.: canal do potenciômetro na placa Explorer 16 / Proteus) e resolução/frequência de amostragem precisam ser definidos conforme o circuito de validação.

---

## 3. Tarefas de Execução

### Bloco A: Gerenciamento de Memória
1. **Adicionar** algoritmo de junção de blocos livres contíguos (*coalescing*) dentro de `os_free()` em [`mem.c`](file:///home/carol-lanzu/MPLABProjects/embedded-OS/AdidOS.X/mem.c).
2. **Habilitar** a diretiva `DYNAMIC_ALLOCATION ON` em [`os_config.h`](file:///home/carol-lanzu/MPLABProjects/embedded-OS/AdidOS.X/os_config.h).
3. **Testar** alocações e liberações sucessivas em `mem.c` para verificar se blocos adjacentes são reunidos sem fragmentar o heap.

### Bloco B: Escalonamento por Prioridade
4. **Definir** o padrão numérico de prioridade no sistema (ex.: número maior = maior prioridade).
5. **Escrever** a função `prior_scheduler()` em [`scheduler.c`](file:///home/carol-lanzu/MPLABProjects/embedded-OS/AdidOS.X/scheduler.c) para selecionar a tarefa `READY` com maior prioridade.
6. **Integrar** o mecanismo Round-Robin para alternar entre tarefas de mesma prioridade dentro de `prior_scheduler()`.
7. **Alterar** a constante `DEFAULT_SCHEDULER` para `PRIOR_SCHEDULER` em [`os_config.h`](file:///home/carol-lanzu/MPLABProjects/embedded-OS/AdidOS.X/os_config.h).
8. **Validar** a alternância de execução entre tarefas com diferentes prioridades e com prioridades iguais.

### Bloco C: Mutex e Herança de Prioridade
9. **Completar** a definição do tipo `mutex_t` em [`types.h`](file:///home/carol-lanzu/MPLABProjects/embedded-OS/AdidOS.X/types.h), incluindo dono atual, estado de trava e fila de tarefas bloqueadas.
10. **Inserir** o campo `task_base_prior` na estrutura `tcb_t` em [`types.h`](file:///home/carol-lanzu/MPLABProjects/embedded-OS/AdidOS.X/types.h) para guardar a prioridade original da tarefa.
11. **Implementar** a função `mutex_init()` em [`sync.c`](file:///home/carol-lanzu/MPLABProjects/embedded-OS/AdidOS.X/sync.c) para zerar os campos da estrutura.
12. **Implementar** a função `mutex_lock()` em [`sync.c`](file:///home/carol-lanzu/MPLABProjects/embedded-OS/AdidOS.X/sync.c), adicionando bloqueio de tarefa e elevação de prioridade do dono se houver disputa por tarefa mais prioritária.
13. **Implementar** a função `mutex_unlock()` em [`sync.c`](file:///home/carol-lanzu/MPLABProjects/embedded-OS/AdidOS.X/sync.c), restaurando a prioridade original da tarefa e liberando a próxima tarefa da fila de espera.
14. **Testar** cenário de inversão de prioridade para verificar a elevação e restauração de prioridade.

### Bloco D: Alocação Dinâmica de Estrutura do RTOS
15. **Escolher** a estrutura interna do RTOS a ser dinamizada (recomendado: alocação dinâmica da pilha da tarefa em `create_task()` ou do buffer da fila de mensagens em `m_queue_init()`).
16. **Substituir** a declaração estática da estrutura escolhida por ponteiro alocado via `os_malloc()`.
17. **Verificar** a correta inicialização e uso da memória alocada sem corrupção de pilha ou dados.

### Bloco E: Driver e API do Conversor AD (ADC)
18. **Criar** o arquivo de cabeçalho `adc.h` com assinaturas de inicialização (`adc_init`) e leitura (`adc_read`).
19. **Criar** o arquivo de implementação `adc.c` configurando os registradores do microcontrolador (AD1CON1, AD1CON2, AD1CON3, AD1PCFG, AD1CHS).
20. **Implementar** a função de leitura bloqueante ou síncrona que dispara a amostragem, aguarda a conversão e retorna o valor digitalizado.
21. **Testar** a leitura analógica no simulador Proteus ou placa com uma tarefa dedicada.
