#ifndef WZ_XDMA_H
#define WZ_XDMA_H

#include "wzdma/xdma-ioctl.h"
#include "stdint.h"
#define TOT_BUF_LEN ((int64_t) WZ_DMA_BUFLEN * (int64_t) WZ_DMA_NOFBUFS)

struct wz_private {
	struct wz_xdma_data_block_desc bdesc __attribute__ ((aligned (8) ));
	struct wz_xdma_data_block_confirm bconf __attribute__ ((aligned (8) ));
	int fd_user;
	int fd_control;
	int fd_memory;
	volatile uint32_t *usr_regs;
	volatile char *data_buf;
};

#ifdef __cplusplus
extern "C" {
#endif
    int wz_init(struct wz_private* wz);
    int wz_close(struct wz_private* wz);
    int wz_start_dma(struct wz_private* wz);
    int wz_stop_dma(struct wz_private* wz);
    ssize_t wz_read_start(struct wz_private* wz, char **buffer);
    int wz_read_complete(struct wz_private* wz);

	int wz_reset_board();
#ifdef __cplusplus
} // extern "C"
#endif

#endif
