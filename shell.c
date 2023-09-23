#include <stdio.h>
#include <unistd.h>
#include<string.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<pthread.h>
#include<fcntl.h>

char *history = NULL;
int last_command_exists = 0; // Variável para rastrear se o último comando existe


//fork() - OK
//pthread_create() - OK
//pthread_join() - OK
//exec() - FALTA
//wait() - FALTA
//dup2() - FALTA
//pipe() - FALTA

/*FALTA:
Pipe |
redirecionamento <, >, >>
background
ler arquivo [batch] que encaminhe para outro arquivo
batchfile sem comando exit
 */

struct thread_node {
    pthread_t thread_id;
    struct thread_node* next;
};

// Função executada por cada thread para executar um comando
int execute_parallel_command(char *command);
// Função para executar um comando no modo sequencial
int execute_sequential_command(char *command);
/* // executa 
void execute_command(char *command); */
void *thread_function(void *arg);
void add_thread(struct thread_node** head, pthread_t nova_thread);

int main(int argc, char *argv[]){
    int jump=0;

    int should_run = 1; /* flag para determinar quando encerrar o programa */
    int parallel_mode = 0; // 0 para sequencial, 1 para paralelo (estilo padrão é sequencial)

    if (argc==1){ //modo interativo
        while (should_run){
            printf("frssm %s> ", parallel_mode ? "par" : "seq");
            fflush(stdout);

            char *input = NULL;
            size_t input_size = 0;

            if (getline(&input, &input_size, stdin) == -1) {
                if (feof(stdin)){
                    printf("ctrl + D\n");
                }else{
                    perror("Erro na leitura da entrada");
                }
                break;
            }
 

            size_t input_length = strlen(input);
            if (input_length > 0 && input[input_length - 1] == '\n') {
                input[input_length - 1] = '\0';
            }

            // Se a entrada for "!!", imprimir o último comando
            if (strcmp(input, "!!") != 0) {
                // Liberando a memória do último comando anterior
                free(history);
                // Copiando o conteúdo de input para last_command
                history = malloc(strlen(input) + 1); // +1 para o caractere nulo
                if (history == NULL) {
                    perror("Erro ao alocar memória");
                    return 1;
                }
                strcpy(history, input);
                last_command_exists = 1; // Atualiza a flag para indicar que o último comando existe
            }
            
            if (strcmp(input, "style sequential") == 0 /* || (strcmp(history, "style sequential")==0 && strcmp(input, "!!")==0) */) {//cada ação deve ocorrer uma por vez, da esquerda para a direita, num mesmo processo filho.
                parallel_mode = 0;
                continue;
            } else if (strcmp(input, "style parallel") ==  0 /*|| (strcmp(history, "style parallel")==0 && strcmp(input, "!!")==0) */) {//todas ações devem ser executadas em paralelo, nesse caso uma nova thread deve ser criada para cada comando. Não há limites para o número de comando por linha.
                parallel_mode = 1;
                continue;
            }
            if (history!=NULL){
                if ((strcmp(history, "style sequential")==0 && strcmp(input, "!!")==0)){
                    continue;
                }else if(strcmp(history, "style parallel")==0 && strcmp(input, "!!")==0){
                    continue;
                }
            }

            if (strcmp(input, "exit") == 0) {
                should_run = 0; // Define should_run como 0 para sair do loop
            }else if(!last_command_exists){
                printf("No commands\n");
            } else if (parallel_mode==0) {
                // Execute o comando no modo sequencial
                if (strcmp(input, "!!") == 0 && last_command_exists) {
                    int status = execute_sequential_command(history);
                }else{
                    int status = execute_sequential_command(input);
                }
                // Trate o status conforme necessário
            } else {
                // Execute o comando em paralelo
                if (strcmp(input, "!!") == 0 && last_command_exists) {
                    int status = execute_parallel_command(history);
                }else{
                    int status = execute_parallel_command(input);
                }
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

            size_t line_length = strlen(line);
            if (line_length > 0 && line[line_length - 1] == '\n') {
                line[line_length - 1] = '\0';
            }

            // Se a entrada for "!!", imprimir o último comando
            if (strcmp(line, "!!") != 0) {
                // Liberando a memória do último comando anterior
                free(history);
                // Copiando o conteúdo de input para last_command
                history = malloc(strlen(line) + 1); // +1 para o caractere nulo
                if (history == NULL) {
                    perror("Erro ao alocar memória");
                    return 1;
                }
                strcpy(history, line);
                last_command_exists = 1; // Atualiza a flag para indicar que o último comando existe
            }

            if (strcmp(line, "style sequential") == 0 /* || (strcmp(history, "style sequential")==0 && strcmp(input, "!!")==0) */) {//cada ação deve ocorrer uma por vez, da esquerda para a direita, num mesmo processo filho.
                parallel_mode = 0;
                continue;
            } else if (strcmp(line, "style parallel") ==  0 /*|| (strcmp(history, "style parallel")==0 && strcmp(input, "!!")==0) */) {//todas ações devem ser executadas em paralelo, nesse caso uma nova thread deve ser criada para cada comando. Não há limites para o número de comando por linha.
                parallel_mode = 1;
                continue;
            }

            if (history!=NULL){
                if ((strcmp(history, "style sequential")==0 && strcmp(line, "!!")==0)){
                    continue;
                }else if(strcmp(history, "style parallel")==0 && strcmp(line, "!!")==0){
                    continue;
                }
            }

            if (strcmp(line, "exit") == 0) {
                should_run = 0; // Define should_run como 0 para sair do loop
                //free(line); // Libera a memória alocada para a linha lida
                fclose(batch_file);
            }else if(!last_command_exists){
                printf("No commands\n");
            } else if (parallel_mode==0) {
                // Execute o comando no modo sequencial
                if (strcmp(line, "!!") == 0 && last_command_exists) {
                    int status = execute_sequential_command(history);
                }else{
                    int status = execute_sequential_command(line);
                }
                // Trate o status conforme necessário
            } else {
                // Execute o comando em paralelo
                if (strcmp(line, "!!") == 0 && last_command_exists) {
                    int status = execute_parallel_command(history);
                }else{
                    int status = execute_parallel_command(line);
                }
                // Trate o status conforme necessário
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

void *thread_function(void *arg) {
    char *command = (char *)arg;
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "%s 2>/dev/null", command); // Redireciona a saída de erro para /dev/null
    int status = system(buffer);

    if (status == -1) {
        printf("Erro ao executar o comando: %s\n", command);
        perror("Detalhes do erro");
        exit(1);
    } else if (status != 0) {
        printf("Comando %s: not found\n", command);
    }

    pthread_exit(NULL);
}

//funcionando no arg1
//(head, nova thread)
void add_thread(struct thread_node** head, pthread_t nova_thread) {
    struct thread_node* novo = (struct thread_node*)malloc(sizeof(struct thread_node));
    if (novo == NULL) {
        perror("Erro: Falha na alocação de memória.");
        exit(EXIT_FAILURE);
    }
    novo->thread_id = nova_thread;
    novo->next = *head;
    *head = novo;
}

//funciona no arg1
int execute_sequential_command(char *commands) {
    pid_t child_pid = fork(); // Cria um único processo filho

    if (child_pid == -1) {
        perror("Erro ao criar processo filho");
        return 1;
    } else if (child_pid == 0) { // Código a ser executado no processo filho
        char *token;
        char *saveptr;
        int last_command_status = 0; // Inicializa o status com zero

        // Use strtok_r para dividir a entrada em comandos com base em ;
        token = strtok_r(commands, ";", &saveptr);

        while (token != NULL) {
            //verificar processo filho
            pid_t kkk = getpid();
            printf("%d\n", kkk);
            // Verifique se o comando está vazio ou consiste apenas em espaços
            if (strlen(token) > 0 && strspn(token, " \t\n\r\f\v") != strlen(token)) {
                char buffer[256];
                snprintf(buffer, sizeof(buffer), "%s 2>/dev/null", token); // Redireciona a saída de erro para /dev/null
                int status = system(buffer); // Execute o comando no shell

                if (status == -1) {
                    printf("Erro ao executar o comando: %s\n", token);
                    perror("Detalhes do erro");
                    exit(1);
                } else if (status != 0) {
                    printf("Comando %s: not found\n", token);
                    exit(1);
                }

                last_command_status = status;
            }

            // Avance para o próximo comando
            token = strtok_r(NULL, ";", &saveptr);
        }

        exit(last_command_status); // Saia do processo filho com o status do último comando
    } else { // Código a ser executado no processo pai
        int status;
        waitpid(child_pid, &status, 0); // Espere pelo término do processo filho

        if (WIFEXITED(status)) {
            return WEXITSTATUS(status); // Retorne o status de saída do processo filho
        } else {
            return 1; // O comando não terminou normalmente
        }
    }
}

//funcionando no arg1
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

        add_thread(&head, thread);
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

//inutilizado
/* 
void execute_command(char *command) {
    size_t temp=strlen(command);
    if(strcmp(command,"!!")==0){
        if (last_command_exists) {
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
                pid_t kkk = getpid();
                printf("%d\n", kkk);
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
} */