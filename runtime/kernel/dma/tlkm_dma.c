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
#include <linux/gfp.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/io.h>
#include "tlkm_dma.h"
#include "tlkm_logging.h"
#include "tlkm_perfc.h"
#include "blue_dma.h"
#include "pcie/pcie_device.h"

typedef struct {
	size_t cpy_sz;
	ssize_t t_id;
	void __user *usr_addr;
} chunk_data_t;

static const struct dma_operations tlkm_dma_ops[] = {
	{
		.init = blue_dma_init,
		.intr_read = blue_dma_intr_handler_read,
		.intr_write = blue_dma_intr_handler_write,
		.copy_from = blue_dma_copy_from,
		.copy_to = blue_dma_copy_to,
		.allocate_buffer = pcie_device_dma_allocate_buffer,
		.free_buffer = pcie_device_dma_free_buffer,
		.buffer_cpu = pcie_device_dma_sync_buffer_cpu,
		.buffer_dev = pcie_device_dma_sync_buffer_dev,
	},
	{
		.init = 0,
		.intr_read = 0,
		.intr_write = 0,
		.copy_from = 0,
		.copy_to = 0,
		.allocate_buffer = 0,
		.free_buffer = 0,
		.buffer_cpu = 0,
		.buffer_dev = 0,
	}
};

int tlkm_dma_init(struct tlkm_device *dev, struct dma_engine *dma, u64 dbase,
		  u64 size)
{
	dev_id_t dev_id = dev->dev_id;
	int i = 0;
	int ret = 0;
	void *base = (void *)((uintptr_t)dbase);
	BUG_ON(!dma);
	DEVLOG(dev_id, TLKM_LF_DMA, "initializing DMA engine @ 0x%px ...",
	       base);

	init_waitqueue_head(&dma->rq);
	init_waitqueue_head(&dma->wq);
	init_waitqueue_head(&dma->wq_enque);

	mutex_init(&dma->regs_mutex);
	mutex_init(&dma->rq_mutex);

	dma->dev_id = dev_id;
	dma->base = base;
	dma->dev = dev;
	dma->ack_register = NULL;

	atomic64_set(&dma->wq_requested, 0);
	atomic64_set(&dma->wq_enqueued, 0);
	atomic64_set(&dma->wq_processed, 0);

	DEVLOG(dev_id, TLKM_LF_DMA, "I/O remapping 0x%px - 0x%px...", base,
	       base + size - 1);
	dma->regs = ioremap((resource_size_t)base, size);
	if (dma->regs == 0 || IS_ERR(dma->regs)) {
		DEVERR(dev_id, "failed to map 0x%p - 0x%p: %lx", base,
		       base + size - 1, PTR_ERR(dma->regs));
		ret = EIO;
		goto err_dma_ioremap;
	}

	DEVLOG(dev_id, TLKM_LF_DMA, "detecting DMA engine type ...");
	while (tlkm_dma_ops[i].init != 0) {
		int r = tlkm_dma_ops[i].init(dma);
		if (r) {
			dma->ops = tlkm_dma_ops[i];
			break;
		}
		++i;
	}

	if (tlkm_dma_ops[i].init == 0) {
		DEVLOG(dev_id, TLKM_LF_DMA, "unknown DMA engine");
		goto err_unknown_dma;
	}

	DEVLOG(dev_id, TLKM_LF_DMA,
	       "allocating DMA buffers of 2 x %d x %zd bytes ...",
	       TLKM_DMA_CHUNKS, TLKM_DMA_CHUNK_SZ);

	for (i = 0; i < TLKM_DMA_CHUNKS; ++i) {
		ret = dma->ops.allocate_buffer(dev->dev_id, dev,
					       &dma->dma_buf_read[i],
					       &dma->dma_buf_read_dev[i],
					       FROM_DEV, TLKM_DMA_CHUNK_SZ);
		if (ret) {
			ret = PTR_ERR(dma->dma_buf_read[i]);
			DEVERR(dev_id,
			       "failed to allocate %zd bytes for read direction",
			       TLKM_DMA_CHUNK_SZ);
			goto err_dma_bufs_read;
		}
	}

	for (i = 0; i < TLKM_DMA_CHUNKS; ++i) {
		ret = dma->ops.allocate_buffer(dev->dev_id, dev,
					       &dma->dma_buf_write[i],
					       &dma->dma_buf_write_dev[i],
					       TO_DEV, TLKM_DMA_CHUNK_SZ);
		if (ret) {
			ret = PTR_ERR(dma->dma_buf_write[i]);
			DEVERR(dev_id,
			       "failed to allocate %zd bytes for write direction",
			       TLKM_DMA_CHUNK_SZ);
			goto err_dma_bufs_write;
		}
	}

	DEVLOG(dev_id, TLKM_LF_DMA, "DMA engine initialized");
	return 0;

err_dma_bufs_write:
	for (i -= 1; i >= 0; --i) {
		dma->ops.free_buffer(dev->dev_id, dev, &dma->dma_buf_write[i],
				     &dma->dma_buf_write_dev[i], TO_DEV,
				     TLKM_DMA_CHUNK_SZ);
	}
	i = TLKM_DMA_CHUNKS;
err_dma_bufs_read:
	for (i -= 1; i >= 0; --i) {
		dma->ops.free_buffer(dev->dev_id, dev, &dma->dma_buf_read[i],
				     &dma->dma_buf_read_dev[i], FROM_DEV,
				     TLKM_DMA_CHUNK_SZ);
	}
err_unknown_dma:
	iounmap(dma->regs);
	dma->regs = 0;
err_dma_ioremap:
	return -ENODEV;
}

void tlkm_dma_exit(struct dma_engine *dma)
{
	int i = 0;
	if (dma->regs != 0 && !IS_ERR(dma->regs)) {
		struct tlkm_device *dev = dma->dev;
		DEVLOG(dma->dev_id, TLKM_LF_DMA, "freeing buffers");
		for (i = 0; i < TLKM_DMA_CHUNKS; ++i) {
			dma->ops.free_buffer(dev->dev_id, dev,
					     &dma->dma_buf_write[i],
					     &dma->dma_buf_write_dev[i], TO_DEV,
					     TLKM_DMA_CHUNK_SZ);
			dma->ops.free_buffer(dev->dev_id, dev,
					     &dma->dma_buf_read[i],
					     &dma->dma_buf_read_dev[i],
					     FROM_DEV, TLKM_DMA_CHUNK_SZ);
		}
		DEVLOG(dma->dev_id, TLKM_LF_DMA, "unmapping IO memory");
		iounmap(dma->regs);
		memset(dma, 0, sizeof(*dma));
		DEVLOG(dma->dev_id, TLKM_LF_DMA, "deinitialized DMA engine");
	}
}

ssize_t tlkm_dma_copy_to(struct dma_engine *dma, dev_addr_t dev_addr,
			 const void __user *usr_addr, size_t len)
{
	struct tlkm_device *dev = dma->dev;
	size_t cpy_sz = len;
	int err;
	uint64_t last_chunk = 0;

	if ((dev_addr % dma->alignment) != 0) {
		DEVERR(dma->dev_id,
		       "Transfer is not properly aligned for dma engine. All transfers have to be aligned to %d bytes.",
		       dma->alignment);
		return -EAGAIN;
	}

	while (len > 0) {
		uint64_t last_chunk_slot = 0;
		last_chunk = atomic64_inc_return(&dma->wq_requested) - 1;
		last_chunk_slot = last_chunk % TLKM_DMA_CHUNKS;

		DEVLOG(dma->dev_id, TLKM_LF_DMA,
		       "DMA Status: Requested: %lld, Enqueued: %lld Completed: %lld",
		       (long long int)atomic64_read(&dma->wq_requested),
		       (long long int)atomic64_read(&dma->wq_enqueued),
		       (long long int)atomic64_read(&dma->wq_processed));

		DEVLOG(dma->dev_id, TLKM_LF_DMA,
		       "outstanding bytes: %zd - usr_addr = 0x%px, dev_addr = 0x%px",
		       len, usr_addr, (void *)dev_addr);

		DEVLOG(dma->dev_id, TLKM_LF_DMA,
		       "using buffer: %lld and slot %lld",
		       (long long int)last_chunk,
		       (long long int)last_chunk_slot);

		cpy_sz = len < TLKM_DMA_CHUNK_SZ ? len : TLKM_DMA_CHUNK_SZ;

		if (wait_event_interruptible(dma->wq,
					     atomic64_read(&dma->wq_processed) +
							     TLKM_DMA_CHUNKS >
						     last_chunk)) {
			DEVWRN(dma->dev_id,
			       "got killed while waiting for free buffer");
			err = -EACCES;
			goto copy_err;
		}

		dma->ops.buffer_cpu(dev->dev_id, dev,
				    &dma->dma_buf_write[last_chunk_slot],
				    &dma->dma_buf_write_dev[last_chunk_slot],
				    TO_DEV, cpy_sz);
		if (copy_from_user(dma->dma_buf_write[last_chunk_slot],
				   usr_addr, cpy_sz)) {
			DEVERR(dma->dev_id, "could not copy data from user");
			err = -EAGAIN;
			goto copy_err;
		} else {
			dma->ops.buffer_dev(
				dev->dev_id, dev,
				&dma->dma_buf_write[last_chunk_slot],
				&dma->dma_buf_write_dev[last_chunk_slot],
				TO_DEV, cpy_sz);

			if (wait_event_interruptible(
				    dma->wq_enque,
				    atomic64_read(&dma->wq_enqueued) ==
					    last_chunk)) {
				DEVWRN(dma->dev_id,
				       "got killed while waiting for write enqueue for chunk %lld -> %lld",
				       (long long int)last_chunk,
				       (long long int)atomic64_read(
					       &dma->wq_enqueued));
				err = -EACCES;
				goto copy_err;
			}
			dma->ops.copy_to(
				dma, dev_addr,
				dma->dma_buf_write_dev[last_chunk_slot],
				cpy_sz);

			wake_up_interruptible(&dma->wq_enque);

			usr_addr += cpy_sz;
			dev_addr += cpy_sz;
			len -= cpy_sz;
		}
	}

	if (wait_event_interruptible(
		    dma->wq, atomic64_read(&dma->wq_processed) > last_chunk)) {
		DEVWRN(dma->dev_id,
		       "got killed while waiting for last transfer");
		err = -EACCES;
		goto finish_err;
	}

	tlkm_perfc_dma_writes_add(dma->dev_id, len);
	return len;

copy_err:
	DEVWRN(dma->dev_id, "Still got chunk %lld, waiting to free.",
	       (long long int)last_chunk);
	wait_event_interruptible(dma->wq, atomic64_read(&dma->wq_enqueued) ==
						  last_chunk);
	atomic64_inc(&dma->wq_enqueued);
	wait_event_interruptible(dma->wq, atomic64_read(&dma->wq_processed) ==
						  last_chunk);
	atomic64_inc(&dma->wq_processed);
finish_err:
	return err;
}

ssize_t tlkm_dma_copy_from(struct dma_engine *dma, void __user *usr_addr,
			   dev_addr_t dev_addr, size_t len)
{
	struct tlkm_device *dev = dma->dev;
	size_t cpy_sz = len;
	int i, err;
	int current_buffer = 0;
	chunk_data_t chunks[TLKM_DMA_CHUNKS];
	if ((dev_addr % dma->alignment) != 0) {
		DEVERR(dma->dev_id,
		       "Transfer is not properly aligned for dma engine. All transfers have to be aligned to %d bytes.",
		       dma->alignment);
		return -EAGAIN;
	}

	mutex_lock(&dma->rq_mutex);

	for (i = 0; i < TLKM_DMA_CHUNKS; ++i) {
		chunks[i].t_id = 0;
		chunks[i].usr_addr = 0;
		chunks[i].cpy_sz = 0;
	}

	atomic64_set(&dma->rq_enqueued, 0);
	atomic64_set(&dma->rq_processed, 0);

	while (len > 0) {
		DEVLOG(dma->dev_id, TLKM_LF_DMA,
		       "outstanding bytes: %zd - usr_addr = 0x%px, dev_addr = 0x%px",
		       len, usr_addr, (void *)dev_addr);
		DEVLOG(dma->dev_id, TLKM_LF_DMA,
		       "using buffer: %d and waiting for t_id == %zd",
		       current_buffer, chunks[current_buffer].t_id);
		if (wait_event_interruptible(
			    dma->rq, atomic64_read(&dma->rq_processed) >=
					     chunks[current_buffer].t_id)) {
			DEVWRN(dma->dev_id,
			       "got killed while hanging in waiting queue");
			err = -EACCES;
			goto copy_err;
		}
		if (chunks[current_buffer].usr_addr != 0) {
			dma->ops.buffer_cpu(
				dev->dev_id, dev,
				&dma->dma_buf_read[current_buffer],
				&dma->dma_buf_read_dev[current_buffer],
				FROM_DEV, chunks[current_buffer].cpy_sz);
			if (copy_to_user(chunks[current_buffer].usr_addr,
					 dma->dma_buf_read[current_buffer],
					 chunks[current_buffer].cpy_sz)) {
				DEVERR(dma->dev_id,
				       "could not copy data to user");
				err = -EAGAIN;
				goto copy_err;
			}
			chunks[current_buffer].usr_addr = 0;
		}

		cpy_sz = len < TLKM_DMA_CHUNK_SZ ? len : TLKM_DMA_CHUNK_SZ;
		dma->ops.buffer_dev(dev->dev_id, dev,
				    &dma->dma_buf_read[current_buffer],
				    &dma->dma_buf_read_dev[current_buffer],
				    FROM_DEV, cpy_sz);
		chunks[current_buffer].t_id = dma->ops.copy_from(
			dma, dma->dma_buf_read_dev[current_buffer], dev_addr,
			cpy_sz);

		chunks[current_buffer].usr_addr = usr_addr;
		chunks[current_buffer].cpy_sz = cpy_sz;

		usr_addr += cpy_sz;
		dev_addr += cpy_sz;
		len -= cpy_sz;
		current_buffer = (current_buffer + 1) % TLKM_DMA_CHUNKS;
	}

	for (i = 0; i < TLKM_DMA_CHUNKS; ++i) {
		if (wait_event_interruptible(
			    dma->rq, atomic64_read(&dma->rq_processed) >=
					     chunks[i].t_id)) {
			DEVWRN(dma->dev_id,
			       "got killed while hanging in waiting queue");
			err = -EACCES;
			goto copy_err;
		}

		if (chunks[i].usr_addr != 0) {
			dma->ops.buffer_cpu(dev->dev_id, dev,
					    &dma->dma_buf_read[i],
					    &dma->dma_buf_read_dev[i], FROM_DEV,
					    chunks[i].cpy_sz);
			if (copy_to_user(chunks[i].usr_addr,
					 dma->dma_buf_read[i],
					 chunks[i].cpy_sz)) {
				DEVERR(dma->dev_id,
				       "could not copy data to user");
				err = -EAGAIN;
				goto copy_err;
			}
			chunks[i].usr_addr = 0;
		}
	}

	tlkm_perfc_dma_reads_add(dma->dev_id, len);
	mutex_unlock(&dma->rq_mutex);
	return len;

copy_err:
	mutex_unlock(&dma->rq_mutex);
	return err;
}
