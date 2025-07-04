#include"kernel/types.h"
#include"kernel/stat.h"
#include"user/user.h"
#include"kernel/fs.h"


void run(char* program,char** args){
    // 创建子线程,在子线程中运行程序
    if(fork()==0){
        exec(program,args);
        exit(0);
    }
    return ;
}

int main(int argc,char* argv[]){
    char buf[2048];
    char* p=buf;
    char* last_p=buf;

    // 字符串指针数组 每个字符串中存储的都是字符串指针
    char* argsbuf[128];
    // 指向上面那个字符串指针数组
    char** args=argsbuf;

    // 将xargs的参数复制到argsbuf中
    // 例子find . b | xargs grep hello
    // agrc[1]中是grep,[2]中是hello
    // find . b的结果通过标准输入给出
    for(int i=1;i<argc;i++){
        *args=argv[i];
        args++;
    }

    // 记录当前参数的位置，如果是上面的例子则是[2]
    char** pa=args;

    // p指向的是buf
    while(read(0,p,1)!=0){
        // 如果是传入多个参数 如 hello text end等，需要换行输出
        if(*p==' '||*p=='\n'){
            char temp=*p;
            *p='\0';

            // 将标准输入传入的参数(以空格划分或者以换行划分的)
            *(pa++)=last_p;

            last_p=p+1;
            if(temp=='\n'){
                *pa=0;
                run(argv[1],argsbuf);
                pa=args;
            }
        }
        // 继续读取数据
        p++;
    }

    // 什么情况下会出现这个种情况
    if(pa!=args){
        *p='\0';
        *(pa++)=last_p;
        *pa=0;
        run(argv[1],argsbuf);
    }
    while(wait(0)!=-1){}
    exit(0);
}
