#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }
    if (pid == 0) {
        /* Filho: executa o programa tpe_child (P1) */
        execl("./tpe_child", "./tpe_child", (char*)NULL);
        perror("execl failed");
        _exit(2);
    } else {
        /* Pai: espera o filho terminar */
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid");
            return 3;
        }
        if (WIFEXITED(status)) {
            int code = WEXITSTATUS(status);
            if (code == 0) {
                printf("P1 finalizou com sucesso (codigo 0).\n");
            } else {
                printf("P1 finalizou com codigo %d\n", code);
            }
        } else if (WIFSIGNALED(status)) {
            printf("P1 foi terminado por sinal %d\n", WTERMSIG(status));
        } else {
            printf("P1 finalizou de forma inesperada.\n");
        }
    }
    return 0;
}
