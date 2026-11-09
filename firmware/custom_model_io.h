#ifndef CUSTOM_MODEL_IO_H
#define CUSTOM_MODEL_IO_H

#include <Arduino.h>

bool model_set_input(int8_t input_value, int index);
int8_t model_get_output(int index);


#endif