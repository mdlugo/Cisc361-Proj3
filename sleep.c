#include <stdio.h>

int main()
{
  int i = 0;
  while (1) {
    printf("child is asleep for 5 seconds...\n");
    sleep(5);
  }
  return 17;
}
