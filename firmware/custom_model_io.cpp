#include <Arduino.h>
#include <MicroTFLite.h>

#include "custom_model_io.h"

extern TfLiteTensor *tflInputTensor;
extern TfLiteTensor *tflOutputTensor;
extern float tflInputScale;
extern int32_t tflInputZeroPoint;


bool model_set_input(int8_t input_value, int index) {
  int maxSize = 1;
  for (int i = 0; i < tflInputTensor->dims->size; i++)
    maxSize *= tflInputTensor->dims->data[i];
  if (tflInputTensor == nullptr || index >= maxSize) {
    Serial.print(F("Input tensor index out of range."));
    return false;
  }

  if (tflInputTensor->type == kTfLiteInt8) {
    tflInputTensor->data.int8[index] = input_value;
  } else {
    Serial.println(F("Unsupported input tensor type (not int8_t)."));
    return false;
  }

  return true;
}

int8_t model_get_output(int index) {
  if (tflOutputTensor == nullptr || index >= tflOutputTensor->dims->data[1]) {
    Serial.println(F("Output tensor index out of range!"));
    return -1;
  }

  if (tflOutputTensor->type == kTfLiteInt8) {
    int8_t output_value = tflOutputTensor->data.int8[index];

    return output_value;
  } else {
    Serial.println(F("Unsupported output tensor type (not int8_t)."));
    return -1;
  }
}
