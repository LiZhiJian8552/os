#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// 筛选质数
void sieve(int pleft[2]){
    // 从左邻居接受数据
    int p;
    read(pleft[0],&p,sizeof(p));
    // 遇到结束标志
    if(p==-1){
        exit(0);
    }
    printf("prime %d\n",p);

    // 创建管道,用于与右邻居通信
    int pright[2];
    pipe(pright);
    if(fork()==0){  //右邻居(子进程)
        close(pright[1]);
        close(pleft[0]);
        sieve(pright);
    }else{
        close(pright[0]);

        int buf;
        while(read(pleft[0],&buf,sizeof(buf))&&buf!=-1){
            if(buf%p!=0){
                write(pright[1],&buf,sizeof(buf));
            }
        }
        // while退出则表示收到了-1
        buf=-1;
        write(pright[1],&buf,sizeof(buf));
        wait(0);
        exit(0);
    }
}


int main(){
    // 创建初始管道
    int input_pipe[2];
    // 初始化管道
    pipe(input_pipe);

    if(fork()==0){  //子进程（右邻居）
        // 子进程不需要像父进程写东西，所以写段要关闭
        close(input_pipe[1]);
        sieve(input_pipe);
        exit(0);
    }else{  //父进程
        // 父进程不需要读，所以关闭管道的读端
        close(input_pipe[0]);
        int i;
        // 向子进程写入数据
        for(i=2;i<=35;i++){
            write(input_pipe[1],&i,sizeof(i));
        }
        // 写入结束标志
        i=-1;
        write(input_pipe[1],&i,sizeof(i));

    }
    // 等待子线程结束
    wait(0);
    exit(0);
}