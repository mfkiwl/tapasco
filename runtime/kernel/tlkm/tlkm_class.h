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
#ifndef TLKM_CLASS_H__
#define TLKM_CLASS_H__

#include <linux/interrupt.h>
#include "tlkm_types.h"
#include "tlkm_platform.h"
#include "tlkm_control.h"

#define TLKM_CLASS_NAME_LEN 30

struct tlkm_device;
struct tlkm_class;

typedef int (*tlkm_class_create_f)(struct tlkm_device *, void *data);
typedef void (*tlkm_class_destroy_f)(struct tlkm_device *);
typedef int (*tlkm_class_init_subsystems_f)(struct tlkm_device *, void *data);
typedef void (*tlkm_class_exit_subsystems_f)(struct tlkm_device *);
typedef int (*tlkm_class_probe_f)(struct tlkm_class *);
typedef void (*tlkm_class_remove_f)(struct tlkm_class *);

typedef int (*tlkm_class_miscdev_open_f)(struct tlkm_device *);
typedef void (*tlkm_class_miscdev_close_f)(struct tlkm_device *);

typedef long (*tlkm_device_ioctl_f)(struct tlkm_device *, unsigned int ioctl,
				    unsigned long data);
typedef int (*tlkm_device_init_irq_f)(struct tlkm_device *,
				      struct list_head *interrupts);
typedef void (*tlkm_device_exit_irq_f)(struct tlkm_device *);
typedef int (*tlkm_device_pirq_f)(struct tlkm_device *,
				  struct tlkm_irq_mapping *mapping);
typedef void (*tlkm_device_rirq_f)(struct tlkm_device *,
				   struct tlkm_irq_mapping *mapping);

typedef void *(*tlkm_class_addr2map_f)(struct tlkm_device *dev,
				       dev_addr_t const addr);

struct tlkm_class {
	char name[TLKM_CLASS_NAME_LEN];
	tlkm_class_create_f create;
	tlkm_class_destroy_f destroy;
	tlkm_class_init_subsystems_f init_subsystems;
	tlkm_class_exit_subsystems_f exit_subsystems;
	tlkm_class_miscdev_open_f miscdev_open;
	tlkm_class_miscdev_close_f miscdev_close;
	tlkm_class_probe_f probe;
	tlkm_class_remove_f remove;
	tlkm_class_addr2map_f addr2map;
	tlkm_device_ioctl_f ioctl; /* ioctl implementation */
	tlkm_device_init_irq_f init_interrupts;
	tlkm_device_exit_irq_f exit_interrupts;
	tlkm_device_pirq_f pirq;
	tlkm_device_rirq_f rirq;
	size_t number_of_interrupts;
	struct platform platform; /* register space definitions */
	void *private_data;
};

#endif /* TLKM_CLASS_H__ */
