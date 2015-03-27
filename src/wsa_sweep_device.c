#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include "wsa_sweep_device.h"
#include "kiss_fft.h"


void benchmark(struct timeval *since, char *msg)
{
	struct timeval now;
	time_t diff_sec;
	suseconds_t diff_usec;

	gettimeofday(&now, NULL);

	// diff seconds
	diff_sec = now.tv_sec - since->tv_sec;

	// diff nsec.. account for wrap
	if (now.tv_usec >= since->tv_usec) {
		diff_usec = now.tv_usec - since->tv_usec;
	} else {
		diff_usec = (now.tv_usec + 1000000) - since->tv_usec;
		diff_sec -= 1;
	}

	printf("Mark -- %s -- %d.%06u\n", msg, diff_sec, diff_usec);
}

/**
 * dumps a vrt packet header to stdout
 *
 * @param header - the struct wsa_vrt_packet_header to dump
 */
void wsa_dump_vrt_packet_header(struct wsa_vrt_packet_header *header)
{
	printf("VRT Header");
	
	if (header->stream_id == RECEIVER_STREAM_ID)
		printf("(CTX_RECEIVER): ");
	else if (header->stream_id == DIGITIZER_STREAM_ID)
		printf("(CTX_DIGITIZER): ");
	else if (header->stream_id == EXTENSION_STREAM_ID)
		printf("(CTX_EXTENSION): ");
	else if (header->stream_id == I16Q16_DATA_STREAM_ID)
		printf("(DATA_I16Q16): ");
	else if (header->stream_id == I16_DATA_STREAM_ID)
		printf("(DATA_I16): ");
	else if (header->stream_id == I32_DATA_STREAM_ID)
		printf("(DATA_I32): ");
	else
		printf("(UNKNOWN=0x%08x): ", header->stream_id);

	if (header->packet_type == IF_PACKET_TYPE)
		printf("type=IF, ");
	else if (header->packet_type == CONTEXT_PACKET_TYPE)
		printf("type=CONTEXT, ");
	else if (header->packet_type == EXTENSION_PACKET_TYPE)
		printf("type=EXTENSION, ");
	else
		printf("type=UNKNOWN(%d), ", header->packet_type);
	
	printf("count=%d, spp=%u, ts:%u.%012llus\n", 
		header->pkt_count, 
		header->samples_per_packet,
		header->time_stamp.sec,
		header->time_stamp.psec
	);
}


/**
 * creates a new sweep device object and returns it
 *
 * @param device - a pointer to the wsa we've connected to
 * @return - a pointer to the allocated struct, or NULL on failure
 */
struct wsa_sweep_device *wsa_sweep_device_new(struct wsa_device *device)
{
	struct wsa_sweep_device *sweepdev;

	// alloc memory for our object
	sweepdev = malloc(sizeof(struct wsa_sweep_device));
	if (sweepdev == NULL)
		return NULL;

	// initialize everything in the struct
	sweepdev->real_device = device;

	return sweepdev;
}


/**
 * destroys a sweep device.  This does not free the device param that was passed in initially.  only the sweep device
 *
 * @sweepdev - the object to destroy
 */
void wsa_sweep_device_free(struct wsa_sweep_device *sweepdev)
{
	// free the memory of the sweep device, (but not the real device, it came from the parent, so it's their problem)
	free(sweepdev);
}


/**
 * allocates memory to do power spectrum domain captures on the bandwidths indicated
 *
 * @param sweep_device - the sweep device
 * @param fstart - the start of the band to sweep
 * @param fstop - the end of the band to sweep
 * @param rbw - the minimum resolution bandwidth desired
 * @param mode - which mode to perform the sweep in
 * @param pscfg - a pointer to an unallocated power spectrum config struct
 * @returns - negative on error, otherwise the number of bytes allocated
 */
int wsa_power_spectrum_alloc(
	struct wsa_sweep_device *sweep_device,
	uint64_t fstart,
	uint64_t fstop,
	uint32_t rbw,
	char const *mode,
	struct wsa_power_spectrum_config **pscfg
)
{
	struct wsa_power_spectrum_config *ptr;

	// alloc some memory for it
	ptr = malloc(sizeof(struct wsa_power_spectrum_config));
	if (ptr == NULL)
		return -1;
	*pscfg = ptr;

	// copy the sweep settings into the cfg object
	ptr->fstart = fstart;
	ptr->fstop = fstop;
	ptr->rbw = rbw;
	ptr->buflen = (fstop - fstart) / rbw;
	ptr->buf = malloc(sizeof(float) * ptr->buflen);
	printf("-> buflen = %d\n", ptr->buflen);
	printf("-> buf @ 0x%08x\n", (unsigned int) ptr->buf);
	if (ptr->buf == NULL) {
		free(ptr);
		return -1;
	}

	return 0;
}


/**
 * destroys a power spectrum config object
 *
 * @param cfg - the config oject to destroy
 */
void wsa_power_spectrum_free(struct wsa_power_spectrum_config *cfg)
{
	// free the buffer and then the struct
	free(cfg->buf);
	free(cfg);
}


/**
 * captures some power spectrum using the configuration supplied
 *
 * @param sweep_device - the sweep device to use
 * @param cfg - the power spectrum config to use
 * @param buf - if buf is not NULL, a pointer to the allocated memory is stored there for convience
 * @return - 0 on success, negative on error
 */
int wsa_capture_power_spectrum(
	struct wsa_sweep_device *sweep_device,
	struct wsa_power_spectrum_config *cfg,
	float **buf
)
{
	int i;
	int16_t result;
	struct wsa_device *dev = sweep_device->real_device;
	struct wsa_vrt_packet_header header;
	struct wsa_vrt_packet_trailer trailer;
	struct wsa_receiver_packet receiver;
	struct wsa_digitizer_packet digitizer;
	struct wsa_extension_packet sweep;
	int16_t i16_buffer[1024];
	int16_t q16_buffer[1024];
	int32_t i32_buffer[1024];
	struct timeval start;
	kiss_fft_cfg fft;
	kiss_fft_cpx fftin[1024];
	kiss_fft_cpx fftout[1024];

	gettimeofday(&start, NULL);
	benchmark(&start, "start");

	// assign their convienence pointer
	if (*buf)
		*buf = cfg->buf;

	/*
	 * do a single capture
	 */

	// flush buffer
	wsa_flush_data(dev);

	// do a capture
	wsa_set_rfe_input_mode(dev, WSA_RFE_SHN_STRING);
	wsa_set_samples_per_packet(dev, 1024);
	wsa_set_packets_per_block(dev, 1);
	result = wsa_capture_block(dev);
	if (result < 0) {
		fprintf(stderr, "error: wsa_capture_block(): %d\n", result);
		return -1;
	}
	benchmark(&start, "capture");

	/*
	 * read out packets until we get a data packet
	 */
	while(1) {

		// poison the buffers
		for(i=0; i<1024; i++)
			i16_buffer[i] = 9999;

		// read a packet
		result = wsa_read_vrt_packet(
			dev, 
			&header, &trailer, &receiver, &digitizer, &sweep,
			i16_buffer, q16_buffer, i32_buffer, 
			1024, 
			5000
		);
		if (result < 0) {
			fprintf(stderr, "error: wsa_read_vrt_packet(): %d\n", result);
			return -1;
		}
		wsa_dump_vrt_packet_header(&header);

		if (header.packet_type == IF_PACKET_TYPE)
			break;
	}
	benchmark(&start, "read");

	/*
	 * fft that
	 */

	/*
	 * apply reflevel
	 */


	return 0;
}
