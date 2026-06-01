#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 2. REQUISITOS DE ARQUITETURA
#define MAX_PROCESSES 10 
#define CMD_LEN 256
#define CICLOS_PADRAO 4 // Se o usuário não informar o peso, assume 4 ticks

// Estados exigidos pelo requisito
typedef enum { PRONTO, EXECUTANDO, BLOQUEADO_SEMAFORO, TERMINADO } Estado;

// Bloco Descritor de Processo (PCB)
typedef struct {
    int pid;
    char nome[50];
    Estado estado;
    int ciclos_restantes;
    
    // Auxiliar para o Desafio Hardcore (Deadlock)
    int recurso_esperado; 
} PCB;

// Protótipos
void cmd_spawn(char* nome);
void cmd_ps();
void cmd_kill(int pid);
void cmd_lock(int pid, int recurso);
void cmd_unlock(int pid, int recurso);
void cmd_cpu();
void verificar_deadlock();
int encontrar_processo(int pid);
const char* estado_para_string(Estado e);

// Memória RAM Simulada
PCB tabela_processos[MAX_PROCESSES];
int next_pid = 1;

// 4. O NÚCLEO: Variável para gerenciar a Alternância Circular (Round Robin)
int ultimo_executado = -1;

// 5. DESAFIO HARDCORE: Semáforos / Mutex
int impressora_lock = 0; // 0 = Livre, PID = Ocupado por aquele processo
int disco_lock = 0;

// --- FUNÇÕES AUXILIARES ---
const char* estado_para_string(Estado e) {
    switch(e) {
        case PRONTO: return "PRONTO";
        case EXECUTANDO: return "EXECUTANDO";
        case BLOQUEADO_SEMAFORO: return "BLOQUEADO";
        case TERMINADO: return "TERMINADO";
        default: return "DESCONHECIDO";
    }
}

int encontrar_processo(int pid) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (tabela_processos[i].pid == pid) return i;
    }
    return -1;
}

// --- COMANDOS DO SHELL ---

// Requisito 3: spawn <nome>
void cmd_spawn(char* nome) {
    int slot_livre = -1;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (tabela_processos[i].pid == 0) {
            slot_livre = i;
            break;
        }
    }

    if (slot_livre == -1) {
        printf("[Erro] Out of Memory (OOM)! Tabela de Processos cheia.\n");
        return;
    }

    tabela_processos[slot_livre].pid = next_pid++;
    strcpy(tabela_processos[slot_livre].nome, nome);
    tabela_processos[slot_livre].estado = PRONTO;
    tabela_processos[slot_livre].ciclos_restantes = CICLOS_PADRAO;
    tabela_processos[slot_livre].recurso_esperado = 0;

    printf("[Sistema] PCB instanciado: '%s' (PID %d) carregado na RAM.\n", nome, tabela_processos[slot_livre].pid);
}

// Requisito 3: ps
void cmd_ps() {
    printf("\nPID\tNOME\t\tESTADO\t\tCICLOS\tRECURSOS\n");
    printf("----------------------------------------------------------\n");
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (tabela_processos[i].pid != 0) {
            char rec[30] = "-";
            if (tabela_processos[i].recurso_esperado == 1) strcpy(rec, "Espera Impressora");
            if (tabela_processos[i].recurso_esperado == 2) strcpy(rec, "Espera Disco");

            printf("%d\t%-10s\t%-10s\t%d\t%s\n", 
                tabela_processos[i].pid, 
                tabela_processos[i].nome, 
                estado_para_string(tabela_processos[i].estado), 
                tabela_processos[i].ciclos_restantes, rec);
        }
    }
    printf("Mutex Ativos -> Impressora: [%d] | Disco: [%d] (0 = Livre)\n\n", impressora_lock, disco_lock);
}

// Requisito 3: kill <pid>
// Requisito 3: kill <pid> (Versão Refinada com Segurança de Mutex)
void cmd_kill(int pid) {
    int idx = encontrar_processo(pid);
    if (idx != -1) {
        // Correção: Aciona o unlock adequado para repassar o recurso antes de morrer
        if (impressora_lock == pid) cmd_unlock(pid, 1);
        if (disco_lock == pid) cmd_unlock(pid, 2);
        
        tabela_processos[idx].pid = 0; // Remoção Lógica
        printf("[Sistema] Processo PID %d removido logicamente da Tabela.\n", pid);
    } else {
        printf("[Erro] PID nao encontrado.\n");
    }
}

// Requisito 5: Detector de Deadlock
void verificar_deadlock() {
    if (impressora_lock != 0 && disco_lock != 0 && impressora_lock != disco_lock) {
        int id_imp = encontrar_processo(impressora_lock);
        int id_dsc = encontrar_processo(disco_lock);
        
        if (id_imp != -1 && id_dsc != -1) {
            if (tabela_processos[id_imp].recurso_esperado == 2 && tabela_processos[id_dsc].recurso_esperado == 1) {
                printf("\n======================================================\n");
                printf("  [KERNEL PANIC] SCENARIO DE DEADLOCK IRREVERSIVEL!   \n");
                printf("======================================================\n");
                printf("  PID %d segura a Impressora e aguarda o Disco.\n", impressora_lock);
                printf("  PID %d segura o Disco e aguarda a Impressora.\n", disco_lock);
                printf("  A CPU continuara girando, mas estes processos travaram.\n");
                printf("======================================================\n\n");
            }
        }
    }
}

// Requisito 5: lock e unlock
void cmd_lock(int pid, int recurso) {
    int idx = encontrar_processo(pid);
    if (idx == -1 || (recurso != 1 && recurso != 2)) return;

    int *mutex_alvo = (recurso == 1) ? &impressora_lock : &disco_lock;
    char *nome_rec = (recurso == 1) ? "Impressora" : "Disco";

    if (*mutex_alvo == 0) { // Livre
        *mutex_alvo = pid;
        printf("[Mutex] PID %d adquiriu acesso exclusivo: %s.\n", pid, nome_rec);
    } else if (*mutex_alvo != pid) { // Ocupado
        tabela_processos[idx].estado = BLOQUEADO_SEMAFORO;
        tabela_processos[idx].recurso_esperado = recurso;
        printf("[Mutex] %s em uso por PID %d. PID %d foi BLOQUEADO.\n", nome_rec, *mutex_alvo, pid);
        verificar_deadlock();
    }
}

void cmd_unlock(int pid, int recurso) {
    int idx = encontrar_processo(pid);
    if (idx == -1 || (recurso != 1 && recurso != 2)) return;

    int *mutex_alvo = (recurso == 1) ? &impressora_lock : &disco_lock;
    char *nome_rec = (recurso == 1) ? "Impressora" : "Disco";

    if (*mutex_alvo == pid) {
        *mutex_alvo = 0;
        printf("[Mutex] PID %d liberou: %s.\n", pid, nome_rec);
        
        // Acorda os processos da fila (FIFO simples)
        for (int i = 0; i < MAX_PROCESSES; i++) {
            if (tabela_processos[i].pid != 0 && tabela_processos[i].estado == BLOQUEADO_SEMAFORO && tabela_processos[i].recurso_esperado == recurso) {
                tabela_processos[i].estado = PRONTO;
                tabela_processos[i].recurso_esperado = 0;
                *mutex_alvo = tabela_processos[i].pid;
                printf("[Mutex] %s repassado para PID %d (Agora PRONTO).\n", nome_rec, tabela_processos[i].pid);
                break;
            }
        }
    }
}

// Requisito 4: cpu (Escalonador Round Robin)
void cmd_cpu() {
    int index_escolhido = -1;

    // Passo 1: Limpeza de TERMINADOS e ajuste de EXECUTANDO
    for(int i = 0; i < MAX_PROCESSES; i++) {
        if(tabela_processos[i].pid != 0) {
            if(tabela_processos[i].estado == TERMINADO) {
                printf("[Sistema] Desvinculando processo TERMINADO da RAM: PID %d\n", tabela_processos[i].pid);
                tabela_processos[i].pid = 0; 
                
                // Libera recursos esquecidos (Safety net)
                if (impressora_lock == tabela_processos[i].pid) cmd_unlock(tabela_processos[i].pid, 1);
                if (disco_lock == tabela_processos[i].pid) cmd_unlock(tabela_processos[i].pid, 2);
            } 
            else if (tabela_processos[i].estado == EXECUTANDO) {
                // Se alguém estava executando e não terminou, volta pro fim da fila lógica (PRONTO)
                tabela_processos[i].estado = PRONTO;
            }
        }
    }

    // Passo 2: Busca Alternância Circular (Round Robin)
    for (int i = 1; i <= MAX_PROCESSES; i++) {
        int idx = (ultimo_executado + i) % MAX_PROCESSES;
        // Ignora sumariamente quem está BLOQUEADO_SEMAFORO (Requisito 5)
        if (tabela_processos[idx].pid != 0 && tabela_processos[idx].estado == PRONTO) {
            index_escolhido = idx;
            break;
        }
    }

    // Passo 3: Executa o "tick" no processo escolhido
    if (index_escolhido != -1) {
        ultimo_executado = index_escolhido;
        tabela_processos[index_escolhido].estado = EXECUTANDO;
        tabela_processos[index_escolhido].ciclos_restantes--;

        printf("[CPU] Tick! Executando PID %d (%s). Ciclos restantes: %d\n", 
               tabela_processos[index_escolhido].pid, 
               tabela_processos[index_escolhido].nome, 
               tabela_processos[index_escolhido].ciclos_restantes);

        if (tabela_processos[index_escolhido].ciclos_restantes <= 0) {
            tabela_processos[index_escolhido].estado = TERMINADO;
            printf("[Sistema] PID %d zerou seus ciclos e mudou para TERMINADO.\n", tabela_processos[index_escolhido].pid);
        }
    } else {
        printf("[CPU] Tick Ocioso. Nenhum processo em estado PRONTO.\n");
    }
}

// Requisito 3: Laço de Repetição Principal (Shell)
int main() {
    char input[CMD_LEN];
    memset(tabela_processos, 0, sizeof(tabela_processos)); // Inicializa RAM com zeros

    printf("========================================\n");
    printf("       trianguleOS v3 - RR Engine       \n");
    printf("========================================\n");

    while (1) {
        printf("shell> ");
        
        // Leitura via entrada padrão <string.h>
        if (fgets(input, CMD_LEN, stdin) == NULL) break;
        input[strcspn(input, "\n")] = 0; 
        if (strlen(input) == 0) continue;

        char *cmd = strtok(input, " ");
        if (cmd == NULL) continue;

        if (strcmp(cmd, "exit") == 0) {
            printf("Limpando memorias e encerrando o simulador...\n");
            break;
        } 
        else if (strcmp(cmd, "spawn") == 0) {
            char *nome = strtok(NULL, " ");
            if (nome) cmd_spawn(nome);
            else printf("Uso correto: spawn <nome>\n");
        }

        else if (strcmp(cmd, "help") == 0) {
            printf("Comandos disponiveis:\n");
            printf("  spawn <nome> - Cria um novo processo\n");
            printf("  ps - Lista todos os processos\n");
            printf("  kill <pid> - Encerra um processo\n");
            printf("  cpu - Exibe informacoes sobre a CPU\n");
            printf("  lock <pid> <1(Imp) ou 2(Disco)> - Bloqueia um recurso\n");
            printf("  unlock <pid> <1(Imp) ou 2(Disco)> - Desbloqueia um recurso\n");
            printf("  help - Exibe esta mensagem de ajuda\n");
            printf("  exit - Encerra o simulador\n");
        }

        else if (strcmp(cmd, "ps") == 0) {
            cmd_ps();
        }
        else if (strcmp(cmd, "cpu") == 0) {
            cmd_cpu();
        }
        else if (strcmp(cmd, "kill") == 0) {
            char *pid = strtok(NULL, " ");
            if (pid) cmd_kill(atoi(pid));
        }
        else if (strcmp(cmd, "lock") == 0) {
            char *pid = strtok(NULL, " ");
            char *recurso = strtok(NULL, " ");
            if (pid && recurso) cmd_lock(atoi(pid), atoi(recurso));
            else printf("Uso: lock <pid> <1(Imp) ou 2(Disco)>\n");
        }
        else if (strcmp(cmd, "unlock") == 0) {
            char *pid = strtok(NULL, " ");
            char *recurso = strtok(NULL, " ");
            if (pid && recurso) cmd_unlock(atoi(pid), atoi(recurso));
        }
        else {
            printf("Comando desconhecido. Comandos validos: spawn, ps, kill, cpu, lock, unlock, exit.\n");
        }
    }
    return 0;
}