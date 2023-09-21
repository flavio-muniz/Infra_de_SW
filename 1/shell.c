#include <stdio.h>
#include <unistd.h>
#include<string.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<pthread.h>

char *history = NULL;

//fork() - OK
//exec() - OK, com ressalvas
//wait() - OK
//dup2() - FALTA
//pipe() - FALTA
//pthread_create() - FALTA
//pthread_join() - FALTA

//falta a partir de pipes
typedef struct Node {
    char *arg;
    struct Node *next;
} Node;

// Estrutura para armazenar o resultado da execução de uma thread
typedef struct {
    int thread_id;
    int status; // 0 se a execução foi bem-sucedida, -1 se houve um erro
} ThreadResult;


void execute_command(char *command) {
    if (strcmp(command, "exit") == 0) {
        exit(0); // Se o comando for "exit", simplesmente saia do processo filho
    }
    // Verifique se o comando começa com "echo"
    if (strncmp(command, "echo ", 5) == 0) {
        // Remova as aspas duplas se presentes
        char *text = command + 5;
        size_t text_length = strlen(text);

        if (text[0] == '"' && text[text_length - 1] == '"') {
            text[text_length - 1] = '\0'; // Remova a aspa dupla final
            text++; // Avance para depois da primeira aspa dupla
        }

        // Use write para imprimir o texto diretamente (sem as aspas)
        write(STDOUT_FILENO, text, strlen(text));
        write(STDOUT_FILENO, "\n", 1);
    }else {
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
        if (execvp(args[0], args) == -1) {
            perror("execvp");
            //exit(1); // Saia do processo filho em caso de erro
        }
        // Libere a memória alocada para os argumentos
        free(args);
    }
}

// Função que será executada por cada thread
void *execute_command_thread(void *arg) {
    char *command = (char *)arg;
    execute_command(command);
    return NULL;
}

int main(int argc, char *argv[]){
    int should_run = 1; /* flag para determinar quando encerrar o programa */
    int parallel_mode = 0; // 0 para sequencial, 1 para paralelo (estilo padrão é sequencial)

    // Vetor para armazenar as threads criadas
    pthread_t *threads = NULL;
    int num_threads = 0;

    if (argc==1){
        //modo interativo
        while (should_run) {
            printf("frssm %s> ", parallel_mode ? "par" : "seq");
            fflush(stdout);

            char *input = NULL;
            size_t input_size = 0;

            if (getline(&input, &input_size, stdin) == -1) {
                perror("Erro na leitura da entrada");
                break;
            }

            size_t input_length = strlen(input);
            if (input_length > 0 && input[input_length - 1] == '\n') {
                input[input_length - 1] = '\0';
            }


            if (strcmp(input, "style sequential") == 0) {
                parallel_mode = 0;
                continue;
            } else if (strcmp(input, "style parallel") == 0) {
                parallel_mode = 1;
                continue;
            }

            char *token = strtok(input, ";");
            while (token != NULL) {
                if (parallel_mode) {
                    // Crie uma nova thread para executar o comando em paralelo
                    pthread_t thread;
                    pthread_create(&thread, NULL, execute_command_thread, (void *)token);
                    // Adicione a thread ao vetor
                    threads = realloc(threads, (num_threads + 1) * sizeof(pthread_t));
                    threads[num_threads++] = thread;
                    //pthread_join(thread, NULL); // Aguarde o término da thread
                    //pthread_detach(thread); // Detache a thread para que ela não bloqueie o programa
                } else {
                    pid_t pid = fork();
                    if (pid == -1) {
                        perror("fork");
                        exit(EXIT_FAILURE);
                    } else if (pid == 0) {
                        // Este é o processo filho
                        execute_command(token);
                        exit(EXIT_SUCCESS);
                    } else {
                        int status;
                        waitpid(pid, &status, 0); // Espere pelo término do processo filho
                    }
                }
            
            // Após o loop while acima, espere que todas as threads terminem
            if (parallel_mode) {
                for (int i = 0; i < num_threads; i++) {
                    pthread_join(threads[i], NULL);
                }
                free(threads);
            }
                token = strtok(NULL, ";");
            }

            if (strcmp(input, "exit") == 0) {
                should_run = 0; // Define should_run como 0 para sair do loop
            }
        }
    }else if(argc==2){
        // Modo batch: Um arquivo de lote foi fornecido como argumento
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

            // Verifique se a linha contém um comando de estilo
            if (strcmp(line, "style sequential") == 0) {
                parallel_mode = 0;
                continue; // Não processe este comando como um comando normal
            } else if (strcmp(line, "style parallel") == 0) {
                parallel_mode = 1;
                continue; // Não processe este comando como um comando normal
            }

            // Separe os comandos usando o caractere ';'
            char *token = strtok(line, ";");
            while (token != NULL) {
                // Crie um novo processo para executar o comando
                pid_t pid = fork();
                if (pid == -1) {
                    perror("fork");
                    exit(EXIT_FAILURE);
                } else if (pid == 0) {
                    // Este é o processo filho
                    execute_command(token);
                } else {
                    // Este é o processo pai, espere pelo término do processo filho
                    int status;
                    wait(&status);
                }
                // Avance para o próximo comando
                token = strtok(NULL, ";");
            }
        }
        free(line); // Libera a memória alocada para a linha lida
        fclose(batch_file);

    } else {
        fprintf(stderr, "Uso incorreto: %s [batchFile]\n", argv[0]);
        return 1;
    }

    // Aguarde todas as threads concluírem
    if (parallel_mode) {
        pthread_exit(NULL);
    }

    return 0;
     
}




/*#include <stdio.h>
#include <unistd.h>
#include<string.h>
#include<stdlib.h>
#include<pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>

typedef struct Node {
    char *arg;
    struct Node *next;
} Node;

typedef struct CommandHistory {
    Node *head;
    Node *tail;
} CommandHistory;

// Função para adicionar um comando ao histórico
void addToHistory(CommandHistory *history, const char *command);
// Função para liberar a memória alocada para o histórico de comandos
void freeHistory(CommandHistory *history);
// Função para inicializar a estrutura de histórico de comandos
void initHistory(CommandHistory *history);
// Função que será executada por cada thread
void *execute_command(void *arg);
// Função para remover espaços em branco do início e do fim de uma string
char *trim(char *str);


int main(int argc, char *argv[]){
    CommandHistory history;
    initHistory(&history);
    int should_run = 1; // flag para determinar quando encerrar o programa
    int parallel_mode = 0; // 0 para sequencial, 1 para paralelo (estilo padrão é sequencial)

    if (argc==1){
        //modo interativo
        while (should_run) {
            //printf("frssm seq> ");
            printf("frssm %s> ", parallel_mode ? "par" : "seq");
            fflush(stdout);

            // Aloca dinamicamente memória para a entrada
            char *input = NULL;
            size_t input_size = 0;
            ssize_t characters_read;//usado para representar tamanhos de dados(bytes) assinados

            characters_read = getline(&input, &input_size, stdin);
            if (characters_read == -1) {
                perror("Erro na leitura da entrada");
                break; // Sai do loop se houver um erro na leitura
            }

            //Remova a quebra de linha do final da entrada
            size_t input_length = strlen(input);
            if (input_length > 0 && input[input_length - 1] == '\n') {
                input[input_length - 1] = '\0';
            }    

            addToHistory(&history, input);

            // Verifique se o comando contém um pipe
            if (strchr(input, '|') != NULL) {
                // Comando com pipe
                // Implemente a lógica para executar comandos em sequência com pipes
                // Exemplo: ls -l | sort -k 5

                int pipefd[2];
                pid_t child_pid;

                // Divida o comando em partes usando o caractere '|'
                char *command1 = strtok(input, "|");
                char *command2 = strtok(NULL, "|");

                // Remova espaços em branco em branco do início e do fim dos comandos
                command1 = trim(command1);
                command2 = trim(command2);

                // Crie um pipe
                if (pipe(pipefd) == -1) {
                    perror("Erro ao criar o pipe");
                    exit(EXIT_FAILURE);
                }

                // Crie um processo filho
                child_pid = fork();
                if (child_pid == -1) {
                    perror("Erro ao criar um processo filho");
                    exit(EXIT_FAILURE);
                }

                if (child_pid == 0) {
                    // Este é o processo filho

                    // Feche a extremidade de leitura do pipe
                    close(pipefd[0]);

                    // Redirecione a saída padrão para a extremidade de escrita do pipe
                    dup2(pipefd[1], STDOUT_FILENO);
                    
                    // Executa o primeiro comando
                    execute_command(command1);

                } else {
                    //esse é o processo pai

                    // Espere o processo filho terminar
                    wait(NULL);

                    // Feche a extremidade de escrita do pipe
                    close(pipefd[1]);

                    // Redirecione a entrada padrão para a extremidade de leitura do pipe
                    dup2(pipefd[0], STDIN_FILENO);

                    // Executa o segundo comando
                    execute_command(command2);
                    
                }
            }else if(strchr(input,'>')!=NULL){
                // Comando com redirecionamento de saída
                char *command = strtok(input, ">");
                char *output_file = strtok(NULL, ">");

                // Remove espaços em branco do início e do fim
                command = trim(command);
                output_file = trim(output_file);

                int append = 0; // Para verificação de '>>'
                // Verifique se é um redirecionamento de saída com '>>'
                if (strstr(output_file, ">>") != NULL) {
                    output_file = strtok(output_file, ">>");
                    append = 1;
                }

                // Abra o arquivo de saída para escrita (ou adição ao final, se for o caso)
                int fd;
                if (append) {
                    fd = open(output_file, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
                } else {
                    fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
                }
                if (fd == -1) {
                    perror("Erro ao abrir o arquivo de saída");
                    exit(EXIT_FAILURE);
                }

                // Redirecione a saída padrão para o arquivo
                dup2(fd, STDOUT_FILENO);

                // Executa o comando
                execute_command(command);

                // Fecha o arquivo de saída
                close(fd);
            }else if(strchr(input, '<') != NULL){
                // Comando com redirecionamento de entrada

                char *command = strtok(input, "<");
                char *input_file = strtok(NULL, "<");

                // Remove espaços em branco do início e do fim
                command = trim(command);
                input_file = trim(input_file);

                // Abra o arquivo de entrada para leitura
                int fd = open(input_file, O_RDONLY);
                if (fd == -1) {
                    perror("Erro ao abrir o arquivo de entrada");
                    exit(EXIT_FAILURE);
                }
                // Redirecione a entrada padrão para o arquivo
                dup2(fd, STDIN_FILENO);

                // Executa o comando
                execute_command(command);

                // Fecha o arquivo de entrada
                close(fd);

            }else{
                // Comando simples

                // Separe os comandos usando o caractere ';'
                char *token = strtok(input, ";");

                while (token != NULL) {
                    // Crie uma nova thread para executar o comando
                    pthread_t thread;
                    pthread_create(&thread, NULL, execute_command, (void *)token);
                    pthread_join(thread, NULL);//?
                    // Avance para o próximo comando
                    token = strtok(NULL, ";");
                }
            }

            if (strcmp(input, "exit") == 0) {
                printf("2");
                should_run = 0; // Define should_run como 0 para sair do loop
            }
        }

    }else if(argc==2){
        // Modo batch: Um arquivo de lote foi fornecido como argumento

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
            
            if (strchr(line, '|') != NULL) {
                // Comando com pipe
                // Implemente a lógica para executar comandos em sequência com pipes
                // Exemplo: ls -l | sort -k 5

                int pipefd[2];
                pid_t child_pid;

                // Divida o comando em partes usando o caractere '|'
                char *command1 = strtok(line, "|");
                char *command2 = strtok(NULL, "|");

                // Remova espaços em branco em branco do início e do fim dos comandos
                command1 = trim(command1);
                command2 = trim(command2);

                // Crie um pipe
                if (pipe(pipefd) == -1) {
                    perror("Erro ao criar o pipe");
                    exit(EXIT_FAILURE);
                }

                // Crie um processo filho
                child_pid = fork();
                if (child_pid == -1) {
                    perror("Erro ao criar um processo filho");
                    exit(EXIT_FAILURE);
                }

                if (child_pid == 0) {
                    // Este é o processo filho

                    // Feche a extremidade de leitura do pipe
                    close(pipefd[0]);

                    // Redirecione a saída padrão para a extremidade de escrita do pipe
                    dup2(pipefd[1], STDOUT_FILENO);

                   // Executa o primeiro comando
                    execute_command(command1);

                } else {
                    //esse é o processo pai

                    // Espere o processo filho terminar
                    wait(NULL);

                    // Feche a extremidade de escrita do pipe
                    close(pipefd[1]);

                    // Redirecione a entrada padrão para a extremidade de leitura do pipe
                    dup2(pipefd[0], STDIN_FILENO);

                    // Executa o segundo comando
                    execute_command(command2);
                }
            }else if(strchr(line,'>')!=NULL){
                // Comando com redirecionamento de saída
            
                char *command = strtok(line, ">");
                char *output_file = strtok(NULL, ">");

                // Remove espaços em branco do início e do fim
                command = trim(command);
                output_file = trim(output_file);

                int append = 0; // Para verificação de '>>'

                // Verifique se é um redirecionamento de saída com '>>'
                if (strstr(output_file, ">>") != NULL) {
                    output_file = strtok(output_file, ">>");
                    append = 1;
                }

                // Abra o arquivo de saída para escrita (ou adição ao final, se for o caso)
                int fd;
                if (append) {
                    fd = open(output_file, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
                } else {
                    fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
                }

                if (fd == -1) {
                    perror("Erro ao abrir o arquivo de saída");
                    exit(EXIT_FAILURE);
                }

                // Redirecione a saída padrão para o arquivo
                dup2(fd, STDOUT_FILENO);

                // Executa o comando
                execute_command(command);

                // Fecha o arquivo de saída
                close(fd);
            }else if(strchr(line,'<')!=NULL){
                // Comando com redirecionamento de entrada

                char *command = strtok(line, "<");
                char *input_file = strtok(NULL, "<");

                // Remove espaços em branco do início e do fim
                command = trim(command);
                input_file = trim(input_file);

                // Abra o arquivo de entrada para leitura
                int fd = open(input_file, O_RDONLY);
                if (fd == -1) {
                    perror("Erro ao abrir o arquivo de entrada");
                    exit(EXIT_FAILURE);
                }

                // Redirecione a entrada padrão para o arquivo
                dup2(fd, STDIN_FILENO);

                // Executa o comando
                execute_command(command);

                // Fecha o arquivo de entrada
                close(fd);
   
            }else {
                // Comando simples

                // Separe os comandos usando o caractere ';'
                char *token = strtok(line, ";");

                while (token != NULL) {
                    // Crie uma nova thread para executar o comando
                    pthread_t thread;
                    pthread_create(&thread, NULL, execute_command, (void *)token);
                    pthread_join(thread, NULL);//?
                    // Avance para o próximo comando
                    token = strtok(NULL, ";");
                }
            }
            if (strcmp(line, "exit") == 0) {
                printf("1");
                break; // Sai do loop se encontrar o comando "exit"
            }
        }
        free(line); // Libera a memória alocada para a linha lida
        fclose(batch_file);

    } else {
        fprintf(stderr, "Uso incorreto: %s [batchFile]\n", argv[0]);
        return 1;
    }
    // Antes de sair, libere a memória do histórico
    freeHistory(&history);

    // Aguarde todas as threads concluírem
    pthread_exit(NULL);

    return 0;

}

void addToHistory(CommandHistory *history, const char *command) {
    Node *newNode = (Node *)malloc(sizeof(Node));
    if (newNode == NULL) {
        perror("Erro ao alocar memória para histórico");
        exit(EXIT_FAILURE);
    }

    newNode->arg = strdup(command);
    newNode->next = NULL;

    if (history->tail == NULL) {
        history->head = newNode;
        history->tail = newNode;
    } else {
        history->tail->next = newNode;
        history->tail = newNode;
    }
}

void freeHistory(CommandHistory *history) {
    Node *current = history->head;
    while (current != NULL) {
        Node *temp = current;
        current = current->next;
        free(temp->arg);
        free(temp);
    }
}

void initHistory(CommandHistory *history) {
    history->head = NULL;
    history->tail = NULL;
}

void *execute_command(void *arg) {
    char *command = (char *)arg;
    char *shell = "/bin/sh";

    // Use execvp para executar o comando
    char *cmd[] = {shell, "-c", command, NULL};
    execvp(shell, cmd);

    // Se a execução falhar, exiba um erro
    perror("Erro ao executar o comando");
    exit(EXIT_FAILURE);

    return NULL;
}

char *trim(char *str) {
    size_t len = strlen(str);

    // Remova espaços em branco do início
    while (len > 0 && isspace(str[len - 1])) {
        str[--len] = '\0';
    }

    // Remova espaços em branco do final
    size_t start = 0;
    while (isspace(str[start])) {
        start++;
    }

    if (start > 0) {
        len -= start;
        memmove(str, str + start, len);
        str[len] = '\0';
    }

    return str;
}
*/