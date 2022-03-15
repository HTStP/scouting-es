/*
 * This file defines the API for the DWZ DMA driver.
 */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>

#include "wz_dma.h"

// perror may modify errno, we have to save it
#define PERROR(msg) do { int err = errno; perror(msg); errno = err; } while (0)

/* Open necessary devices and map DMAable memory */
int wz_init(struct wz_private* wz) 
{
	int res;

	wz->fd_user = -1;
	wz->fd_control = -1;
	wz->fd_memory = -1;
	wz->usr_regs = NULL;
	wz->data_buf = NULL;

	//int err = 0;

	if ( (wz->fd_user = open("/dev/wz-xdma0_user", O_RDWR)) < 0 ) {
		PERROR("Can't open /dev/wz-xdma0_user");
		return -1;
	};

	if ( (wz->fd_control = open("/dev/wz-xdma0_control", O_RDWR)) < 0 ) {
		PERROR("Can't open /dev/wz-xdma0_control");
		return -1;
	};

	if ( (wz->fd_memory = open("/dev/wz-xdma0_c2h_0", O_RDWR)) < 0 ) {
		PERROR("Can't open /dev/wz-xdma0_c2h_0");
		return -1;
	};

	//Allocate buffers
	if ( (res = ioctl(wz->fd_memory, IOCTL_XDMA_WZ_ALLOC_BUFFERS, 0L)) < 0 ) {
		PERROR("Can't mmap DMA buffers");
		return -1;
	}

	//Now mmap the user registers
	if ( (wz->usr_regs = (volatile uint32_t *) mmap(NULL, 1024*1024, PROT_READ | PROT_WRITE, MAP_SHARED, wz->fd_user, 0)) == MAP_FAILED) {
		PERROR("Can't mmap user registers");
		return -1;	
	}

	if ( (wz->data_buf = (volatile char *) mmap(NULL, TOT_BUF_LEN, PROT_READ|PROT_WRITE, MAP_SHARED, wz->fd_memory, 0)) == MAP_FAILED) { 
		PERROR("Can't mmap data buffer");
		return -1;
	}

	// Ensure, that all pages are mapped (allocated)
	// Assume page size is 4096. If it is larger, it will not hurt, just take long(er) time
	int pagesize = 4096;
	unsigned long i;
	for (i = 0; i < TOT_BUF_LEN; i += pagesize) {
		// Touch memory so it is really allocated
        ((volatile char*) wz->data_buf)[i] = ((volatile char*) wz->data_buf)[i];
	}

	return 0;
}

/* The opposite of wz_init */
int wz_close(struct wz_private* wz) 
{
	munmap((void *)wz->data_buf, TOT_BUF_LEN);
	munmap((void *)wz->usr_regs, 1024*1024);
	ioctl(wz->fd_memory, IOCTL_XDMA_WZ_FREE_BUFFERS, 0L);
	close(wz->fd_memory);
	close(wz->fd_control);
	close(wz->fd_user);
	return 0;
}

/* Start the DMA engine */
inline int wz_start_dma(struct wz_private* wz)
{
	return ioctl(wz->fd_memory, IOCTL_XDMA_WZ_START, 0L);
}

/* Stop the DMA engine */
inline int wz_stop_dma(struct wz_private* wz)
{
	return ioctl(wz->fd_memory, IOCTL_XDMA_WZ_STOP, 0L);
}


static inline int wz_get_buf(struct wz_private* wz) 
{
	return ioctl(wz->fd_memory, IOCTL_XDMA_WZ_GETBUF, (long) &wz->bdesc);
}

static inline int wz_confirm_buf(struct wz_private* wz)
{		
	return ioctl(wz->fd_memory, IOCTL_XDMA_WZ_CONFIRM, (long) &wz->bconf);
}


/* Acquire and return buffer */
inline ssize_t wz_read_start(struct wz_private* wz, char **buffer)
{
	int ret;
	if ( (ret = wz_get_buf( wz )) < 0) {
		return ret;
	}

	int64_t start_offset = (int64_t)wz->bdesc.first_desc * (int64_t)WZ_DMA_BUFLEN;
	int64_t end_offset   = (int64_t)wz->bdesc.last_desc * (int64_t)WZ_DMA_BUFLEN + (int64_t) wz->bdesc.last_len;
	int64_t bytes_read = end_offset - start_offset;

	*buffer = (char *)(wz->data_buf + start_offset);

	return bytes_read;
}


/* Mark the buffer to be available for DMA */
inline int wz_read_complete(struct wz_private* wz)
{
	wz->bconf.first_desc = wz->bdesc.first_desc;
	wz->bconf.last_desc = wz->bdesc.last_desc;

	return wz_confirm_buf( wz );
}


/* 
 * The user logic is driving custom FPGA logic. This is not necessary to use if not implemented in the FPGA.
 */

inline void wz_start_source(struct wz_private* wz)
{
	wz->usr_regs[0x10000/4] = 1;
	asm volatile ("" : : : "memory");
}

// User logic
inline void wz_stop_source(struct wz_private* wz)
{
	wz->usr_regs[0x10000/4] = 0;
	asm volatile ("" : : : "memory");
}


#include <stdio.h> 
#include <arpa/inet.h>

/*
 * Normally, one should write to a register. But because we don't have a support for register for
 * the moment, we use a server connected to Vivado to do a reset
 */ 
int wz_reset_board()
{
    struct sockaddr_in saddr_in;
    int sock;
    char buf[12];
    int bytes_read;

    // Fill the server address
    inet_aton("127.0.0.1", &(saddr_in.sin_addr));
    saddr_in.sin_family = AF_INET;
    saddr_in.sin_port = htons(12345);

    //print out IP address
    printf("Server IP:  %s\n", inet_ntoa(saddr_in.sin_addr));

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        PERROR("Socket");
        return -1;
    }

    // Connect socket to the server
    if (connect(sock, (struct sockaddr *) &saddr_in, sizeof(struct sockaddr_in)) < 0) {
        PERROR("Connect");
        goto error;
    }

    if (send(sock, "reset\n", 6, 0) < 0) {
        PERROR("Send");
        goto error;
    }

    if ((bytes_read = recv(sock, buf, sizeof(buf) - 1, 0)) < 0) {
        PERROR("Recv");
        goto error;
    }

    // Put null terminating character
    buf[bytes_read] = 0;
    printf("Received '%s'\n", buf);

    close(sock);

    printf("Finished\n");
    return 0;

error:
    close(sock);
    return -1;
}
