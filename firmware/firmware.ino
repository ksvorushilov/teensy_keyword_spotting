#include <MicroTFLite.h>  // Include the wrapper library
#include <arm_math.h>     // cmsisdsp
#include <Audio.h>        // teensy audio library
#include <U8g2lib.h>      // screen


#include "custom_buffer.h"
#include "custom_model_io.h"
#include "model_data.h"  //  model array
#include "melspec.h"

#define DEBUG_MODE 0

#define INPUT_SR 44100
#define TARGET_SR 8000
#define I2S_BIT_WIDTH 16

#define INPUT_SCALE_BITS -7  // which means input should be divided by 128
#define INPUT_ZERO_POINT -127

// two buffers for effective frame hop handling
#define I2S_BUFFER_LEN 128
buffer_t sample_buffer1;
buffer_t sample_buffer2;
buffer_t *current_sample_buffer;
DMAMEM q15_t holdout_data[I2S_BUFFER_LEN] = { 0 };
bool sample_buffer_full;

#define SPEC_BUFFER_LEN 240
#define SPEC_BUFFER_WIDTH (SPEC_BUFFER_LEN / N_MELS)
buffer_t spec_buffer1;
buffer_t spec_buffer2;
buffer_t *current_spec_buffer;
DMAMEM q15_t holdout_melspec[SPEC_BUFFER_LEN] = { 0 };
bool spec_buffer_full;

// cmsisdsp helper struct
melspec_t melspec_obj;

// tflite stuff
constexpr int kTensorArenaSize = 32 * 1024;  // size of buffer for model to run
uint8_t tensorArena[kTensorArenaSize];       // buffer itself
#define NN_INPUT_SIZE 480                    // 16 mels x 30 frames

// reconstruction filter for downsampler
#define RECON_FILTER_LEN 64
int16_t reconstruction_fir_coefs[] = { -21, -28, -28, -19, 2, 31, 60, 79, 72,
                                       30, -44, -130, -198, -211, -143, 7, 207, 395,
                                       491, 428, 177, -226, -679, -1029, -1107, -778, 13,
                                       1209, 2646, 4077, 5230, 5872, 5872, 5230, 4077, 2646,
                                       1209, 13, -778, -1107, -1029, -679, -226, 177, 428,
                                       491, 395, 207, 7, -143, -211, -198, -130, -44,
                                       30, 72, 79, 60, 31, 2, -19, -28, -28,
                                       -21 };

// audio library tools
AudioInputI2S i2s1;
AudioFilterFIR recon_fir;
AudioRecordQueue queue;
AudioConnection patchCord1(i2s1, 0, recon_fir, 0);
AudioConnection patchCord2(recon_fir, queue);

// display
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);

void setup() {
  // uart
  Serial.begin(115200);  // baud ignored in Teensy, always high-speed

  // inference engine
  Serial.println("Init model...");
  if (!ModelInit(mymodel_tflite, tensorArena, kTensorArenaSize)) {
    Serial.println("Error: Model initialization failed!");
    for (;;) {
      ;  // endless loop
    }
  }
  Serial.println("Model init ok.");

  // initialise buffers
  initialise_buffers();

  // initialise melspec stuff
  melspec_init(&melspec_obj);

  // init display
  u8g2.begin();
  u8g2.setFont(u8g2_font_cu12_tr);  //_font_cu12_tr

  // run filter and qudio queue
  AudioMemory(32);
  recon_fir.begin(reconstruction_fir_coefs, RECON_FILTER_LEN);
  queue.begin();
}

void loop() {
  bufferise_decimated();
  if (sample_buffer_full) {
    sample_buffer_full = false;
    calculate_spec();
    // delay(500);
  }
  if (spec_buffer_full) {
    spec_buffer_full = false;
    int8_t infer = inference();
    Serial.println(infer);
    if (infer > 102)  // scale 1/256, bias 128, formula (x + 128) / 256;
                      // 0.9 ~ (128 + 128 - 26)/256 => x = 128 - 26 = 102
    {


      queue.end();  //stop receiving i2s data
      recon_fir.end();


      echo_state();  // output display message

      initialise_buffers();
      // Clear any queued audio
      while (queue.available() > 0) {
        queue.readBuffer();
        queue.freeBuffer();
      }
      delay(900);
      clear_screen();
      delay(100);  // settle down from voltage spikes created by display
      
      recon_fir.begin(reconstruction_fir_coefs, RECON_FILTER_LEN);
      queue.begin();
    }
  }
}

void bufferise_decimated() {

  if (queue.available() >= 2) {
    // define i2s buffer to read values to
    q15_t i2s_buffer[I2S_BUFFER_LEN * 2];
    // Read a 256-byte block
    memcpy(i2s_buffer, queue.readBuffer(), sizeof(q15_t) * I2S_BUFFER_LEN);
    queue.freeBuffer();
    // Read another 256-byte block
    memcpy(i2s_buffer + I2S_BUFFER_LEN, queue.readBuffer(), sizeof(q15_t) * I2S_BUFFER_LEN);
    queue.freeBuffer();

    // decimate to buffer
    uint32_t input_step_precise = (INPUT_SR << 4) / TARGET_SR;                                  // << 4 for better precision
    for (uint32_t accum = 0; (accum >> 4) < I2S_BUFFER_LEN * 2; accum += input_step_precise) {  // accum contains next sample number
      if (buffer_write(current_sample_buffer, i2s_buffer[accum >> 4])) {                        // which means if current buffer becomes full
        sample_buffer_full = true;
        current_sample_buffer = current_sample_buffer == &sample_buffer1 ? &sample_buffer2 : &sample_buffer1;
        memcpy(holdout_data, current_sample_buffer->data, sizeof(q15_t) * I2S_BUFFER_LEN);  // 128 16-bit samples
        buffer_init(current_sample_buffer, I2S_BUFFER_LEN);
      }
    }
  }
}

void calculate_spec() {
  // holdout data = previous frame. sample buffer which is not current is current frame
  q15_t spec_input[FFT_LEN];
  // copy from both available buffers, full one and holdout one.
  memcpy(spec_input, holdout_data, sizeof(q15_t) * FFT_LEN / 2);
  buffer_t *nonactive_buffer = current_sample_buffer == &sample_buffer1 ? &sample_buffer2 : &sample_buffer1;
  memcpy(&spec_input[FFT_LEN / 2], nonactive_buffer->data, sizeof(q15_t) * FFT_LEN / 2);

  // calculate melspec and write it to buffer
  q15_t spec_output[N_MELS];
  melfft_q15(&melspec_obj, spec_input, spec_output);
  for (int i = 0; i < N_MELS; i++) {
    if (buffer_write(current_spec_buffer, spec_output[i])) {  // if buffer is full
      spec_buffer_full = true;
      current_spec_buffer = current_spec_buffer == &spec_buffer1 ? &spec_buffer2 : &spec_buffer1;
      memcpy(holdout_melspec, current_spec_buffer->data, sizeof(q15_t) * SPEC_BUFFER_LEN);  // 128 16-bit samples
      buffer_init(current_spec_buffer, SPEC_BUFFER_LEN);
    }
  }
}

int8_t inference() {
  // neural network inference function

  // copy signal to input
  for (uint32_t i = 0; i < NN_INPUT_SIZE / 2; i++) {
    q15_t quantised_value;
    arm_shift_q15(&holdout_melspec[i], INPUT_SCALE_BITS, &quantised_value, 1);
    quantised_value += INPUT_ZERO_POINT;

    uint32_t next_index = 2 * SPEC_BUFFER_WIDTH * (i % N_MELS) + i / N_MELS;  // tensor transposition for data compatibility

    model_set_input((int8_t)quantised_value, next_index);
  }

  buffer_t *nonactive_melspec_buffer = current_spec_buffer == &spec_buffer1 ? &spec_buffer2 : &spec_buffer1;
  for (uint32_t i = 0; i < NN_INPUT_SIZE / 2; i++) {
    q15_t quantised_value;
    arm_shift_q15(&nonactive_melspec_buffer->data[i], INPUT_SCALE_BITS, &quantised_value, 1);
    quantised_value += INPUT_ZERO_POINT;

    uint32_t j = i + NN_INPUT_SIZE / 2;  // same tensor transposition but with shift
    uint32_t next_index = 2 * SPEC_BUFFER_WIDTH * (j % N_MELS) + j / N_MELS;
    model_set_input((int8_t)quantised_value, next_index);
  }

#if DEBUG_MODE
  unsigned long startTime = micros();
#endif
  bool inf_status = ModelRunInference();  // the actual inference
#if DEBUG_MODE
  unsigned long endTime = micros();
#endif
  if (!inf_status) {
    Serial.println("Inference Failed!");
    delay(1000);
    return 0;
  }
#if DEBUG_MODE
  Serial.print("Time in us:");
  Serial.println(endTime - startTime);
#endif

  int8_t probability = model_get_output(0);  // function from MicroTFLite rewritten to output int8_t without dequantising
  return probability;
}

void initialise_buffers(void) {
  buffer_init(&sample_buffer1, I2S_BUFFER_LEN);
  buffer_flush(&sample_buffer1);

  buffer_init(&sample_buffer2, I2S_BUFFER_LEN);
  buffer_flush(&sample_buffer2);

  current_sample_buffer = &sample_buffer1;
  sample_buffer_full = false;

  buffer_init(&spec_buffer1, SPEC_BUFFER_LEN);
  buffer_flush(&spec_buffer1);

  buffer_init(&spec_buffer2, SPEC_BUFFER_LEN);
  buffer_flush(&spec_buffer2);

  current_spec_buffer = &spec_buffer1;
  spec_buffer_full = false;
  clear_holdout_buffers();
}

void echo_state(void) {

  u8g2.clearBuffer();
  u8g2.drawStr(40, 40, "Hi! :)");
  u8g2.sendBuffer();
}

void clear_screen(void) {

  u8g2.clearBuffer();
  u8g2.sendBuffer();
}

FLASHMEM void clear_holdout_buffers() {
  memset(holdout_data, 0, sizeof(holdout_data));
  memset(holdout_melspec, 0, sizeof(holdout_melspec));
}
