# Chapter 1: Operating system interfaces

第一章阐述了 xv6 操作系统向上展示的基本接口，包括：进程、内存、文件描述符、管道和文件系统，以及终端是如何使用它们的。

需要提到的是，终端（Shell）只是一个普通的用户程序，运行在用户空间中，并不具有特殊的（内核）权限。终端只是通过系统调用(system call) 的方式与内核互动，因此，其他的用户程序也可以通过类似的方式来使用 xv6 的内核。

## 进程和内存

现代操作系统最核心的任务，就是以分时的形式"同时"运行多个应用程序（进程）。

### 进程

以下的函数大多定义于 `kernel/proc.c` 。

#### `int fork()`

位置：`kernel/proc.c:270`

```c
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

进程（父进程）调用 `fork` 函数，会新建一个新进程（子进程）。进程真的会维护父进程的 PID 。

父进程与子进程完全相同（包括内存，自然也包括指令），但是他们从 `fork` 之后就相互独立，任何修改都不会影响彼此。

`fork` 函数会在调用父进程和子进程中“分别” return ，在父进程返回子进程的 PID，在子进程中返回 0 。借此可以区分子进程和父进程，执行进一步操作

#### `int exit(int status)`

位置：`kernel/proc.c:336`

```c
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


一个进程调用 `exit(status)`，会使当前进程退出运行，将控制权交还给 scheduler 去进行调度。

惯例上，`status=0` 代表成功， `status=1` 代表失败。

进程调用 `exit` 之后，操作系统需要执行的操作有：

+ 关闭所有打开的文件
+ 释放已经分配的内存
+ 调整进程父子关系
+ 唤醒父进程

#### `int wait(int * status)`

位置：`kernel/proc.c:381`

```c
// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(uint64 addr)
{
  struct proc *np;
  int havekids, pid;
  struct proc *p = myproc();

  acquire(&wait_lock);

  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(np = proc; np < &proc[NPROC]; np++){
      if(np->parent == p){
        // make sure the child isn't still in exit() or swtch().
        acquire(&np->lock);

        havekids = 1;
        if(np->state == ZOMBIE){
          // Found one.
          pid = np->pid;
          if(addr != 0 && copyout(p->pagetable, addr, (char *)&np->xstate,
                                  sizeof(np->xstate)) < 0) {
            release(&np->lock);
            release(&wait_lock);
            return -1;
          }
          freeproc(np);
          release(&np->lock);
          release(&wait_lock);
          return pid;
        }
        release(&np->lock);
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || p->killed){
      release(&wait_lock);
      return -1;
    }
    
    // Wait for a child to exit.
    sleep(p, &wait_lock);  //DOC: wait-sleep
  }
}
```

进程调用 `wait` 函数，会等待**一个**子进程退出运行（exit）或被强制停止运行（kill）（如果没有子进程会返回 -1），将子进程返回的状态放到调用参数给出的 `status` 地址上。

#### `int exec(char *file, char *argv[])`

位置：`kernel/exec.c:12`

```c
int
exec(char *path, char **argv)
{
  char *s, *last;
  int i, off;
  uint64 argc, sz = 0, sp, ustack[MAXARG], stackbase;
  struct elfhdr elf;
  struct inode *ip;
  struct proghdr ph;
  pagetable_t pagetable = 0, oldpagetable;
  struct proc *p = myproc();

  begin_op();

  if((ip = namei(path)) == 0){
    end_op();
    return -1;
  }
  ilock(ip);

  // Check ELF header
  if(readi(ip, 0, (uint64)&elf, 0, sizeof(elf)) != sizeof(elf))
    goto bad;
  if(elf.magic != ELF_MAGIC)
    goto bad;

  if((pagetable = proc_pagetable(p)) == 0)
    goto bad;

  // Load program into memory.
  for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){
    if(readi(ip, 0, (uint64)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
    if(ph.type != ELF_PROG_LOAD)
      continue;
    if(ph.memsz < ph.filesz)
      goto bad;
    if(ph.vaddr + ph.memsz < ph.vaddr)
      goto bad;
    uint64 sz1;
    if((sz1 = uvmalloc(pagetable, sz, ph.vaddr + ph.memsz)) == 0)
      goto bad;
    sz = sz1;
    if((ph.vaddr % PGSIZE) != 0)
      goto bad;
    if(loadseg(pagetable, ph.vaddr, ip, ph.off, ph.filesz) < 0)
      goto bad;
  }
  iunlockput(ip);
  end_op();
  ip = 0;

  p = myproc();
  uint64 oldsz = p->sz;

  // Allocate two pages at the next page boundary.
  // Use the second as the user stack.
  sz = PGROUNDUP(sz);
  uint64 sz1;
  if((sz1 = uvmalloc(pagetable, sz, sz + 2*PGSIZE)) == 0)
    goto bad;
  sz = sz1;
  uvmclear(pagetable, sz-2*PGSIZE);
  sp = sz;
  stackbase = sp - PGSIZE;

  // Push argument strings, prepare rest of stack in ustack.
  for(argc = 0; argv[argc]; argc++) {
    if(argc >= MAXARG)
      goto bad;
    sp -= strlen(argv[argc]) + 1;
    sp -= sp % 16; // riscv sp must be 16-byte aligned
    if(sp < stackbase)
      goto bad;
    if(copyout(pagetable, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
      goto bad;
    ustack[argc] = sp;
  }
  ustack[argc] = 0;

  // push the array of argv[] pointers.
  sp -= (argc+1) * sizeof(uint64);
  sp -= sp % 16;
  if(sp < stackbase)
    goto bad;
  if(copyout(pagetable, sp, (char *)ustack, (argc+1)*sizeof(uint64)) < 0)
    goto bad;

  // arguments to user main(argc, argv)
  // argc is returned via the system call return
  // value, which goes in a0.
  p->trapframe->a1 = sp;

  // Save program name for debugging.
  for(last=s=path; *s; s++)
    if(*s == '/')
      last = s+1;
  safestrcpy(p->name, last, sizeof(p->name));
    
  // Commit to the user image.
  oldpagetable = p->pagetable;
  p->pagetable = pagetable;
  p->sz = sz;
  p->trapframe->epc = elf.entry;  // initial program counter = main
  p->trapframe->sp = sp; // initial stack pointer
  proc_freepagetable(oldpagetable, oldsz);

  return argc; // this ends up in a0, the first argument to main(argc, argv)

 bad:
  if(pagetable)
    proc_freepagetable(pagetable, sz);
  if(ip){
    iunlockput(ip);
    end_op();
  }
  return -1;
}
```

`fork` 函数虽然可以创建新进程，但只能创建“子进程”，功能受限。

进程调用 `exec` 函数，会将当前进程程序替换为 `path` 指定的文件的程序（也包括内存等）。

指定的文件必须具有 ELF 格式。

`exec` 函数失败会返回 -1；`exec` 函数成功执行的话并不会返回，而是会从 ELF 文件头中找到起始地址，“继续”运行。

`exec` 命令也支持携带参数。


## 输入/输出 与文件描述符

现在我们有了进程，有了程序。程序基本的功能是输入和输出。

操作系统为输入输出功能提供了文件描述符的功能，其抽象掉了具体的对象，只是代表一个具有读/写功能的对象。文件描述符表示的读/写的具体对象可以是：文件、屏幕、管道、设备等等。

### 文件描述符

用一个整数代表一个文件描述符。

### `read`

### `close` 

### `dup`


## 管道

管道是在内核中实现的一个小缓冲区，会给用户进程提供一对两个文件描述符，一个用来读，一个用来写。

管道提供了一种进程间通信的方法。

### pipe 函数

