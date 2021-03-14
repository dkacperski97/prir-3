#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#define BUFFOR_SIZE 80

int fork_id;
key_t range_key = 1001, vector_key = 1002, result_key = 1003;

pid_t * create_forks(int children_number) {
    pid_t fork_pid;
    pid_t *fork_pids = (pid_t *)malloc(children_number * sizeof(pid_t));

    for (int i = 0; i < children_number; i++)
    {
        fork_pid = fork();
        if (fork_pid == -1)
        {
            perror("fork");
            exit(1);
        }
        if (fork_pid == 0)
        {
            printf("Fork PID = %d\n", getpid());
            fork_id = i;
            return NULL;
        }
        fork_pids[i] = fork_pid;
    }

    return fork_pids;
}

double sum(double* vector, int n) {
	int i;
	double sum = 0.0f;
    double* v = vector;
	for(i=0; i<n; i++) {
		sum += *v;
        v++;
	}
	return sum;
}

void on_usr1(int signal) {
    int range_shmid, vector_shmid, result_shmid;
    int *range;
	double *vector, *result;
    printf("USR1 received. PID = %d\n", getpid());

    if ((range_shmid = shmget(range_key, (fork_id + 1) * 2 * sizeof(int), IPC_CREAT | 0666)) < 0) {
        perror("range shmget");
        exit(1);
    }
    if ((range = shmat(range_shmid, NULL, 0)) == (int *) -1) {
        perror("range shmat");
        exit(1);
    }

    if ((vector_shmid = shmget(vector_key, range[(2 * fork_id) + 1] * sizeof(double), IPC_CREAT | 0666)) < 0) {
        perror("vector shmget");
        exit(1);
    }
    if ((vector = shmat(vector_shmid, NULL, 0)) == (double *) -1) {
        perror("vector shmat");
        exit(1);
    }

    if ((result_shmid = shmget(result_key, (fork_id + 1) * sizeof(double), IPC_CREAT | 0666)) < 0) {
        perror("result shmget");
        exit(1);
    }
    if ((result = shmat(result_shmid, NULL, 0)) == (double *) -1) {
        perror("vector shmat");
        exit(1);
    }

    for (int i = 0; i < 10000000; i++) {
        result[fork_id] = sum(vector + range[2*fork_id], range[2*fork_id+1] - range[2*fork_id] + 1);
    }
    printf("Sum calculated by %d, range %d - %d (%d) = %f\n",
            getpid(), range[2*fork_id], range[2*fork_id+1], range[2*fork_id+1] - range[2*fork_id] + 1,
            result[fork_id]);
    
    if (shmdt(range) < 0){
        perror("range shmdt");
        exit(1);
    }
    if (shmdt(vector) < 0){
        perror("vector shmdt");
        exit(1);
    }
    if (shmdt(result) < 0){
        perror("result shmdt");
        exit(1);
    }

    exit(0);
}


int main(int argc, char **argv) { 
    if (argc != 2) {
        printf("argc %d\n", argc);
        return 1;
    }
	int children_number = atoi(argv[1]);
    pid_t *fork_pids = create_forks(children_number);

    if (fork_pids == NULL) {
        sigset_t mask;
        struct sigaction usr1;
        sigemptyset(&mask);
        usr1.sa_handler = (&on_usr1);
        usr1.sa_mask = mask;
        usr1.sa_flags = SA_SIGINFO;
        sigaction(SIGUSR1, &usr1, NULL);

        while(1)
            pause();
    } else {
        FILE* f = fopen("vector.dat", "r");
        char buffor[BUFFOR_SIZE+1];
        int n, i, range_shmid, vector_shmid, result_shmid;
        int *range;
	    double *vector, *result;

        fgets(buffor, BUFFOR_SIZE, f);
        n = atoi(buffor);
        printf("Vector has %d elements\n", n);

        if ((vector_shmid = shmget(vector_key, n * sizeof(double), IPC_CREAT | 0666)) < 0) {
            perror("vector shmget");
            exit(1);
        }

        if ((vector = shmat(vector_shmid, NULL, 0)) == (double *) -1) {
            perror("vector shmat");
            exit(1);
        }

        for(i=0; i<n; i++) {
            fgets(buffor, BUFFOR_SIZE, f);
            vector[i] = atof(buffor);
        }
        fclose(f);

        if (shmdt(vector) < 0){
            perror("vector shmdt");
            exit(1);
        }

        if ((range_shmid = shmget(range_key, children_number * 2 * sizeof(int), IPC_CREAT | 0666)) < 0) {
            perror("range shmget");
            exit(1);
        }

        if ((range = shmat(range_shmid, NULL, 0)) == (int *) -1) {
            perror("range shmat");
            exit(1);
        }

        int size, rest = n % children_number;

        for (int r = 0; r < children_number; r++) {
            size = (n / children_number) + (rest > 0 && r < rest ? 1 : 0);
            range[2*r] = r == 0 ? 0 : range[2*r-1] + 1;
            range[2*r+1] = range[2*r] + size - 1;
        }

        if (shmdt(range) < 0){
            perror("range shmdt");
            exit(1);
        }

        if ((result_shmid = shmget(result_key, children_number * sizeof(double), IPC_CREAT | 0666)) < 0) {
            perror("result shmget");
            exit(1);
        }

        for (i = 0; i < children_number; i++) {
            kill(fork_pids[i], SIGUSR1);
        }
        
        for (i = 0; i < children_number; i++) {
            if (wait(0) < 0)
            {
                perror("wait");
                exit(1);
            }
        }

        if ((result = shmat(result_shmid, NULL, 0)) == (double *) -1) {
            perror("result shmat");
            exit(1);
        }

        printf("The sum of the elements in the vector = %f\n", sum(result, children_number));

        if (shmdt(result) < 0){
            perror("result shmdt");
            exit(1);
        }

        if (shmctl(range_shmid, IPC_RMID, 0) == -1) {
            perror("range shmctl");
        }
        if (shmctl(vector_shmid, IPC_RMID, 0) == -1) {
            perror("vector shmctl");
        }
        if (shmctl(result_shmid, IPC_RMID, 0) == -1) {
            perror("result shmctl");
        }

        return 0;
    }
}