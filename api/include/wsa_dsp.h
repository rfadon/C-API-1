
#include "kiss_fft.h"
#include "thinkrf_stdint.h"


// ////////////////////////////////////////////////////////////////////////////
// Normalize Section                                                         //
// ////////////////////////////////////////////////////////////////////////////
void normalize_iq_data(int32_t samples_per_packet,
					uint32_t stream_id,
					int16_t * i16_buffer,
					int16_t * q16_buffer,
					int32_t * i32_buffer,
					kiss_fft_scalar * idata,
					kiss_fft_scalar * qdata);

void correct_dc_offset(int32_t samples_per_packet,
					kiss_fft_scalar * idata,
					kiss_fft_scalar * qdata);

// ////////////////////////////////////////////////////////////////////////////
// Windowing Section                                                         //
// ////////////////////////////////////////////////////////////////////////////

void window_hanning_scalar_array(kiss_fft_scalar *values, int len);
void window_hanning_cpx(kiss_fft_cpx *value, int len, int index);

// ////////////////////////////////////////////////////////////////////////////
// Spectral Inversion Section                                                //
// ////////////////////////////////////////////////////////////////////////////

void reverse_cpx(kiss_fft_cpx *value, int len);

// ////////////////////////////////////////////////////////////////////////////
// FFT Section                                                               //
// ////////////////////////////////////////////////////////////////////////////
int rfft(kiss_fft_scalar *idata, kiss_fft_cpx *fftdata, int len);
kiss_fft_scalar cpx_to_power(kiss_fft_cpx value);
kiss_fft_scalar power_to_logpower(kiss_fft_scalar value);

// ////////////////////////////////////////////////////////////////////////////
// Utility Functions                                                         //
// ////////////////////////////////////////////////////////////////////////////
int16_t psd_peak_find(uint64_t fstart, 
				uint64_t fstop, 
				uint32_t rbw, 
				uint32_t data_size,
				float *spectra_data,
				uint64_t *peak_freq,
				float *peak_power);

int16_t psd_calculate_channel_power(uint32_t start_bin,
								uint32_t stop_bin,
								float *spectral_data,
								uint32_t data_size,
								float *channel_power);

int16_t psd_calculate_absolute_power(uint32_t start_bin,
								uint32_t stop_bin,
								float *spectral_data,
								uint32_t data_size,
								float *absolute_power);