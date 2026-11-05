# Explicação Simples do Sistema da Estufa

Este documento explica de forma simples, direta e sem excesso de termos técnicos como funcionam os três arquivos principais da aplicação da **Mini Estufa Inteligente de Skincare**: `main.c`, `estufa.h` e `estufa.c`.

---

## 🎯 O que o projeto faz no mundo real?

Imagine uma mini-geladeira / estufa inteligente projetada para conservar cosméticos e cremes na temperatura ideal (entre 12°C e 22°C):
- Se a temperatura estiver **ótima**, o ventilador (cooler) fica desligado.
- Se começar a **esquentar**, o ventilador liga e vai aumentando a velocidade aos poucos.
- Se a temperatura ficar **perigosamente alta** (acima de 36°C), um alerta de emergência é disparado e o ventilador entra na potência máxima (100%).

Tudo isso é gerenciado pelo sistema operacional **AdidOS**, que coordena várias tarefas ao mesmo tempo em um único processador.

---

## 1. `main.c` — O Ponto de Partida ("O Maestro")

O `main.c` é o primeiro arquivo executado quando a placa liga. Ele é muito enxuto e serve apenas para preparar a casa e dar a largada:

1. **Configura o hardware e o sistema operacional**: prepara a memória e os relógios do microcontrolador.
2. **Chama a inicialização da estufa (`estufa_init`)**: liga os sensores e o ventilador.
3. **Cria os 4 "trabalhadores" (tarefas) com suas importâncias (prioridades)**:
   - **Monitor A e Monitor B** (Prioridade Média = 5): dois fiscais que revezam a medição da temperatura.
   - **Gerente** (Prioridade Baixa = 1): o operador que lê os relatórios e regula a velocidade da ventoinha.
   - **Alerta Crítico** (Prioridade Alta = 10): o socorrista de emergência que só age se houver superaquecimento.
4. **Dá o "Play" no sistema (`os_start_scheduler`)**: a partir desse momento, o sistema operacional assume o controle e fica revezando as tarefas de forma automática e inteligente.

---

## 2. `estufa.h` — O Manual de Regras ("O Dicionário do Projeto")

O arquivo de cabeçalho (`.h`) não executa tarefas por si só. Ele serve como um contrato ou cardápio que diz para todos os outros arquivos quais são as regras e ferramentas disponíveis:

- **Regras de Temperatura**:
  - `TEMP_MIN_IDEAL (12°C)` e `TEMP_MAX_IDEAL (22°C)`: faixa perfeita para os produtos.
  - `TEMP_CRITICA (36°C)`: limite de risco onde a emergência deve ser acionada.
- **A "Ficha de Dados" (`estufa_status_t`)**: um molde que guarda:
  - Valor medido no sensor.
  - Temperatura calculada em graus Celsius.
  - Velocidade atual da ventoinha (0 a 100%).
  - Qual monitor fez a última leitura (se foi o A ou o B).
  - Se há alarme de emergência ativo.
- **Nomes das Chaves e Filas**:
  - `mutex_status`: uma "chave de cadeado" para garantir que duas tarefas não mexam na ficha de dados ao mesmo tempo.
  - `fila_alertas`: uma caixa de correio onde os monitores depositam avisos para o Gerente.
  - `sem_alerta`: uma campainha que só toca quando a temperatura atinge nível crítico.
- **Lista de Funções**: declara para o compilador as tarefas e a função de inicialização.

---

## 3. `estufa.c` — A Ação Acontece Aqui ("A Fábrica")

Aqui está o código que realmente faz as coisas acontecerem:

### A) Inicialização (`estufa_init`)
- Liga o leitor de sensor (ADC).
- Liga o controle de velocidade da ventoinha (PWM) e a deixa parada (0%).
- Prepara as caixas de correio (filas) e as chaves de segurança (mutex e semáforos).
- Reserva um pedacinho da memória dinâmica para guardar a ficha de dados da estufa.
- Envia uma mensagem inicial na tela do computador (UART) dizendo que a estufa ligou.

### B) Monitores de Temperatura (`TaskMonitorA` e `TaskMonitorB`)
- Funcionam como dois fiscais em revezamento.
- A cada ciclo, um deles lê a temperatura.
- Ele pega a chave do cadeado (`mutex_status`), anota a medição na ficha e devolve a chave.
- Conforme a temperatura, coloca um bilhete na caixa de correio (`fila_alertas`):
  - `'N'`: temperatura normal.
  - `'W'`: morninho (precisa ligar o cooler).
  - `'C'`: superaquecimento crítico!
- Chama uma pausa (`task_delay(30)`), indo dormir por 30 milissegundos para dar vez ao próximo.

### C) O Operador (`TaskGerente`)
- Fica esperando chegar um bilhete na caixa de correio.
- Ao ler o bilhete:
  - Se for **normal**: desliga a ventoinha (0%).
  - Se for **morno**: calcula a velocidade ideal da ventoinha proporcionalmente (quanto mais quente, mais rápido ela gira).
  - Se for **crítico**: pede força máxima (100%).
- Aplica a nova velocidade na ventoinha e imprime na tela do computador o status: monitor responsável, temperatura atual e porcentagem do cooler.

### D) O Socorrista de Emergência (`TaskAlertaCritico`)
- Fica dormindo a maior parte do tempo aguardando a campainha (`sem_alerta`).
- Se a temperatura passar de 36°C, a campainha toca e ele acorda imediatamente (por ter a maior prioridade de todas).
- Passa na frente de todo mundo, trava a ventoinha em **100% de potência** e estampa avisos em letras garrafais na tela.

---

## ⚙️ 4. Como as Tarefas se Revezam no Processador (Ordem de Execução)

Existe uma dúvida muito comum: **o microcontrolador só tem UM processador (um único "cérebro"). Como as 4 tarefas rodam sem se atropelar e quem decide quem roda primeiro?**

### A Diferença entre as duas "Chaves"
Para entender a ordem, precisamos separar dois conceitos:
1. **A Chave do Processador (Tempo de CPU):** quem tem direito de usar o processador agora. Quem dá essa chave é o **Escalonador do Sistema Operacional**. A regra de ouro é: **quem tem maior prioridade e estiver acordado sempre ganha o processador**.
2. **A Chave do Cadeado (`mutex_status`):** é a chave do armário onde fica guardada a ficha de papel com as informações da estufa. Só serve para evitar que duas tarefas tentem escrever ou ler a ficha no exato mesmo microssegundo. Depois de atualizar a ficha, a tarefa devolve a chave imediatamente.

---

### Passo a Passo: O que acontece após o `main()`?

Quando o `main()` termina e liga o sistema (`os_start_scheduler`), as 4 tarefas estão no seguinte estado:

| Tarefa | Prioridade | Estado Inicial | Motivo |
| :--- | :---: | :---: | :--- |
| **`TaskAlertaCritico`** | **10** (Alta) | 🛑 Bloqueada | Está esperando a campainha (`sem_alerta`). Não gasta processador. |
| **`TaskMonitorA`** | **5** (Média) | 🟢 Pronta | Pronta para medir. |
| **`TaskMonitorB`** | **5** (Média) | 🟢 Pronta | Pronta para medir. |
| **`TaskGerente`** | **1** (Baixa) | 🛑 Bloqueada | A caixa de correio começou vazia (`m_queue_read`), então ele dorme esperando bilhete. |

---

### O Ciclo em Ação:

```
[1. Monitor A roda] 
       │ Le a temperatura, guarda na ficha (usa mutex rapidinho),
       │ deposita bilhete na fila e faz task_delay(30) -> vai dormir!
       ▼
[2. Monitor B roda] 
       │ Como A foi dormir, o processador vai pro outro de prioridade 5 (Monitor B).
       │ B mede, guarda na ficha, põe outro bilhete na fila e faz task_delay(30) -> vai dormir!
       ▼
[3. Gerente roda!] 
       │ Agora os dois fiscais (A e B) estao dormindo!
       │ O Gerente (Prioridade 1) finalmente ganha o processador!
       │ Ele consome os bilhetes, pega o mutex, ajusta o cooler e imprime na tela.
       ▼
[4. O Despertador toca (Timer)]
       │ Apos os 30 milissegundos, Monitor A e B acordam.
       │ Como prioridade 5 > 1, eles retomam o processador e o ciclo recomeca!
```

---

### Perguntas Frequentes sobre essa Dinâmica:

#### 1. "O Gerente disputa a chave com o Monitor A e B?"
- **Pelo processador:** Não disputa diretamente por sorteio. O Gerente só ganha a vez de usar o processador quando **tanto o Monitor A quanto o Monitor B vão dormir (`task_delay`)**. Se os fiscais ficassem rodando sem parar (sem `task_delay`), o Gerente nunca conseguiria rodar (o chamado *starvation*).
- **Pelo cadeado (`mutex_status`):** O Gerente só pega o mutex por alguns microssegundos para ler a ficha atualizada e registrar o novo valor do cooler, soltando-o logo em seguida.

#### 2. "E se a estufa pegar fogo enquanto o Gerente estiver rodando?" (Herança de Prioridade - PIP)
- Imagine que o **Gerente** pegou a chave do armário (`mutex_status`).
- De repente, a temperatura bateu 37°C e o **Alerta Crítico (Prioridade 10)** acordou!
- O Alerta Crítico quer a chave do armário, mas ela está na mão do Gerente (Prioridade 1).
- O que o AdidOS faz? Ele usa a **Herança de Prioridade**: temporariamente, ele eleva a prioridade do Gerente para 10. Assim, os Monitores (prioridade 5) não podem atrapalhar o Gerente. O Gerente termina de usar a chave rapidinho, devolve o mutex e o Alerta Crítico assume imediatamente para salvar a estufa!

---

## 📊 Resumo Final dos Arquivos

| Arquivo | Papel Principal | O que tem dentro? |
| :--- | :--- | :--- |
| **`main.c`** | Abertura do sistema | Configura a placa, cadastra as 4 tarefas com suas prioridades e dá a partida no SO. |
| **`estufa.h`** | Regras e definições | Limiares de temperatura, formatos dos bilhetes, declaração das tarefas e variáveis globais. |
| **`estufa.c`** | Funcionamento real | Código das leituras, controle proporcional do cooler, envio de mensagens e telas de alerta. |
