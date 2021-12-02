#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    int ticks_before = uptime();
    int p_1[2], p_2[2];
    char a[2]; // read and write buffers
    pipe(p_1);
    pipe(p_2);
    a[0] = 'a';
    write(p_1[1], a, 1);
    close(p_1[1]);
    int pid = fork();
    if(pid == 0) { // new process
        read(p_1[0], a, 1);
        close(p_1[1]);
        close(p_1[0]);
        printf("p_1:%d %d\n",p_1[0],p_1[1]);
        write(p_2[1], a, 1);
        close(p_2[1]);
        exit(0);
    } 
    // parent process
    read(p_2[0], a, 1);
    close(p_2[1]);
    close(p_2[0]);
    printf("finished\n");
    wait(0);
    int ticks_after = uptime();
    int ticks_diff = ticks_after - ticks_before;
    printf("%d\n", ticks_diff);
    exit(0);
}