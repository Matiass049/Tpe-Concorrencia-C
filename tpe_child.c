#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

#define THREAD_COUNT 5

/* --- Semáforo binário estilo Dijkstra (semd) --- */
typedef struct {
    int value; /* 0 ou 1 (binary) but generalized */
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} semd_t;

int semd_init(semd_t *s, int initval) {
    s->value = initval;
    if (pthread_mutex_init(&s->mutex, NULL) != 0) return -1;
    if (pthread_cond_init(&s->cond, NULL) != 0) {
        pthread_mutex_destroy(&s->mutex);
        return -1;
    }
    return 0;
}

int semd_P(semd_t *s) {
    pthread_mutex_lock(&s->mutex);
    while (s->value <= 0) {
        pthread_cond_wait(&s->cond, &s->mutex);
    }
    s->value--;
    pthread_mutex_unlock(&s->mutex);
    return 0;
}

int semd_V(semd_t *s) {
    pthread_mutex_lock(&s->mutex);
    s->value++;
    pthread_cond_signal(&s->cond);
    pthread_mutex_unlock(&s->mutex);
    return 0;
}

int semd_destroy(semd_t *s) {
    pthread_cond_destroy(&s->cond);
    pthread_mutex_destroy(&s->mutex);
    return 0;
}

/* --- util: timestamp --- */
char *current_timestamp() {
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    char *buf = malloc(32);
    if (!buf) return NULL;
    snprintf(buf, 32, "%04d-%02d-%02d %02d:%02d:%02d",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec);
    return buf;
}

/* --- mock da API (fixo) --- */
char *api_mock_response(long id) {
    /* retorna string malloc'd */
    char tmp[128];
    double valor = 987.65;
    int n = snprintf(tmp, sizeof(tmp), "{\"id\":%ld,\"status\":\"ok\",\"valor\":%.2f}", id, valor);
    char *res = malloc(n + 1);
    if (!res) return NULL;
    strcpy(res, tmp);
    return res;
}

/* --- Dados compartilhados entre threads --- */
typedef struct {
    long *ids;         /* array de ids */
    size_t total;      /* total de ids */
    size_t index;      /* índice atual (próximo id a processar) */
    semd_t sem_idx;    /* semáforo para proteger acesso ao index */
    semd_t sem_log;    /* semáforo para proteger escrita no log */
    int thread_num;    /* número da thread (para passagem) */
} shared_t;

typedef struct {
    shared_t *shared;
    int thread_id; /* 1..THREAD_COUNT */
} thread_arg_t;

/* Worker thread */
void *worker(void *arg) {
    thread_arg_t *targ = (thread_arg_t*)arg;
    shared_t *sh = targ->shared;
    int tid = targ->thread_id;

    while (1) {
        long id = -1;

        /* P() para pegar índice */
        semd_P(&sh->sem_idx);
        if (sh->index >= sh->total) {
            /* nada mais a processar */
            semd_V(&sh->sem_idx);
            break;
        }
        id = sh->ids[sh->index];
        sh->index++;
        semd_V(&sh->sem_idx);

        /* Simula chamada à API */
        char *json = api_mock_response(id);
        if (!json) {
            /* erro na mock: apenas continue */
            continue;
        }

        /* Protege escrita no arquivo de log */
        semd_P(&sh->sem_log);
        FILE *f = fopen("logs.txt", "a");
        if (f) {
            char *ts = current_timestamp();
            if (!ts) ts = strdup("0000-00-00 00:00:00");
            /* formato com vírgulas conforme pedido */
            fprintf(f, "%s, Thread %d, ID %ld, %s\n", ts, tid, id, json);
            free(ts);
            fclose(f);
        } else {
            /* se falhar, escrevemos perror (mas sem travar) */
            perror("fopen logs.txt");
        }
        semd_V(&sh->sem_log);

        free(json);

        /* opcional: pequena pausa para simular latência (descomente se quiser) */
        /* usleep(1000); */
    }

    return NULL;
}

/* Lê lista_ids.txt e retorna array allocated e sets total.
 * Caller must free(*out_ids).
 */
int read_ids(const char *filename, long **out_ids, size_t *out_total) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        perror("fopen lista_ids.txt");
        return -1;
    }
    size_t cap = 1024;
    size_t n = 0;
    long *arr = malloc(sizeof(long) * cap);
    if (!arr) {
        fclose(f);
        perror("malloc");
        return -1;
    }
    while (1) {
        long id;
        int r = fscanf(f, "%ld", &id);
        if (r == 1) {
            if (n >= cap) {
                cap *= 2;
                long *tmp = realloc(arr, sizeof(long) * cap);
                if (!tmp) { free(arr); fclose(f); perror("realloc"); return -1; }
                arr = tmp;
            }
            arr[n++] = id;
        } else if (r == EOF) {
            break;
        } else { /* r == 0: bad format */
            /* skip token */
            if (fscanf(f, "%*s") == EOF) break;
        }
    }
    fclose(f);
    *out_ids = arr;
    *out_total = n;
    return 0;
}

int main(int argc, char *argv[]) {
    /* Lê lista_ids.txt (deve estar no mesmo diretório) */
    long *ids = NULL;
    size_t total = 0;
    if (read_ids("lista_ids.txt", &ids, &total) != 0) {
        fprintf(stderr, "Erro ao ler lista_ids.txt. Crie o arquivo com: seq 1 100 > lista_ids.txt (exemplo)\n");
        return 2;
    }

    /* inicializa compartilhados */
    shared_t shared;
    shared.ids = ids;
    shared.total = total;
    shared.index = 0;
    if (semd_init(&shared.sem_idx, 1) != 0) {
        fprintf(stderr, "Erro inicializar sem_idx\n");
        free(ids);
        return 3;
    }
    if (semd_init(&shared.sem_log, 1) != 0) {
        fprintf(stderr, "Erro inicializar sem_log\n");
        semd_destroy(&shared.sem_idx);
        free(ids);
        return 4;
    }

    /* remove logs.txt anterior para começar limpo */
    remove("logs.txt");

    /* cria threads */
    pthread_t threads[THREAD_COUNT];
    thread_arg_t args[THREAD_COUNT];

    for (int i = 0; i < THREAD_COUNT; ++i) {
        args[i].shared = &shared;
        args[i].thread_id = i + 1;
        if (pthread_create(&threads[i], NULL, worker, &args[i]) != 0) {
            perror("pthread_create");
            /* join os já criados, cleanup e sair */
            for (int j = 0; j < i; ++j) pthread_join(threads[j], NULL);
            semd_destroy(&shared.sem_idx);
            semd_destroy(&shared.sem_log);
            free(ids);
            return 5;
        }
    }

    /* espera as threads terminarem */
    for (int i = 0; i < THREAD_COUNT; ++i) {
        pthread_join(threads[i], NULL);
    }

    /* cleanup */
    semd_destroy(&shared.sem_idx);
    semd_destroy(&shared.sem_log);
    free(ids);

    return 0;
}
