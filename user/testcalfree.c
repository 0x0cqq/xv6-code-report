#include "kernel/types.h"
#include "user/user.h"
#include "kernel/stat.h"

int main(int argc, char *argv[]) {
    int b = 0;
    int a = calfree();
    int pid = fork();
    if(pid == 0) {
        exit(calfree());
    }
    wait(&b);
    printf("%d %d\n",a,b);
    exit(0);
}
