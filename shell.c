#include <stdio.h>
#include <unistd.h>
#include<string.h>
#include<stdlib.h>
#include <ctype.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<pthread.h>
#include<fcntl.h>
#include <signal.h>

char *history = "No commands.";
int last_command_exists = 0; // Variável para rastrear se o último comando existe
int parallel_mode = 0; // 0 para sequencial, 1 para paralelo (estilo padrão é sequencial)
int should_run = 1; /* flag para determinar quando encerrar o programa */
//fork() - OK
//pthread_create() - OK
//pthread_join() - OK
//exec() - FALTA
//wait() - OK
//dup2() - FALTA
//pipe() - OK

/*FALTA:
Pipe |
redirecionamento <, >, >>
background
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
void ltrim(char *str);
void rtrim(char *str);
// Função para remover espaços em branco à esquerda e à direita de uma string
void trim(char *str);
int historico_seq(char *commands, pid_t kkk);
//troca as !! que tiverem no input pelo que tiver no histórico
void replace (char *input) ;


int main(int argc, char *argv[]){
    int hist=0;
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

            replace(input);
            if (strcmp(input, "style sequential") == 0 ) {//cada ação deve ocorrer uma por vez, da esquerda para a direita, num mesmo processo filho.
                parallel_mode = 0;
                continue;
            } else if (strcmp(input, "style parallel") ==  0 ) {//todas ações devem ser executadas em paralelo, nesse caso uma nova thread deve ser criada para cada comando. Não há limites para o número de comando por linha.
                parallel_mode = 1;
                continue;
            }
            if (parallel_mode==0) {
                // Execute o comando no modo sequencial
                int status = execute_sequential_command(input);
                
                // Trate o status conforme necessário
            } else {
                // Execute o comando em paralelo
                int status = execute_parallel_command(input);
                // Trate o status conforme necessário
            }
            
            
            if (strcmp(input,"!!")!=0 && hist==0){
                history = malloc(strlen(input) + 1);
                strcpy(history, input);
                last_command_exists = 1; // Atualiza a flag para indicar que o último comando existe
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
            
            size_t line_length = strlen(line);
            if (line_length > 0 && line[line_length - 1] == '\n') {
                line[line_length - 1] = '\0';
            }

            replace(line);
            if (strcmp(line, "style sequential") == 0 ) {//cada ação deve ocorrer uma por vez, da esquerda para a direita, num mesmo processo filho.
                parallel_mode = 0;
                continue;
            } else if (strcmp(line, "style parallel") ==  0 ) {//todas ações devem ser executadas em paralelo, nesse caso uma nova thread deve ser criada para cada comando. Não há limites para o número de comando por linha.
                parallel_mode = 1;
                continue;
            }
            if (parallel_mode==0) {
                // Execute o comando no modo sequencial
                int status = execute_sequential_command(line);
                
                // Trate o status conforme necessário
            } else {
                // Execute o comando em paralelo
                int status = execute_parallel_command(line);
                // Trate o status conforme necessário
            }
            
            
            if (strcmp(line,"!!")!=0 && hist==0){
                history = malloc(strlen(line) + 1);
                strcpy(history, line);
                last_command_exists = 1; // Atualiza a flag para indicar que o último comando existe
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


//funciona no arg1
int execute_sequential_command(char *commands) {
    
    int pipefd_parallel[2];
    int pipefd_should_run[2];
    if(pipe(pipefd_parallel)==-1 || pipe(pipefd_should_run)==-1){
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid_t pid_pai = getpid();
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
        //token = strtok(novaString, ";");
        char verificar [20];
        while (token != NULL /* && should_run==1 */) {
            trim(token);
            pid_t kkk = getpid();
            if(strcmp(token,"exit")==0){
                should_run=0;
                close(pipefd_should_run[0]);
                //should_run=0;
                write(pipefd_should_run[1],&should_run,sizeof(should_run));
                close (pipefd_parallel[1]);
                //kill(pid_pai,SIGTERM);
                break;
            }else if(strcmp(token,"No commands.")==0){
                printf("No commands.");
            }else if(strcmp(token,"style parallel")==0){
                parallel_mode = 1;
                token = strtok_r(NULL, ";", &saveptr);
                //inicio
                close(pipefd_parallel[0]);
                parallel_mode=1;
                write(pipefd_parallel[1],&parallel_mode, sizeof(parallel_mode));
                close (pipefd_parallel[1]);
                //fim
                continue; 
            }else if ( parallel_mode==1){
                execute_parallel_command(token);
            }else{
                //printf("Número do PID [%d]\n", kkk);
                // Verifique se o comando está vazio ou consiste apenas em espaços
                
                if (strlen(token) > 0 && strspn(token, " \t\n\r\f\v") != strlen(token)) {
                    char buffer[256];
                    snprintf(buffer, sizeof(buffer), "%s 2>/dev/null", token); // Redireciona a saída de erro para /dev/null
                    int status = system(buffer); // Execute o comando no shell
                    //perror("Erro ao executar o comando");
                    if (status == -1) {
                        printf("Erro ao executar o comando: %s\n", token);
                        perror("Detalhes do erro");
                        //exit(1);
                    } else if (status != 0) {
                        printf("Comando %s: not found\n", token);
                    }
                    last_command_status = status;
                }
                // Avance para o próximo comando
            }
            
            strcpy(verificar,token);
            token = strtok_r(NULL, ";", &saveptr);
        }
        if(strcmp(verificar,"No commands.")==0){
            printf("\n");
        }
        exit(last_command_status); // Saia do processo filho com o status do último comando        
    } else { // Código a ser executado no processo pai
    
        close(pipefd_parallel[1]);
        read(pipefd_parallel[0],&parallel_mode,sizeof(parallel_mode));
        close(pipefd_parallel[0]);
        int status;
        wait(&status); // Espere pelo término do processo filho
        close(pipefd_should_run[1]);
        read(pipefd_should_run[0],&should_run,sizeof(should_run));
        close(pipefd_should_run[0]);
        

        if (WIFEXITED(status)) { 
            return WEXITSTATUS(status); // Retorne o status de saída do processo filho      
        } else {
            return 1; // O comando não terminou normalmente
        }
        
    }
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

//funcionando no arg1
int execute_parallel_command(char *command) {
    struct thread_node* head = NULL;
    struct thread_node* join = NULL;
    int thread_count = 0;

    char* token = strtok(command, ";");
    if(parallel_mode==0){

    }
    while (token != NULL) {
        trim(token);
        if(strcmp(token,"exit")==0){
            should_run=0;
            break;
        }
        if(strcmp(token,"style sequential")==0){
            parallel_mode=0;
            token = strtok(NULL, ";");
            continue;
        }
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


// Função para remover espaços em branco à esquerda de uma string
void ltrim(char *str) {
    int len = strlen(str);
    int i, j = 0;

    // Encontre o primeiro caractere não branco
    for (i = 0; i < len; i++) {
        if (!isspace((unsigned char)str[i])) {
            break;
        }
    }

    // Desloque a string para a esquerda
    for (; i < len; i++) {
        str[j++] = str[i];
    }
    str[j] = '\0';
}

// Função para remover espaços em branco à direita de uma string
void rtrim(char *str) {
    int len = strlen(str);
    int i;

    // Encontre o último caractere não branco
    for (i = len - 1; i >= 0; i--) {
        if (!isspace((unsigned char)str[i])) {
            break;
        }
    }

    // Adicione um terminador nulo após o último caractere não branco
    str[i + 1] = '\0';
}

// Função para remover espaços em branco à esquerda e à direita de uma string
void trim(char *str) {
    ltrim(str);
    rtrim(str);
}


void replace (char *input) {
    char *token;
    char *saveptr;  // Ponteiro de estado para strtok_r
    char *temp;     // Ponteiro temporário para armazenar partes da string

    // Use strtok_r para obter o primeiro token
    token = strtok_r(input, ";", &saveptr);

    // String para armazenar a resposta
    char resposta[1000] = "";  // Você pode ajustar o tamanho conforme necessário

    while (token != NULL) {
        trim(token);
        // Verifique se o token é "<>"
        if (strcmp(token, "!!") == 0) {
            strcat(resposta, history);
            strcat(resposta,";");
        } else {
            strcat(resposta, token);
            if (saveptr != NULL) {
                strcat(resposta, ";");
            }
        }

        // Use NULL como primeiro argumento para obter os próximos tokens
        token = strtok_r(NULL, ";", &saveptr);
    }

    // Copie a resposta para a variável original
    strcpy(input, resposta);
}


