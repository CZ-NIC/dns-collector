#include "lib/lib.h"

int main(void)
{
  log_init("test");
  msg(L_INFO, "Hoooot!");
  return 0;
}
