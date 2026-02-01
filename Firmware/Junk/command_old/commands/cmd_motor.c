#include "cli.h"
#include <stdio.h>
#include <stdlib.h>

extern void Motor_SetPWM(float duty);

int cmd_pwm(int argc, char **argv) {
    if (argc < 2) {
        printf("Current PWM: [read actual value here]\r\n");
        return 0;
    }
    
    float duty = atof(argv[1]);
    if (duty < 0.0f || duty > 100.0f) {
        printf("Invalid: %f. Use 0-100\r\n", duty);
        return 1;
    }
    
    Motor_SetPWM(duty / 100.0f);
    printf("PWM set to %.1f%%\r\n", duty);
    return 0;
}