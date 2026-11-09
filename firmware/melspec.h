#ifndef MELSPEC_H
#define MELSPEC_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <arm_math.h>

    typedef struct melspec_t
    {
        arm_rfft_instance_q15 fft_instance;
    } melspec_t;

#define FFT_LEN 256
#define FRAME_LEN FFT_LEN
#define HOP_LEN (FRAME_LEN / 2)
#define DIRECT_FFT 0
#define INVERSE_FFT 1
#define ARM_RFFT_BITS_TO_UPSCALE 8 // https://arm-software.github.io/CMSIS_5/DSP/html/group__RealFFT.html#ga00e615f5db21736ad5b27fb6146f3fc5

#define N_MELS 16

#define FFT_BITREVERSE 0
#define FFT_NO_BITREVERSE 1

#define MICRO_Q15 0x00000219
#define SHIFT_MELFILTER_SATURATION_Q15 15

    void melfft_q15(melspec_t *melspec_obj, q15_t *data, q15_t *result);
    void melspec_init(melspec_t *melspec_obj);

#ifdef __cplusplus
}
#endif

#endif
