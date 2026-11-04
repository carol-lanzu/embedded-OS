#include "mem.h" 
#include "os_config.h"
#include "types.h"
#include <stdio.h>


// Alinhado em 2: o PIC24 gera trap ao ler uma word em endereço ímpar
uint8_t heap_mem[HEAP_SIZE] __attribute__((aligned(2)));


void os_heap_init(void)
{
    block_info_t *blk_info = (block_info_t *)heap_mem;

    blk_info->size = HEAP_SIZE - sizeof(block_info_t);
    blk_info->free = 1;
}

void *os_malloc(uint16_t nr_bytes)
{
    uint8_t *heap_ptr   = heap_mem;
    uint8_t *heap_end   = heap_mem + HEAP_SIZE;

    if (nr_bytes == 0)
        return NULL;

    while (heap_ptr + sizeof(block_info_t) <= heap_end) {

        block_info_t *blk_info = (block_info_t *)heap_ptr;

        // Verifica se o bloco é grande o suficiente para a nova alocação
        if (blk_info->free && blk_info->size >= nr_bytes) {

            uint16_t remaining = blk_info->size - nr_bytes;

            // Verifica se é possível criar um novo bloco
            if (remaining > sizeof(block_info_t)) {

                block_info_t *new_blk_info = (block_info_t *)(heap_ptr + sizeof(block_info_t) + nr_bytes);
                new_blk_info->size = remaining - sizeof(block_info_t);
                new_blk_info->free = 1;

                blk_info->size = nr_bytes;
            }

            // Marca o bloco como ocupado
            blk_info->free = 0;

            // Retorna somente a área de dados
            return heap_ptr + sizeof(block_info_t);
        }

        // Passa para o próximo bloco
        heap_ptr += sizeof(block_info_t) + blk_info->size;
    }

    // Não encontrou espaço
    return NULL;
}

void os_free(void *address)
{
    if (address == NULL)
        return;

    block_info_t *blk_info = (block_info_t *)((uint8_t *)address - sizeof(block_info_t));

    blk_info->free = 1;
}
