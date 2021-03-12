/* Executes and waits for a single child process. */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

void
main (void) 
{
    exit(-1);
  wait (exec ("child-simple"));
}
