#include "mem.h"
#include "os_config.h"
#include "types.h"
#include <stdio.h>

/* ------------------------------------------------------------
 * Heap estatico: array de bytes usado como memoria gerenciada.
 * Cada bloco e precedido por um cabecalho block_info_t:
 *
 *  heap_mem
 *  |
 *  v
 *  +------------------+------- ... -------+------------------+--- ...
 *  |  block_info_t    |   dados do bloco  |  block_info_t    |
 *  |  size  | free    |   (size bytes)    |  size  | free    |
 *  +------------------+------- ... -------+------------------+--- ...
 *  ^                                      ^
 *  blk                                    next_blk
 *
 * Invariante: heap_ptr sempre avanca de (sizeof(block_info_t) + blk->size)
 * para chegar ao cabecalho do proximo bloco.
 * ------------------------------------------------------------ */

uint8_t heap_mem[HEAP_SIZE];


/* os_heap_init
 * Cria um unico bloco livre que ocupa todo o heap.
 * Deve ser chamada uma vez antes de qualquer os_malloc/os_free. */
void os_heap_init(void)
{
    block_info_t *blk_info = (block_info_t *)heap_mem;

    blk_info->size = HEAP_SIZE - sizeof(block_info_t);
    blk_info->free = 1;
}


/* os_malloc
 * Algoritmo first-fit: percorre o heap do inicio ao fim e retorna
 * o primeiro bloco livre com tamanho suficiente.
 * Se sobrar espaco apos a alocacao (> sizeof cabecalho), o residuo
 * e transformado em um novo bloco livre (split). */
void *os_malloc(uint16_t nr_bytes)
{
    uint8_t *heap_ptr   = heap_mem;
    uint8_t *heap_end   = heap_mem + HEAP_SIZE;

    if (nr_bytes == 0)
        return NULL;

    while (heap_ptr + sizeof(block_info_t) <= heap_end) {

        block_info_t *blk_info = (block_info_t *)heap_ptr;

        /* Bloco livre e grande o suficiente? */
        if (blk_info->free && blk_info->size >= nr_bytes) {

            uint16_t remaining = blk_info->size - nr_bytes;

            /* Split: so divide se o residuo comportar um novo cabecalho
             * mais pelo menos 1 byte util, evitando blocos degenerados. */
            if (remaining > sizeof(block_info_t)) {

                block_info_t *new_blk = (block_info_t *)(heap_ptr + sizeof(block_info_t) + nr_bytes);
                new_blk->size = remaining - sizeof(block_info_t);
                new_blk->free = 1;

                blk_info->size = nr_bytes;
            }

            blk_info->free = 0;

            /* Retorna ponteiro para a area de dados (apos o cabecalho) */
            return heap_ptr + sizeof(block_info_t);
        }

        /* Avanca para o proximo bloco */
        heap_ptr += sizeof(block_info_t) + blk_info->size;
    }

    /* Sem espaco disponivel */
    return NULL;
}


/* os_free
 * Marca o bloco como livre e aplica coalescing (fusao) com os vizinhos
 * contiguos que tambem estejam livres, eliminando fragmentacao.
 *
 * Estrategia de fusao em dois passos:
 *   1. Fusao com o bloco POSTERIOR (forward merge):
 *      Calculamos o endereco do proximo bloco. Se ele estiver dentro
 *      do heap e for livre, absorvemos seu tamanho (cabecalho + dados).
 *
 *   2. Fusao com o bloco ANTERIOR (backward merge):
 *      Percorremos o heap desde o inicio ate encontrar o bloco cujo
 *      endereco de fim e exatamente o inicio do bloco atual. Se esse
 *      anterior for livre, ele absorve o bloco atual.
 *
 * Protecao de fronteira:
 *   - A verificacao (next_addr + sizeof(block_info_t) <= heap_end)
 *     garante que ha espaco suficiente para ler o cabecalho de next_blk
 *     antes de desreferenciar o ponteiro.
 *   - No varredura do anterior, a condicao (scan == blk) interrompe
 *     o loop antes de ultrapassar o proprio bloco.
 *   - O ponteiro de entrada e validado contra os limites do heap
 *     antes de qualquer operacao.                                        */
void os_free(void *address)
{
    if (address == NULL)
        return;

    uint8_t *heap_end = heap_mem + HEAP_SIZE;

    /* Recupera o cabecalho do bloco a partir do ponteiro de dados */
    block_info_t *blk = (block_info_t *)((uint8_t *)address - sizeof(block_info_t));

    /* Sanidade: cabecalho deve estar dentro do heap */
    if ((uint8_t *)blk < heap_mem || (uint8_t *)blk >= heap_end)
        return;

    blk->free = 1;

    /* ----------------------------------------------------------
     * Passo 1 - Fusao com o bloco POSTERIOR
     * next_addr e o primeiro byte apos os dados do bloco atual.
     * Verificamos se cabem pelo menos sizeof(block_info_t) bytes
     * a partir desse ponto antes de ler o cabecalho do proximo.
     * ---------------------------------------------------------- */
    uint8_t *next_addr = (uint8_t *)blk + sizeof(block_info_t) + blk->size;

    if (next_addr + sizeof(block_info_t) <= heap_end) {
        block_info_t *next_blk = (block_info_t *)next_addr;

        if (next_blk->free) {
            /* Absorve cabecalho + dados do bloco posterior */
            blk->size += sizeof(block_info_t) + next_blk->size;
        }
    }

    /* ----------------------------------------------------------
     * Passo 2 - Fusao com o bloco ANTERIOR
     * Varredura linear desde o inicio do heap para localizar o
     * bloco cuja area de dados termina exatamente em blk.
     * ---------------------------------------------------------- */
    uint8_t *scan = heap_mem;

    while (scan + sizeof(block_info_t) <= heap_end) {
        block_info_t *prev     = (block_info_t *)scan;
        //scan é o endereço onde o bloco atual (prev) começa
        uint8_t      *prev_end = scan + sizeof(block_info_t) + prev->size;

        /* prev e o bloco imediatamente anterior ao atual? */
        if (prev_end == (uint8_t *)blk) {
            if (prev->free) {
                prev->size += sizeof(block_info_t) + blk->size;
            }
            break;
        }

        /* Chegamos ao proprio bloco: nao ha anterior */
        if (prev_end > (uint8_t *)blk)
            break;

        scan = prev_end;
    }
}


/* ============================================================
 * TESTES DE COALESCENCIA
 * Compilar com -DMEM_TEST para ativar.
 *
 * Cenario principal: alocar A, B, C (64 bytes cada); liberar
 * B, depois A, depois C.
 *
 * Resultado esperado passo a passo:
 *   Inicial        : [LIVRE  1017B]
 *   Apos A,B,C     : [OCP 64][OCP 64][OCP 64][LIVRE residuo]
 *   Apos free(B)   : [OCP 64][LIVRE 64][OCP 64][LIVRE residuo]
 *   Apos free(A)   : [LIVRE A+hdr+B=131B][OCP 64][LIVRE residuo]
 *   Apos free(C)   : [LIVRE heap inteiro = 1017B]
 * ============================================================ */
#ifdef MEM_TEST

static void print_heap_map(void)
{
    uint8_t *ptr     = heap_mem;
    uint8_t *end     = heap_mem + HEAP_SIZE;
    int      blk_idx = 0;

    printf("\n=== Mapa do Heap ===\n");
    while (ptr + sizeof(block_info_t) <= end) {
        block_info_t *b = (block_info_t *)ptr;
        printf("  [%d] offset=%4u  size=%4u  %s\n",
               blk_idx,
               (unsigned)(ptr - heap_mem),
               b->size,
               b->free ? "LIVRE" : "OCUPADO");
        ptr += sizeof(block_info_t) + b->size;
        blk_idx++;
    }
    printf("====================\n");
}

void mem_test_coalescing(void)
{
    printf("\n*** Teste de Coalescencia ***\n");

    os_heap_init();
    printf("\n[Inicial]");
    print_heap_map();

    void *A = os_malloc(64);
    void *B = os_malloc(64);
    void *C = os_malloc(64);
    printf("\n[Apos alocar A=64, B=64, C=64]");
    print_heap_map();

    os_free(B);
    printf("\n[Apos free(B) - posterior ainda ocupado, B fica isolado]");
    print_heap_map();

    os_free(A);
    printf("\n[Apos free(A) - A funde com B posterior -> bloco A+B]");
    print_heap_map();

    os_free(C);
    printf("\n[Apos free(C) - C funde com A+B anterior -> heap inteiro]");
    print_heap_map();

    /* Validacao automatica */
    block_info_t *b        = (block_info_t *)heap_mem;
    uint16_t      expected = HEAP_SIZE - sizeof(block_info_t);

    if (b->free && b->size == expected)
        printf("\n[PASS] Coalescencia completa: 1 bloco livre, size=%u\n", b->size);
    else
        printf("\n[FAIL] size=%u free=%u (esperado size=%u free=1)\n",
               b->size, b->free, expected);
}

#endif /* MEM_TEST */
