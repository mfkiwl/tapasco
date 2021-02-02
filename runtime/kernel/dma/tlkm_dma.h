/*
 * Copyright (c) 2014-2020 Embedded Systems and Applications, TU Darmstadt.
 *
 * This file is part of TaPaSCo 
 * (see https://github.com/esa-tu-darmstadt/tapasco).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef TLKM_DMA_H__
#define TLKM_DMA_H__

#include <linux/atomic.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/version.h>
#include "tlkm_types.h"

struct dma_engine;
struct tlkm_device;

typedef int (*dma_init_fun)(struct dma_engine *);
typedef irqreturn_t (*dma_intr_handler)(int, void *);
typedef ssize_t (*dma_copy_to_func_t)(struct dma_engine *, dev_addr_t,
				      dma_addr_t, size_t);
typedef ssize_t (*dma_copy_from_func_t)(struct dma_engine *, dma_addr_t,
					dev_addr_t, size_t);

typedef enum { TO_DEV, FROM_DEV } dma_direction_t;

typedef int (*dma_allocate_buffer_func_t)(dev_id_t dev_id,
					  struct tlkm_device *dev,
					  void **buffer, dma_addr_t *dev_handle,
					  dma_direction_t direction,
					  size_t size);
typedef void (*dma_free_buffer_func_t)(dev_id_t dev_id, struct tlkm_device *dev,
				       void **buffer, dma_addr_t *dev_handle,
				       dma_direction_t direction, size_t size);

typedef int (*dma_buffer_cpu_func_t)(dev_id_t dev_id, struct tlkm_device *dev,
				     void **buffer, dma_addr_t *dev_handle,
				     dma_direction_t direction, size_t size);
typedef int (*dma_buffer_dev_func_t)(dev_id_t dev_id, struct tlkm_device *dev,
				     void **buffer, dma_addr_t *dev_handle,
				     dma_direction_t direction, size_t size);

struct dma_operations {
	dma_init_fun init;
	dma_allocate_buffer_func_t allocate_buffer;
	dma_free_buffer_func_t free_buffer;
	dma_buffer_cpu_func_t buffer_cpu;
	dma_buffer_dev_func_t buffer_dev;
	dma_copy_to_func_t copy_to;
	dma_copy_from_func_t copy_from;
	dma_intr_handler intr_read;
	dma_intr_handler intr_write;
};

// Currently any chunk size smaller than 2 MB will result in failures due to missing interrupts
#define TLKM_DMA_CHUNK_SZ (size_t)(256 * 1024) // 256 kiB
#define TLKM_DMA_CHUNKS (16)

struct dma_engine {
	dev_id_t dev_id;
	void *base;
	void __iomem *regs;
	struct mutex regs_mutex;
	struct dma_operations ops;
	wait_queue_head_t rq;
	struct mutex rq_mutex;
	atomic64_t rq_enqueued;
	atomic64_t rq_processed;
	wait_queue_head_t wq;
	wait_queue_head_t wq_enque;
	atomic64_t wq_enqueued;
	atomic64_t wq_processed;
	atomic64_t wq_requested;
	void *dma_buf_read[TLKM_DMA_CHUNKS];
	dma_addr_t dma_buf_read_dev[TLKM_DMA_CHUNKS];
	void *dma_buf_write[TLKM_DMA_CHUNKS];
	dma_addr_t dma_buf_write_dev[TLKM_DMA_CHUNKS];
	struct tlkm_device *dev;
	int alignment;
	volatile uint32_t *ack_register;
};

int tlkm_dma_init(struct tlkm_device *dev, struct dma_engine *dma, u64 base,
		  u64 size);
void tlkm_dma_exit(struct dma_engine *dma);

ssize_t tlkm_dma_copy_to(struct dma_engine *dma, dev_addr_t dev_addr,
			 const void __user *usr_addr, size_t len);
ssize_t tlkm_dma_copy_from(struct dma_engine *dma, void __user *usr_addr,
			   dev_addr_t dev_addr, size_t len);

#endif /* TLKM_DMA_H__ */
