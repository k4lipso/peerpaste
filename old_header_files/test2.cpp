#include <string>
#include <iostream>

int main(int argc, char *argv[])
{
    int a(0);


    int &b = a;
    b++;
    ++b;

    int *c;
    c = &a;
    *c = 3;


    return 0;
}
