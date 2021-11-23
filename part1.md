# 第一部分：内核向上接口

本部分主要聚焦于内核向上提供的接口，一般也被称为系统调用（system call）。

本部分并不会过度关注系统调用内部的实现，而只是聚焦其实现与效果。

## 定义



```c kernel/syscall.h:1
// System call numbers
#define SYS_fork    1
#define SYS_exit    2
#define SYS_wait    3
#define SYS_pipe    4
#define SYS_read    5
#define SYS_kill    6
#define SYS_exec    7
#define SYS_fstat   8
#define SYS_chdir   9
#define SYS_dup    10
#define SYS_getpid 11
#define SYS_sbrk   12
#define SYS_sleep  13
#define SYS_uptime 14
#define SYS_open   15
#define SYS_write  16
#define SYS_mknod  17
#define SYS_unlink 18
#define SYS_link   19
#define SYS_mkdir  20
#define SYS_close  21
```


```c kernel/syscall.c:86
extern uint64 sys_chdir(void);
extern uint64 sys_close(void);
extern uint64 sys_dup(void);
extern uint64 sys_exec(void);
extern uint64 sys_exit(void);
extern uint64 sys_fork(void);
extern uint64 sys_fstat(void);
extern uint64 sys_getpid(void);
extern uint64 sys_kill(void);
extern uint64 sys_link(void);
extern uint64 sys_mkdir(void);
extern uint64 sys_mknod(void);
extern uint64 sys_open(void);
extern uint64 sys_pipe(void);
extern uint64 sys_read(void);
extern uint64 sys_sbrk(void);
extern uint64 sys_sleep(void);
extern uint64 sys_unlink(void);
extern uint64 sys_wait(void);
extern uint64 sys_write(void);
extern uint64 sys_uptime(void);

static uint64 (*syscalls[])(void) = {
[SYS_fork]    sys_fork,
[SYS_exit]    sys_exit,
[SYS_wait]    sys_wait,
[SYS_pipe]    sys_pipe,
[SYS_read]    sys_read,
[SYS_kill]    sys_kill,
[SYS_exec]    sys_exec,
[SYS_fstat]   sys_fstat,
[SYS_chdir]   sys_chdir,
[SYS_dup]     sys_dup,
[SYS_getpid]  sys_getpid,
[SYS_sbrk]    sys_sbrk,
[SYS_sleep]   sys_sleep,
[SYS_uptime]  sys_uptime,
[SYS_open]    sys_open,
[SYS_write]   sys_write,
[SYS_mknod]   sys_mknod,
[SYS_unlink]  sys_unlink,
[SYS_link]    sys_link,
[SYS_mkdir]   sys_mkdir,
[SYS_close]   sys_close,
};

void
syscall(void)
{
  int num;
  struct proc *p = myproc();

  num = p->trapframe->a7;
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    p->trapframe->a0 = syscalls[num]();
  } else {
    printf("%d %s: unknown sys call %d\n",
            p->pid, p->name, num);
    p->trapframe->a0 = -1;
  }
}
```

在 syscall.h 中，所有的系统调用被赋予编号；而在 syscall.c 中，编号与内核的函数声明通过 syscall() 函数连接起来。

```c kernel/sysproc.c:10
uint64
sys_exit(void)
{ /*...*/ }

uint64
sys_getpid(void)
{ /*...*/ }

uint64
sys_fork(void)
{ /*...*/ }

uint64
sys_wait(void)
{ /*...*/ }

uint64
sys_sbrk(void)
{ /*...*/ }

uint64
sys_sleep(void)
{ /*...*/ }

uint64
sys_kill(void)
{ /*...*/ }

uint64
sys_uptime(void)
{ /*...*/ }
```

```c kernel/sysfile.c:55
uint64
sys_dup(void)
{ /*...*/ }

uint64
sys_read(void)
{ /*...*/ }

uint64
sys_write(void)
{ /*...*/ }

uint64
sys_close(void)
{ /*...*/ }

uint64
sys_fstat(void)
{ /*...*/ }

uint64
sys_link(void)
{ /*...*/ }

uint64
sys_unlink(void)
{ /*...*/ }

uint64
sys_open(void)
{ /*...*/ }

uint64
sys_mkdir(void)
{ /*...*/ }

uint64
sys_mknod(void)
{ /*...*/ }

uint64
sys_chdir(void)
{ /*...*/ }

uint64
sys_exec(void)
{ /*...*/ }

uint64
sys_pipe(void)
{ /*...*/ }
```

在 sysfile.c 和 sysproc.c 中分别给出了系统调用的具体实现。

这些以 sys_ 开头的函数，自身并不执行十分具体的操作。其主要工作包括：
1. 检查（用户）提供给系统调用的参数是否合法
2. 利用内核的相关函数，完成接口语义
3. return 正确的值

下面分为进程和文件两个部分，分别解释 `sysproc.c` 和 `sysfile.c` 中的系统调用的语义和实现。

## 进程部分

### fork

```c kernel/sysproc.c:26
uint64
sys_fork(void)
{
  return fork();
}
```

```c kernel/proc.c:270
// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy user memory from parent to child.
  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  // copy saved user registers.
  *(np->trapframe) = *(p->trapframe);

  // Cause fork to return 0 in the child.
  np->trapframe->a0 = 0;

  // increment reference counts on open file descriptors.
  for(i = 0; i < NOFILE; i++)
    if(p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;

  release(&np->lock);

  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  acquire(&np->lock);
  np->state = RUNNABLE;
  release(&np->lock);

  return pid;
}
```

#### 语义

进程（父进程）调用 `fork` 函数，会新建一个新进程（子进程）。【进程真的会维护父进程的 PID 。】

父进程与子进程完全相同（包括内存，自然也包括指令），但是他们从 `fork` 之后就相互独立，任何修改都不会影响彼此。

`fork` 函数会在调用父进程和子进程中“分别” return ，在父进程返回子进程的 PID，在子进程中返回 0 。借此可以区分子进程和父进程，执行进一步操作。

#### 实现

主要进行了以下操作：

1. 分配一个新的进程表给子进程（通过 allocproc 函数）
2. 复制父进程的内存、寄存器、文件描述符给子进程
3. 设置子进程的返回值为 0 （通过改变 trapframe 的 a0 寄存器，也就是返回值放置的地方）
4. 维护子进程的父指针（parent）为父进程。
5. 标记子进程的状态（state）为可以执行（RUNNABLE）。



### exit

```c kernel/sysproc.c:10
uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}
```


```c kernel/proc.c:336
// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void
exit(int status)
{
  struct proc *p = myproc();

  if(p == initproc)
    panic("init exiting");

  // Close all open files.
  for(int fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd]){
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  acquire(&wait_lock);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  wakeup(p->parent);
  
  acquire(&p->lock);

  p->xstate = status;
  p->state = ZOMBIE;

  release(&wait_lock);

  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
}
```
#### 语义


一个进程调用 `exit(status)`，会使当前进程退出运行，将控制权交还给调度器去进行调度。


惯例上，`status=0` 代表成功， `status=1` 代表失败。

#### 实现

1. 关闭、释放所有打开的文件和已经分配的内存
2. 调整进程父子关系
3. 唤醒父进程
4. 交还控制权给调度器
5. 标记进程状态为死亡（`ZOMBIE`）




## 文件部分