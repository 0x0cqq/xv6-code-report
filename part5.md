# 第五部分：中断与设备驱动

驱动（driver）是操作系统软件的一部分，负责提供与某个设备的连接。

一般来说，（IO）设备产生、输出数据的速度要远远地低于 CPU 处理数据的速度，所以进程并不会以轮询的方式去等待 IO 设备的输入，这样的效率很低。进程如果读不到数据就进入阻塞状态，等待设备获取、处理完读入后释放一个中断信号。

硬件接收到中断后，会通过设置 pc 为某个特定的寄存器的值，跳转到中断处理程序处。[^1]

[^1]: 在 RISC-V 真正的处理中，“中断”和异常被统称为 trap ，所以之后我们也会看到许多函数的名称中都有 “trap” 。

本部分对应于 xv6 book 的 chapter 5。

## 设备中断

外接设备中断也是 Traps 的一部分，可能是用户态的中断，也可能是内核态的中断。无论是何种中断（除了下文提及的machine mode 中断），RISC-V 的硬件在接收到中断信号后，都会跳转到 stvec 寄存器的地址处继续运行程序。

在用户态中，stvec 会被设置为 `uservec` 的位置，位于 `kernel/trampoline.S:16`；而在内核态中，stvec 会被设置为 `kernel/kernelvec.S:10` 的位置。[^2]

[^2]: 内核态切换回用户态在 usertrapret 函数中完成，其中有 `w_stvec(TRAMPOLINE + (uservec - trampoline));` 和 `p->trapframe->kernel_trap = (uint64)usertrap;` 两则语句将 stvec 和 kernel_trap 两则设置到正确的地址；在 `trapinithart` 函数，`usertrap` 函数中。


## 时钟中断

RISC-V 的硬件接收到时钟中断（是 machine mode 模式的中断）信号后，会跳转到 mtvec 寄存器的地址处继续运行程序。

```c kernel/start.c:80
  // set the machine-mode trap handler.
  w_mtvec((uint64)timervec);
```

http://www.databusworld.cn/10468.html

