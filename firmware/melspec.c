#include <arm_math.h>

#include "melspec.h"
#include "melspec_data.h"


void melspec_init(melspec_t *melspec_obj)
{
  arm_rfft_init_q15(&melspec_obj->fft_instance, FFT_LEN, DIRECT_FFT, FFT_NO_BITREVERSE);
}

void melfft_q15(melspec_t *melspec_obj, q15_t *data, q15_t *result)
{
  q15_t fft_buffer[2 * FFT_LEN] = {0};
  q15_t win_buffer[FFT_LEN] = {0};

  // apply window function to a slice
  arm_mult_q15(data, hamm_window_coefs, win_buffer, FFT_LEN);

  // perform fft
  arm_rfft_q15(&melspec_obj->fft_instance, win_buffer, fft_buffer);
  // rfft gives results in format 8.8. since our signals are quiet, saturation is necessary for correct work
  arm_shift_q15(fft_buffer, 8, fft_buffer, 2 * FFT_LEN);

  // convert outs to magnitude
  q15_t cpl_mag_buffer[FFT_LEN] = {0};
  uint32_t filter_limit = 1 + (FFT_LEN >> 1);
  arm_cmplx_mag_squared_q15(fft_buffer, cpl_mag_buffer, filter_limit); // we need only first N_FFT // 2 +1 samples here

  /* Apply MEL filters */
  uint32_t coefs_pos = 0;
  for (size_t i = 0; i < N_MELS; i++)
  {
    q63_t curr_result;
    arm_dot_prod_q15(cpl_mag_buffer + mel_filter_pos[i],
                     &(mel_filter_coefs[coefs_pos]),
                     mel_filter_lengths[i],
                     &curr_result);

    coefs_pos += mel_filter_lengths[i];

    curr_result += MICRO_Q15;
    curr_result >>= SHIFT_MELFILTER_SATURATION_Q15;

    result[i] = __SSAT(curr_result, 16);
  }
}
