Processamento Paralelo com Threads em C

Este projeto implementa um sistema de processamento paralelo que simula chamadas de API e registra os resultados em um arquivo de log.

Estruturas Principais
shared_t

Estrutura compartilhada entre threads:

ids: array com os IDs a processar

total: total de IDs

index: índice atual do próximo ID

sem_idx: semáforo para proteger o índice

sem_log: semáforo para proteger escrita no log

thread_arg_t

Argumentos passados para cada thread:

shared: ponteiro para shared_t

thread_id: identificador da thread

semd_t

Semáforo binário estilo Dijkstra:

value: 0 ou 1

mutex e cond para controle de acesso

Funções Principais

main.c: cria o processo filho (tpe_child) e aguarda sua finalização

tpe_child.c: cria 5 threads para processar os IDs

worker(): função executada pelas threads

read_ids(): lê os IDs do arquivo lista_ids.txt

api_mock_response(): simula resposta da API

current_timestamp(): retorna data/hora atual


Compilação e Execução
gcc -o main main.c
gcc -pthread -o tpe_child tpe_child.c
seq 1 100 > lista_ids.txt
./main

Requisitos

Sistema Linux

GCC com suporte a POSIX threads
