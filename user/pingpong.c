#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


int main(){
    // 创建两个管道：pp2c 用于父进程到子进程的通信，pc2p用于子进程到父进程的通信
    int pp2c[2],pc2p[2];
    pipe(pp2c);
    pipe(pc2p);

    if(fork()!=0){
        // 向管道中写入数据
        write(pp2c[1],".",1);
        // 关闭写段
        close(pp2c[1]);

        // 父进程从子进程读取一个字符
        char buf;
        read(pc2p[0],&buf,1);
        printf("%d: received pong\n",getpid());
        // 等待子进程结束
        wait(0);
    }else{
        // 从管道中读取一个字符
        char buf;
        read(pp2c[0],&buf,1);
        printf("%d: received ping\n",getpid());

        // 向另一个管道中写入数据
        write(pc2p[1],&buf,1);
        close(pc2p[1]);
    }
    // 关闭读端
    close(pp2c[0]);
    close(pc2p[0]);

    exit(0);
}