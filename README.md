# Keyword Spotting on Teensy 4.1

This project implements an always-on keyword spotter for the word **“Hi”** on an Arduino-compatible Teensy 4.1 board (i.MX RT1062). It includes a TensorFlow training and evaluation notebook, fixed-point audio feature extraction, and firmware that runs a fully integer-quantized model using MicroTFLite.

The training data targets one speaker and contains positive recordings with variations in voice and microphone position, plus negative speech, similar-sounding words, noise, and silence. The firmware listens to an I2S MEMS microphone and displays a detection on an SH1106 OLED.

## Project contents

- [`research/tf_train_and_save.ipynb`](research/tf_train_and_save.ipynb) — data preparation, CMSIS-DSP feature extraction, TensorFlow QAT training, evaluation, and full-int8 TFLite conversion.
- [`firmware/firmware.ino`](firmware/firmware.ino) — Teensy audio capture, resampling, feature extraction, inference, and display logic.
- [`firmware/melspec.c`](firmware/melspec.c) and [`firmware/melspec.h`](firmware/melspec.h) — embedded CMSIS-DSP mel-spectrogram implementation.
- [`firmware/model_data.h`](firmware/model_data.h) — quantized model data compiled into the firmware.
- [`firmware/custom_model_io.cpp`](firmware/custom_model_io.cpp) and [`firmware/custom_model_io.h`](firmware/custom_model_io.h) — int8 model input and output accessors.
- [`report.pdf`](report.pdf) — project report and additional implementation details.

## Audio and model pipeline

1. Audio is captured as 16-bit PCM at 8 kHz for training and evaluation. The firmware receives I2S audio at 44.1 kHz and downsamples it to 8 kHz with a symmetric FIR filter.
2. The signal is divided into 256-sample frames with 50% overlap. A Hamming window, FFT power spectrum, and 16-bin mel filter bank produce fixed-point Q15 features using CMSIS-DSP.
3. Each model input contains 30 consecutive feature frames (approximately 0.5 seconds). The compact CNN has three convolution/pooling stages followed by dense layers and a sigmoid output.
4. The notebook trains with Adam and binary cross-entropy, using quantization-aware training (QAT) for 100 epochs and saving checkpoints every 10 epochs.
5. The QAT model is converted to a full-integer int8 TFLite model. The firmware stores model data in `model_data.h` and uses an int8 input/output interface.

## Train and evaluate

The notebook uses Python and the following packages:

- TensorFlow
- TensorFlow Model Optimization (`tensorflow-model-optimization`)
- CMSIS-DSP Python bindings (`cmsisdsp`)
- NumPy, SciPy, scikit-learn, and Matplotlib


From the `research` directory, install the dependencies in your Python environment and launch Jupyter:

```shell
python -m pip install numpy scipy tensorflow tensorflow-model-optimization scikit-learn cmsisdsp matplotlib
jupyter lab tf_train_and_save.ipynb
```

Run the notebook with `research` as the working directory because its dataset, test audio, and output paths are relative to that directory. The expected training layout is:

```text
research/
  dataset/
    train/
      positive/
      negative/
  hi_test.wav
  giberish_test.wav
  tf_train_and_save.ipynb
```

The notebook splits training data into training and validation sets, trains the QAT model, evaluates held-out positive and negative recordings with ROC and precision-recall curves, and converts the model to full-int8 TFLite. It writes checkpoints under `research/tf_ckpts/` and the converted model as `research/keyword_model_qat_int8.tflite`.

## Run on Teensy 4.1

Build and upload the sketch in `firmware/firmware.ino` with Arduino IDE and Teensyduino, selecting **Teensy 4.1**. Install the libraries used by the sketch: MicroTFLite, Teensy Audio Library, CMSIS-DSP, and U8g2.

Connect the components as follows:

| Component | Signal | Teensy 4.1 |
|---|---|---|
| ICS43434 I2S microphone | VDD / GND | 3.3V / GND |
| ICS43434 I2S microphone | SCK | Pin 21 |
| ICS43434 I2S microphone | SD | Pin 8 |
| ICS43434 I2S microphone | WS | Pin 20 |
| ICS43434 I2S microphone | L/R | GND (left channel) |
| SH1106 I2C display | VCC / GND | 3.3V / GND |
| SH1106 I2C display | SCL / SDA | Pin 19 / Pin 18 |

The report specifies 4.7 kOhm pull-up resistors on the I2C lines and a 0.1 uF capacitor across the display's power pins. The model runs approximately every quarter-second and takes under 1 ms per inference in the reported setup. On detection, the firmware displays a message and briefly pauses listening.

The notebook conversion output and the model array used by the firmware are separate artifacts. If replacing the model, update `firmware/model_data.h` using xxd tool and verify the input/output quantization parameters and detection threshold in the firmware match the newly converted model.
