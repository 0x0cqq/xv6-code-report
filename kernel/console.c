//
// Console input and output, to the uart.
// Reads are line at a time.
// Implements special input characters:
//   newline -- end of line
//   control-h -- backspace
//   control-u -- kill line
//   control-d -- end of file
//   control-p -- print process list
//

#include <stdarg.h>

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"

#define BACKSPACE 0x100
#define C(x)  ((x)-'@')  // Control-x

//
// send one character to the uart.
// called by printf, and to echo input characters,
// but not from write().
//
void
consputc(int c)
{
  if(c == BACKSPACE){
    // if the user typed backspace, overwrite with a space.
    uartputc_sync('\b'); uartputc_sync(' '); uartputc_sync('\b');
  } else {
    uartputc_sync(c);
  }
}

//
// user write()s to the console go here.
//
int
consolewrite(int user_src, uint64 src, int n)
{
  int i;

  for(i = 0; i < n; i++){
    char c;
    if(either_copyin(&c, user_src, src+i, 1) == -1)
      break;
    uartputc(c);
  }

  return i;
}

//
// user read()s from the console go here.
// copy (up to) a whole input line to dst.
// user_dist indicates whether dst is a user
// or kernel address.
//
int
consoleread(int user_dst, uint64 dst, int n)
{
  uint nowread = 0;
  int c;
  char cbuf;
  while(nowread < n){

    c = consolegetc();

    switch(c){
    case C('P'):  // Print process list.
      procdump();
      break;
    default:
      if(c != 0){
        c = (c == '\r') ? '\n' : c;
        // echo back to the user.
        consputc(c);
        cbuf = c;
        if(either_copyout(user_dst, dst + nowread, &cbuf, 1) == -1)
          break;
        nowread++;
        if(c == '\n' || c == C('D')){
          printf("return! %d", nowread);
          return nowread;
        }
      }
    }
  }
  return nowread;
}

// get a char from...
int
consolegetc()
{
  int c = uartgetc();
  while(c == -1){
    c = uartgetc();
  }
  return c;

}

void
consoleinit(void)
{

  uartinit();

  // connect read and write system calls
  // to consoleread and consolewrite.
  devsw[CONSOLE].read = consoleread;
  devsw[CONSOLE].write = consolewrite;
}
