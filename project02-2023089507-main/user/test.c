#include "kernel/types.h"
#include "user/user.h"
#include "user/thread.h"

void thread_func(void *arg1, void *arg2)
{
    for(int i = 0; i < 3; i++) printf("GOOD THREAD\n");
    exit(0);
}

void legend_while(){
    printf("into while\n");
    while(1);
    printf("DO NOT PRINT");
}


int
main(int argc, char *argv[])
{
    thread_create(thread_func, 0, 0);
    while(thread_join() > 0); //기본적인 clone, join 동작 확인
    thread_join(); //thread없을 때 동작확인
    int pid = thread_create(legend_while, 0, 0);
    printf("CREATE LEGEND WHILE THREAD: %d\n", pid);
    kill(pid);
    while(thread_join() > 0);
    printf("FIN WHILE DO NOT PRINT");
    return 0;
}