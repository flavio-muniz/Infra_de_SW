#include <stdio.h>
#include <unistd.h>
#include<string.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<pthread.h>
#include<fcntl.h>

char *history = NULL;
#define MAX_BACKGROUND_PROCESSES 1000000

pid_t background_processes[MAX_BACKGROUND_PROCESSES];
int num_background_processes = 0;

//fork() - OK
//exec() - OK
//wait() - OK
//dup2() - OK
//pipe() - OK
//pthread_create() - OK
//pthread_join() - OK

struct thread_node {
    pthread_t thread_id;
    struct thread_node* next;
};

// Função executada por cada thread para executar um comando
int execute_parallel_command(char *command);
// Função para executar um comando no modo sequencial
int execute_sequential_command(char *command);
// Função para executar comandos com pipes
int execute_piped_command(char *command);
// executa 
void execute_command(char *command);
//background
int execute_background(const char *command);
void *thread_function(void *arg);
void insertAtBeginning(struct thread_node** head, pthread_t newThreadId);

int main(int argc, char *argv[]){
    

    int should_run = 1; /* flag para determinar quando encerrar o programa */
    int parallel_mode = 0; // 0 para sequencial, 1 para paralelo (estilo padrão é sequencial)

    if (argc==1){ //modo interativo
        while (should_run){
            printf("frssm %s> ", parallel_mode ? "par" : "seq");
            fflush(stdout);

            char *input = NULL;
            size_t input_size = 0;

            if (getline(&input, &input_size, stdin) == -1) {
                perror("Erro na leitura da entrada");
                break;
            }
            if (input != NULL) {
                if(strcmp(input,"!!")!=0){
                    if (history != NULL) {
                        free(history); // Liberar a memória antiga, se houver
                    }
                    history = (char *)malloc((strlen(input) + 1) * sizeof(char));
                    if (history == NULL) {
                        fprintf(stderr, "Erro ao alocar memória.\n");
                        exit(1);
                    }
                    strcpy(history, input);
                }
            }

            size_t input_length = strlen(input);
            if (input_length > 0 && input[input_length - 1] == '\n') {
                input[input_length - 1] = '\0';
            }
            strcpy(history,input);

            if (strcmp(input, "style sequential") == 0) {//cada ação deve ocorrer uma por vez, da esquerda para a direita, num mesmo processo filho.
                parallel_mode = 0;
                continue;
            } else if (strcmp(input, "style parallel") == 0) {//todas ações devem ser executadas em paralelo, nesse caso uma nova thread deve ser criada para cada comando. Não há limites para o número de comando por linha.
                parallel_mode = 1;
                continue;
            }

            if (strcmp(input, "exit") == 0) {
                should_run = 0; // Define should_run como 0 para sair do loop
            } else if (parallel_mode==0) {
                // Execute o comando no modo sequencial
                int status = execute_sequential_command(input);
                // Trate o status conforme necessário
            } else {
                // Execute o comando em paralelo
                int status = execute_parallel_command(input);
                // Trate o status conforme necessário
            }
        }
        
    }else if(argc==2){ // Modo batch: Um arquivo de lote foi fornecido como argumento
        FILE *batch_file = fopen(argv[1], "r");
        if (batch_file == NULL) {
            perror("Erro ao abrir o arquivo de lote");
            return 1;
        }
        char *line = NULL;
        size_t line_size = 0;
        ssize_t line_length;

        while ((line_length = getline(&line, &line_size, batch_file)) != -1) {
            
            // Remova a quebra de linha do final da linha lida
            if (line_length > 0 && line[line_length - 1] == '\n') {
                line[line_length - 1] = '\0';
            }

            if (getline(&line, &line_size, stdin) == -1) {
                perror("Erro na leitura da entrada");
                break;
            }

            if (line != NULL) {
                if (history != NULL) {
                    free(history); // Liberar a memória antiga, se houver
                }
                history = (char *)malloc((strlen(line) + 1) * sizeof(char));

                if (history == NULL) {
                    fprintf(stderr, "Erro ao alocar memória.\n");
                    exit(1);
                }

                strcpy(history, line);
            }

            // Verifique se a linha contém um comando de estilo
            if (strcmp(line, "style sequential") == 0) {
                parallel_mode = 0;
                continue; // Não processe este comando como um comando normal
            } else if (strcmp(line, "style parallel") == 0) {
                parallel_mode = 1;
                continue; // Não processe este comando como um comando normal
            }
            
            char *token = strtok(line, ";");
            while (token != NULL) {
                if (token != NULL) {
                    size_t token_length = strlen(token);
                    if (token_length > 0 && token[token_length - 1] == '\n') {
                        token[token_length - 1] = '\0';
                    }

                    if (strcmp(token, "exit") == 0) {
                        should_run = 0;
                        break;
                    } else if (!parallel_mode) {
                        int status = execute_sequential_command(token);
                    } else {
                        int status = execute_parallel_command(token);
                    }
                }
                token = strtok(NULL, ";");
            }
            if (strcmp(line, "exit") == 0) {
                should_run = 0; // Define should_run como 0 para sair do loop
            }
        }
        free(line); // Libera a memória alocada para a linha lida
        fclose(batch_file);
    } else {
        fprintf(stderr, "Uso incorreto: número incorreto de argumentos para o comando %s [batchFile]\n", argv[0]);
        return 1;
    }
        
    return 0;
}

// Função para executar comandos com redirecionamento de entrada e saída
int execute_redirect_command(char *command) {
    char *token;
    char *saveptr;
    char *input_file = NULL;
    char *output_file = NULL;
    int append = 0; // Indica se o redirecionamento de saída é de adição (>>)

    // Verifique se o comando contém redirecionamento de entrada ou saída
    if (strchr(command, '<') != NULL || strchr(command, '>') != NULL || strstr(command, ">>") != NULL) {
        // Use strtok_r para dividir a entrada em comandos com base em redirecionamento
        token = strtok_r(command, "<>", &saveptr);

        while (token != NULL) {
            if (strlen(token) > 0 && strspn(token, " \t\n\r\f\v") != strlen(token)) {
                if (token[0] == '<') {
                    // Redirecionamento de entrada
                    input_file = token + 1; // Nome do arquivo de entrada
                } else if (token[0] == '>') {
                    // Redirecionamento de saída
                    if (token[1] == '>') {
                        // Redirecionamento de adição (>>)
                        append = 1;
                        output_file = token + 2; // Nome do arquivo de saída para adição
                    } else {
                        // Redirecionamento de saída (>)
                        output_file = token + 1; // Nome do arquivo de saída
                    }
                }
            }
            token = strtok_r(NULL, "<>", &saveptr);
        }
    }

    // Verifique se há redirecionamento de entrada
    if (input_file != NULL) {
        // Abra o arquivo de entrada para leitura
        int fd_input = open(input_file, O_RDONLY);
        if (fd_input == -1) {
            perror("Erro ao abrir o arquivo de entrada");
            return -1;
        }

        // Redirecione a entrada padrão (STDIN) para o arquivo de entrada
        if (dup2(fd_input, STDIN_FILENO) == -1) {
            perror("Erro ao redirecionar a entrada padrão");
            close(fd_input);
            return -1;
        }

        close(fd_input); // Feche o arquivo de entrada após o redirecionamento
    }

    // Verifique se há redirecionamento de saída
    if (output_file != NULL) {
        // Abra o arquivo de saída para escrita
        int fd_output;
        if (append) {
            // Abra o arquivo de saída para adição (>>)
            fd_output = open(output_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
        } else {
            // Abra o arquivo de saída (>)
            fd_output = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        }

        if (fd_output == -1) {
            perror("Erro ao abrir o arquivo de saída");
            return -1;
        }

        // Redirecione a saída padrão (STDOUT) para o arquivo de saída
        if (dup2(fd_output, STDOUT_FILENO) == -1) {
            perror("Erro ao redirecionar a saída padrão");
            close(fd_output);
            return -1;
        }

        close(fd_output); // Feche o arquivo de saída após o redirecionamento
    }

    // Execute o comando original
    execute_command(command);

    // Restaure os descritores de arquivo padrão após o redirecionamento
    dup2(STDIN_FILENO, 0);  // Restaure a entrada padrão
    dup2(STDOUT_FILENO, 1); // Restaure a saída padrão

    return 0; // Retorna com status 0 (sem erro)
}

// Função para executar comandos com pipes
int execute_piped_command(char *command) {
    char *token;
    char *saveptr;
    int prev_pipe_fds[2]; // Armazena os descritores de pipe do comando anterior
    int curr_pipe_fds[2]; // Armazena os descritores de pipe do comando atual

    int first_command = 1; // Flag para o primeiro comando
    int last_command = 0; // Flag para o último comando
    pid_t prev_pid = 0; // Armazena o PID do comando anterior

    // Use strtok_r para dividir a entrada em comandos com base em |
    token = strtok_r(command, "|", &saveptr);

    while (token != NULL) {
        if (strlen(token) > 0 && strspn(token, " \t\n\r\f\v") != strlen(token)) {
            if (!first_command) {
                // Se não for o primeiro comando, redirecione a entrada para o pipe anterior
                if (dup2(prev_pipe_fds[0], STDIN_FILENO) == -1) {
                    perror("Erro ao redirecionar a entrada padrão");
                    return -1;
                }
                close(prev_pipe_fds[0]); // Feche a extremidade de leitura do pipe anterior
                close(prev_pipe_fds[1]); // Feche a extremidade de escrita do pipe anterior
            } else {
                first_command = 0; // Desabilite a flag do primeiro comando
            }

            // Se não for o último comando, crie um novo pipe
            if (strtok_r(NULL, "|", &saveptr) != NULL) {
                if (pipe(curr_pipe_fds) == -1) {
                    perror("Erro ao criar um pipe");
                    return -1;
                }
            } else {
                last_command = 1; // Habilita a flag do último comando
            }

            // Execute o comando dentro de um novo processo filho
            pid_t pid = fork();

            if (pid == 0) { // Processo filho
                if (!first_command) {
                    close(prev_pipe_fds[0]); // Fecha a extremidade de leitura do pipe anterior
                }
                if (!last_command) {
                    close(curr_pipe_fds[0]); // Fecha a extremidade de leitura do pipe atual
                    // Redirecione a saída para o pipe atual
                    if (dup2(curr_pipe_fds[1], STDOUT_FILENO) == -1) {
                        perror("Erro ao redirecionar a saída padrão");
                        exit(1);
                    }
                }

                // Execute o comando usando execvp
                execute_command(token);
                exit(0); // Saia do processo filho após a execução do comando
            } else if (pid < 0) {
                perror("Erro ao criar um processo filho");
                return -1;
            } else {
                // Processo pai
                if (!first_command) {
                    close(prev_pipe_fds[0]); // Fecha a extremidade de leitura do pipe anterior
                    close(prev_pipe_fds[1]); // Fecha a extremidade de escrita do pipe anterior
                }
                if (!last_command) {
                    close(curr_pipe_fds[1]); // Fecha a extremidade de escrita do pipe atual
                }

                prev_pipe_fds[0] = curr_pipe_fds[0]; // Atualiza o pipe anterior
                prev_pipe_fds[1] = curr_pipe_fds[1];

                prev_pid = pid;
            }
        }
        token = strtok_r(NULL, "|", &saveptr);
    }

    // Aguarde o término do último processo filho antes de retornar
    int status;
    wait(&status);

    return 0; // Retorna com status 0 (sem erro)
}

/* int execute_piped_command(char *command) {
    char *token;
    char *saveptr;
    char *prev_pipe = NULL; // Armazena o descritor de leitura do pipe anterior
    int pipe_fds[2]; // Array para os descritores de pipe

    // Use strtok_r para dividir a entrada em comandos com base em |
    token = strtok_r(command, "|", &saveptr);

    while (token != NULL) {
        if (strlen(token) > 0 && strspn(token, " \t\n\r\f\v") != strlen(token)) {
            // Crie um pipe para conectar a saída do comando anterior à entrada do próximo
            if (pipe(pipe_fds) == -1) {
                perror("Erro ao criar um pipe");
                return -1;
            }

            // Execute o comando dentro de um novo processo filho
            pid_t pid = fork();

            if (pid == 0) { // Processo filho
                close(pipe_fds[0]); // Fecha a extremidade de leitura do pipe

                // Redireciona a saída padrão (STDOUT) para a extremidade de escrita do pipe
                if (dup2(pipe_fds[1], STDOUT_FILENO) == -1) {
                    perror("Erro ao redirecionar a saída padrão");
                    exit(1);
                }

                close(pipe_fds[1]); // Fecha a extremidade de escrita do pipe

                // Execute o comando usando execvp
                execute_command(token);
                exit(0); // Saia do processo filho após a execução do comando
            } else if (pid < 0) {
                perror("Erro ao criar um processo filho");
                return -1;
            } else {
                // Processo pai
                close(pipe_fds[1]); // Fecha a extremidade de escrita do pipe

                // Aguarda o término do processo filho
                int status;
                wait(&status);

                // Verifica se o processo filho terminou normalmente
                if (WIFEXITED(status)) {
                    int exit_status = WEXITSTATUS(status);
                    if (exit_status != 0) { // Exibe mensagens de erro apenas se o status for diferente de 0
                        perror("Erro ao terminar o processo filho");
                    }
                }

                // Redireciona a saída do comando anterior para a entrada do próximo
                if (prev_pipe != NULL) {
                    if (dup2(pipe_fds[0], STDIN_FILENO) == -1) {
                        perror("Erro ao redirecionar a entrada padrão");
                        return -1;
                    }
                }

                close(pipe_fds[0]); // Fecha a extremidade de leitura do pipe
                prev_pipe = token;
            }
        }
        token = strtok_r(NULL, "|", &saveptr);
    }

    return 0; // Retorna com status 0 (sem erro)
} */

// Função para executar um comando no modo sequencial
int execute_sequential_command(char *command) {
    char *token;
    char *saveptr;

    // Use strtok_r para dividir a entrada em comandos com base em ;
    token = strtok_r(command, ";", &saveptr);

    // Crie um único processo filho
    pid_t pid = fork();

    if (pid == 0) { // Processo filho
        while (token != NULL) {
            // Verifique se o comando está vazio ou consiste apenas em espaços
            if (strlen(token) > 0 && strspn(token, " \t\n\r\f\v") != strlen(token)) {
                // Execute o comando dentro do processo filho
                execute_command(token);
            }
            // Avance para o próximo comando
            token = strtok_r(NULL, ";", &saveptr);
        }
        exit(0); // Saia do processo filho após a execução de todos os comandos
    } else if (pid < 0) {
        // Erro ao criar um processo filho
        perror("Erro ao criar um processo filho");
        return -1;
    } else {
        // Processo pai
        int status;
        pid_t wpid;
        do {
            wpid = wait(&status);// Aguarda o término do processo filho usando wait
        } while (wpid > 0);
        
        // Verifique se o processo filho terminou normalmente
        if (WIFEXITED(status)) {
            int exit_status = WEXITSTATUS(status);
            
        }
    }

    return 0; // Retorna com status 0 (sem erro)
}


/* 
// Função executada por cada thread para executar um comando
int execute_parallel_command(char *command) {
    char *token;
    char *saveptr;
    pthread_t *threads;
    int num_threads = 0;

    // Use strtok_r para dividir a entrada em comandos com base em ;
    token = strtok_r(command, ";", &saveptr);

    // Contar o número de comandos para criar as threads
    while (token != NULL) {
        if (strlen(token) > 0 && strspn(token, " \t\n\r\f\v") != strlen(token)) {
            num_threads++;
        }
        token = strtok_r(NULL, ";", &saveptr);
    }

    // Alocar espaço para os identificadores das threads
    threads = (pthread_t *)malloc(num_threads * sizeof(pthread_t));
    if (threads == NULL) {
        perror("Erro na alocação de memória");
        return -1;
    }

    // Reiniciar o ponteiro para a próxima iteração
    saveptr = NULL;
    token = strtok_r(command, ";", &saveptr);
    int thread_index = 0;

    // Criar uma thread para cada comando
    while (token != NULL) {
        if (strlen(token) > 0 && strspn(token, " \t\n\r\f\v") != strlen(token)) {
            // Aloque memória para copiar o comando, já que cada thread pode modificá-lo
            char *cmd_copy = strdup(token);
            if (cmd_copy == NULL) {
                perror("Erro na alocação de memória");
                return -1;
            }

            // Crie uma thread para executar o comando
            if (pthread_create(&threads[thread_index], NULL, thread_function, (void *)cmd_copy) != 0) {
                perror("Erro ao criar uma thread");
                return -1;
            }

            thread_index++;
        }
        token = strtok_r(NULL, ";", &saveptr);
    }

    // Aguarde a conclusão de todas as threads
    for (int i = 0; i < num_threads; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
            perror("Erro ao aguardar a thread");
            return -1;
        }
    }

    // Libere a memória alocada para os identificadores das threads
    free(threads);

    return 0; // Retorna com status 0 (sem erro)
} */

int execute_background(const char *command) {
    pid_t pid;

    // Crie um processo filho
    pid = fork();

    if (pid == -1) {
        perror("Erro ao criar um processo filho");
        return -1;
    }

    if (pid == 0) { // Código executado no processo filho
        // Use execl para executar o comando com /bin/sh
        execl("/bin/sh", "sh", "-c", command, (char *)NULL);
        perror("Erro ao executar o comando");
        exit(EXIT_FAILURE);
    } else { // Código executado no processo pai
        /* //printf("Comando iniciado (PID: %d)\n", pid);
        // Aguarde a conclusão do processo filho
        int status;
        wait(&status);

        if (WIFEXITED(status)) {
            int exit_status = WEXITSTATUS(status);
            //printf("Comando concluído com status %d\n", exit_status);
        } else {
            perror("Erro ao aguardar o processo filho");
        } */
        // Registre o PID do processo em segundo plano
        background_processes[num_background_processes++] = pid;
        printf("[%d] %d\n", num_background_processes, pid);
        

        return pid; // Retorna o PID do processo filho
    }
}

void execute_command(char *command) {
    size_t temp=strlen(command);
    if(strcmp(command,"!!")==0){
        if (history != NULL) {
            execute_command(history);
        } else {
            printf("No commands\n");
        }
    }else if (command[temp-1]=='&'){
        execute_background(command);
    }else if (strchr(command, '|') != NULL) {
        // Se contém pipes, execute o comando usando execute_piped_command
        execute_piped_command(command);
    } else if (strchr(command, '<') != NULL || strchr(command, '>') != NULL || strstr(command, ">>") != NULL) {
        // Se contém redirecionamento, execute o comando usando execute_redirect_command
        execute_redirect_command(command);
    } else {

        // Verifique se o comando começa com "echo"
        if (strncmp(command, "echo ", 5) == 0) {
            // Remova as aspas duplas se presentes
            char *text = command + 5;
            size_t text_length = strlen(text);
            
            if ((text[0] == '"' && text[text_length - 1] == '"')||(text[0] == '\'' && text[text_length - 1] == '\'')) {
                text[text_length - 1] = '\0'; // Remova a aspa dupla final
                text++; // Avance para depois da primeira aspa dupla
            }

            // Use write para imprimir o texto diretamente (sem as aspas)
            write(STDOUT_FILENO, text, strlen(text));
            write(STDOUT_FILENO, "\n", 1);
        } else {
            // Inicialize um array para armazenar os argumentos
            char **args = NULL;
            int arg_count = 0;

            // Use strtok para dividir a string em argumentos
            char *token = strtok(command, " ");
            while (token != NULL) {
                args = realloc(args, (arg_count + 1) * sizeof(char *));
                if (args == NULL) {
                    perror("Erro na alocação de memória");
                    exit(1);
                }
                args[arg_count++] = token;
                token = strtok(NULL, " ");
            }
            args = realloc(args, (arg_count + 1) * sizeof(char *));
            if (args == NULL) {
                perror("Erro na alocação de memória");
                exit(1);
            }
            args[arg_count] = NULL; // Certifique-se de terminar com NULL para execvp

            // Execute o comando usando execvp
            pid_t pid = fork();
            if (pid == 0) { // Processo filho
                if (execvp(args[0], args) == -1) {
                    char copia[1024] = "Erro no comando ";
                    strcat(copia,command);
                    perror(copia);
                    
                    exit(1); // Saia do processo filho em caso de erro
                }
                exit(0);
            } else if (pid < 0) {
                perror("Erro ao criar um processo filho");
                exit(1);
            } else {
                // Processo pai
                int status;
                wait(&status); // Aguarda o término do processo filho com o PID correto
                // Verifique se o processo filho terminou normalmente
                if (WIFEXITED(status)) {
                    int exit_status = WEXITSTATUS(status);
                    
                }
            }

        // Libere a memória alocada para os argumentos
        free(args);
        }
    }
}

void insertAtBeginning(struct thread_node** head, pthread_t newThreadId) {
    struct thread_node* newNode = (struct thread_node*)malloc(sizeof(struct thread_node));
    if (newNode == NULL) {
        perror("Erro: Falha na alocação de memória.");
        exit(EXIT_FAILURE);
    }
    newNode->thread_id = newThreadId;
    newNode->next = *head;
    *head = newNode;
}

void *thread_function(void *arg) {
   /*  char *command = (char *)arg;
    // Execute o comando
    execute_command(command);
    free(command); // Libere a memória alocada para o comando
    
    return NULL; */
    char *command = (char *)arg;
    system(command);
    pthread_exit(NULL);
}

int execute_parallel_command(char* command) {
    struct thread_node* head = NULL;
    struct thread_node* join = NULL;
    int thread_count = 0;

    char* token = strtok(command, ";");
    while (token != NULL) {
        pthread_t thread;

        if (pthread_create(&thread, NULL, thread_function, token) != 0) {
            perror("Erro ao criar uma thread");
            return -1;
        }

        insertAtBeginning(&head, thread);
        thread_count++;

        token = strtok(NULL, ";");
    }

    join = head;
    for (int i = 0; i < thread_count; i++) {
        if (pthread_join(join->thread_id, NULL) != 0) {
            perror("Erro ao aguardar a thread");
            return -1;
        }
        join = join->next;
    }

    // Libere a memória alocada para as estruturas de dados
    struct thread_node* current = head;
    while (current != NULL) {
        struct thread_node* temp = current;
        current = current->next;
        free(temp);
    }

    return 0;
}