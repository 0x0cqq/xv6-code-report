# 第二部分：内核的启动与组织

现在我们关注内核的具体的实现。其实，内核也只是一个“程序”，它遵循着硬件的规范与硬件沟通，从而启动、运行等等。

在这一部分，我们将从代码层面解释内核程序的启动，以及启动之后内核如何组织进程和内存。

本部分对应于 xv6 book 的 chapter 2 。

## 启动

我们的第一个程序只能由硬件来唤醒，这里是软硬件的接口。

这里涉及到的文件分别是：`kernel/entry.S`, `kernel/kernel.ld`, `kernel/start.c`, `main.c` 。

### kernel/kernel.ld：链接描述文件


```
OUTPUT_ARCH( "riscv" )
ENTRY( _entry )

SECTIONS
{
  /*
   * ensure that entry.S / _entry is at 0x80000000,
   * where qemu's -kernel jumps.
   */
  . = 0x80000000;

  .text : {
    *(.text .text.*)
    . = ALIGN(0x1000);
    _trampoline = .;
    *(trampsec)
    . = ALIGN(0x1000);
    ASSERT(. - _trampoline == 0x1000, "error: trampoline larger than one page");
    PROVIDE(etext = .);
  }
  /* ... */
}
```

这是链接描述文件，负责在链接环节精细地调整程序的构成。

第一行指明了程序的目标架构是 riscv，第二行则告知链接器，程序的入口是 _entry 标签，链接器会把 _entry 标签所在的代码放置到存放着所有代码的 （.text 段）的最前面。`. = 0x80000000;` 语句树立了一个地址的标签，随后 .text 段就以此地址开始（因此 `_entry` 标签的地址也就是 `0x80000000`），而 `*(.text .text.*)` 就会把编译出来对象文件的 .text 代码段全都放到一起。

QEMU（作为虚拟的硬件设备）指定的程序入口位置为地址为 `0x80000000` 的位置。

因此，当电脑一接上电源，我们的操作系统便会从 _entry 标签开始执行。

不将操作系统入口放在 `0x0` 的地址位置，是因为 QEMU 在 `0x0` 到 `0x80000000` 之间预留了 I/O 设备的“地址”。（事实上，QEMU 模拟的 RAM 的硬件地址就是从 `0x80000000` 开始的，到 `0x86400000` 结束）

值得提醒的是，在启动的时候，页表硬件并没有被启动，所以此时地址并不会经过翻译，我们会直接操作真实的物理内存。（值得提到的是，即使页表之后启动了，内核的虚拟的内存位置和真实的物理内存位置仍然一致，这是“直接映射”，参见 book p35）

### kernel/entry.S：_entry 入口


## 进程组织

## 内存组织

## 参考文献

https://biscuitos.github.io/blog/LD-ENTRY/

https://www.jianshu.com/p/42823b3b7c8e

https://blog.csdn.net/shenjin_s/article/details/88712249