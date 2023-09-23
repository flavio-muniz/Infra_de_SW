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

        while (token != NULL /* && should_run==1 */) {
            trim(token);
            if(strcmp(token,"exit")==0){
                should_run=0;
                break;
            }
            //verificar processo filho
            pid_t kkk = getpid();
            printf("Número do PID [%d]\n", kkk);
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
            token = strtok_r(NULL, ";", &saveptr);
        }
        exit(last_command_status); // Saia do processo filho com o status do último comando        
    } else { // Código a ser executado no processo pai
        int status;
        wait(&status); // Espere pelo término do processo filho
        if (WIFEXITED(status)) { 
            exit(0);
            return WEXITSTATUS(status); // Retorne o status de saída do processo filho      
        } else {
            return 1; // O comando não terminou normalmente
        }
    }
    
}