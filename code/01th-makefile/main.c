#include <stdio.h>
#include "add.h"
#include "sub.h"

int a = 20;
int b = 10;

int main(int argc, char const *argv[])
{
    /* code */
    printf("main\r\n");
    printf("%d+%d=%d\r\n", a, b, add(a, b));
    printf("%d-%d=%d\r\n", a, b, sub(a, b));
    return 0;
}
