#include <stdio.h>
#include <stdint.h>

int64_t buffer = 0xdeadbeefdeadbeef;

uint64_t hello(int i)
{
  printf("Hello, %d\n", i);
  fflush(stdout);
  return i + 1;
}

uint64_t print_buffer()
{
  printf("0x%016lx\n", buffer);
  fflush(stdout);
  return 1;
}
