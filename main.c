#include <stdio.h>

#include "menu.h"

int main(int argc, char *argv[], char **penv)
{
    // printf("int - %lu, float - %lu, double - %lu, uint32_t - %lu\r\n", sizeof(int), sizeof(float), sizeof(double), sizeof(uint32_t));
    Menu_Init();
    
    return 0;
}