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
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/version.h>
#include <linux/atomic.h>
#include "tlkm_logging.h"
#include "tlkm_control.h"
#include "pcie/pcie.h"
#include "pcie/pcie_irq.h"
#include "pcie/pcie_device.h"

#define _INTR(nr)                                                              \
	void tlkm_pcie_slot_irq_work_##nr(struct work_struct *work)            \
	{                                                                      \
		struct tlkm_pcie_device *dev =                                 \
			(struct tlkm_pcie_device *)container_of(               \
				work, struct tlkm_pcie_device, irq_work[nr]);  \
		BUG_ON(!dev->parent->ctrl);                                    \
		tlkm_control_signal_slot_interrupt(dev->parent->ctrl, nr);     \
	}                                                                      \
                                                                               \
	irqreturn_t tlkm_pcie_slot_irq_##nr(int irq, void *dev_id)             \
	{                                                                      \
		struct pci_dev *pdev = (struct pci_dev *)dev_id;               \
		struct tlkm_pcie_device *dev =                                 \
			(struct tlkm_pcie_device *)dev_get_drvdata(            \
				&pdev->dev);                                   \
		BUG_ON(!dev);                                                  \
		if (!schedule_work(&dev->irq_work[nr]))                        \
			tlkm_perfc_irq_error_already_pending_inc(              \
				dev->parent->dev_id);                          \
		tlkm_perfc_total_irqs_inc(dev->parent->dev_id);                \
		dev->ack_register[0] = nr + pcie_cls.npirqs;                   \
		return IRQ_HANDLED;                                            \
	}

TLKM_PCIE_SLOT_INTERRUPTS
#undef _INTR

int pcie_irqs_init(struct tlkm_device *dev)
{
	struct tlkm_pcie_device *pdev =
		(struct tlkm_pcie_device *)dev->private_data;

	int ret = 0, irqn,
	    err[NUMBER_OF_INTERRUPTS] = { [0 ... NUMBER_OF_INTERRUPTS - 1] =
						  1 };
	BUG_ON(!dev);
	pdev->ack_register =
		(volatile uint32_t *)(dev->mmap.plat +
				      tlkm_status_get_component_base(
					      dev, "PLATFORM_COMPONENT_INTC0") +
				      0x8120);
	DEVLOG(dev->dev_id, TLKM_LF_IRQ, "registering %d interrupts ...",
	       NUMBER_OF_INTERRUPTS);
#define _INTR(nr) irqn = nr + pcie_cls.npirqs;

	TLKM_PCIE_SLOT_INTERRUPTS
#undef _INTR
#define _INTR(nr)                                                              \
	irqn = nr + pcie_cls.npirqs;                                           \
	if ((err[nr] = request_irq(pci_irq_vector(pdev->pdev, irqn),           \
				   tlkm_pcie_slot_irq_##nr, IRQF_EARLY_RESUME, \
				   TLKM_PCI_NAME, pdev->pdev))) {              \
		DEVERR(dev->dev_id, "could not request interrupt %d: %d",      \
		       irqn, err[nr]);                                         \
		goto irq_error;                                                \
	} else {                                                               \
		pdev->irq_mapping[irqn] = pci_irq_vector(pdev->pdev, irqn);    \
		DEVLOG(dev->dev_id, TLKM_LF_IRQ,                               \
		       "interrupt line %d/%d assigned with return value %d",   \
		       irqn, pci_irq_vector(pdev->pdev, irqn), err[nr]);       \
		INIT_WORK(&pdev->irq_work[nr], tlkm_pcie_slot_irq_work_##nr);  \
	}
	TLKM_PCIE_SLOT_INTERRUPTS
#undef _INTR
	return 0;

irq_error:
#define _INTR(nr)                                                              \
	irqn = nr + pcie_cls.npirqs;                                           \
	if (!err[nr]) {                                                        \
		free_irq(pdev->irq_mapping[irqn], pdev->pdev);                 \
		pdev->irq_mapping[irqn] = -1;                                  \
	} else {                                                               \
		ret = err[nr];                                                 \
	}
	TLKM_PCIE_SLOT_INTERRUPTS
#undef _INTR
	return ret;
}

void pcie_irqs_exit(struct tlkm_device *dev)
{
	struct tlkm_pcie_device *pdev =
		(struct tlkm_pcie_device *)dev->private_data;
	int irqn;
#define _INTR(nr)                                                              \
	irqn = nr + pcie_cls.npirqs;                                           \
	if (pdev->irq_mapping[irqn] != -1) {                                   \
		DEVLOG(dev->dev_id, TLKM_LF_IRQ,                               \
		       "freeing interrupt %d with mapping %d", irqn,           \
		       pdev->irq_mapping[irqn]);                               \
		free_irq(pdev->irq_mapping[irqn], pdev->pdev);                 \
		pdev->irq_mapping[irqn] = -1;                                  \
	}
	TLKM_PCIE_SLOT_INTERRUPTS
#undef _INTR
	DEVLOG(dev->dev_id, TLKM_LF_IRQ, "interrupts deactivated");
}

#define _INTR(nr)                                                                                                            \
	void aws_ec2_tlkm_pcie_slot_irq_work_##nr(struct work_struct *work)                                                  \
	{                                                                                                                    \
		uint32_t isr;                                                                                                \
		struct tlkm_pcie_device *dev =                                                                               \
			(struct tlkm_pcie_device *)container_of(                                                             \
				work, struct tlkm_pcie_device, irq_work[nr]);                                                \
		/*struct platform *p = &dev->parent->cls->platform;*/                                                        \
		/* read ISR (interrupt status register) */                                                                   \
		isr = dev->ack_register[1 + nr];                                                                             \
		if (unlikely(!isr)) {                                                                                        \
			DEVERR(dev->parent->dev_id,                                                                          \
			       "Interrupt received, but ISR %d is empty", nr);                                               \
			return;                                                                                              \
		}                                                                                                            \
		do {                                                                                                         \
			/* Returns one plus the index of the least significant 1-bit of x, or if x is zero, returns zero. */ \
			const uint32_t slot = __builtin_ffs(isr) - 1;                                                        \
			tlkm_control_signal_slot_interrupt(dev->parent->ctrl,                                                \
							   nr * 32 + slot);                                                  \
			isr ^= (1U << slot);                                                                                 \
		} while (isr);                                                                                               \
	}                                                                                                                    \
                                                                                                                             \
	irqreturn_t aws_ec2_tlkm_pcie_slot_irq_##nr(int irq, void *dev_id)                                                   \
	{                                                                                                                    \
		struct pci_dev *pdev = (struct pci_dev *)dev_id;                                                             \
		struct tlkm_pcie_device *dev =                                                                               \
			(struct tlkm_pcie_device *)dev_get_drvdata(                                                          \
				&pdev->dev);                                                                                 \
		if (!schedule_work(&dev->irq_work[nr]))                                                                      \
			tlkm_perfc_irq_error_already_pending_inc(                                                            \
				dev->parent->dev_id);                                                                        \
		tlkm_perfc_total_irqs_inc(dev->parent->dev_id);                                                              \
		return IRQ_HANDLED;                                                                                          \
	}

TLKM_AWS_EC2_SLOT_INTERRUPTS
#undef _INTR

int aws_ec2_pcie_irqs_init(struct tlkm_device *dev)
{
	struct tlkm_pcie_device *pdev =
		(struct tlkm_pcie_device *)dev->private_data;

	int ret = 0, irqn, err[4] = { [0 ... 3] = 1 };
	BUG_ON(!dev);
	pdev->ack_register =
		(volatile uint32_t *)(dev->mmap.plat +
				      tlkm_status_get_component_base(
					      dev, "PLATFORM_COMPONENT_INTC0") +
				      0x8120);
	DEVLOG(dev->dev_id, TLKM_LF_IRQ, "registering %d interrupts ...", 4);
#define _INTR(nr)                                                              \
	irqn = nr + pcie_cls.npirqs;                                           \
	if ((err[nr] = request_irq(pci_irq_vector(pdev->pdev, irqn),           \
				   aws_ec2_tlkm_pcie_slot_irq_##nr,            \
				   IRQF_EARLY_RESUME, TLKM_PCI_NAME,           \
				   pdev->pdev))) {                             \
		DEVERR(dev->dev_id, "could not request interrupt %d: %d",      \
		       irqn, err[nr]);                                         \
		goto irq_error;                                                \
	} else {                                                               \
		pdev->irq_mapping[irqn] = pci_irq_vector(pdev->pdev, irqn);    \
		DEVLOG(dev->dev_id, TLKM_LF_IRQ,                               \
		       "interrupt line %d/%d assigned with return value %d",   \
		       irqn, pci_irq_vector(pdev->pdev, irqn), err[nr]);       \
		INIT_WORK(&pdev->irq_work[nr],                                 \
			  aws_ec2_tlkm_pcie_slot_irq_work_##nr);               \
	}
	TLKM_AWS_EC2_SLOT_INTERRUPTS
#undef _INTR
	return 0;

irq_error:
#define _INTR(nr)                                                              \
	irqn = nr + pcie_cls.npirqs;                                           \
	if (!err[nr]) {                                                        \
		free_irq(pdev->irq_mapping[irqn], pdev->pdev);                 \
		pdev->irq_mapping[irqn] = -1;                                  \
	} else {                                                               \
		ret = err[nr];                                                 \
	}
	TLKM_AWS_EC2_SLOT_INTERRUPTS
#undef _INTR
	return ret;
}

void aws_ec2_pcie_irqs_exit(struct tlkm_device *dev)
{
	struct tlkm_pcie_device *pdev =
		(struct tlkm_pcie_device *)dev->private_data;
	int irqn;

#define _INTR(nr)                                                              \
	irqn = nr + pcie_cls.npirqs;                                           \
	if (pdev->irq_mapping[irqn] != -1) {                                   \
		DEVLOG(dev->dev_id, TLKM_LF_IRQ,                               \
		       "freeing interrupt %d with mappping %d", irqn,          \
		       pdev->irq_mapping[irqn]);                               \
		free_irq(pdev->irq_mapping[irqn], pdev->pdev);                 \
		pdev->irq_mapping[irqn] = -1;                                  \
	}
	TLKM_AWS_EC2_SLOT_INTERRUPTS
#undef _INTR

	DEVLOG(dev->dev_id, TLKM_LF_IRQ, "interrupts deactivated");
}

irqreturn_t intr_handler_platform(int irq, void *data)
{
	struct pcie_irq_mapping *mapping = (struct pcie_irq_mapping *)data;
	struct tlkm_pcie_device *dev = mapping->dev->private_data;
	tlkm_control_signal_platform_interrupt(dev->parent->ctrl,
					       mapping->irq_no);
	dev->ack_register[0] = mapping->irq_no;
	return IRQ_HANDLED;
}

int pcie_irqs_request_platform_irq(struct tlkm_device *dev, int irq_no)
{
	int err = 0;
	int i;
	struct tlkm_pcie_device *pdev =
		(struct tlkm_pcie_device *)dev->private_data;
	if (irq_no >= dev->cls->npirqs) {
		DEVERR(dev->dev_id,
		       "invalid platform interrupt number: %d (must be < %zd)",
		       irq_no, dev->cls->npirqs);
		return -ENXIO;
	}

	for (i = 0; i < TLKM_PLATFORM_INTERRUPTS; ++i) {
		if (pdev->irq_handler_helper[i].dev == 0) {
			pdev->irq_handler_helper[i].dev = dev;
			pdev->irq_handler_helper[i].irq_no = irq_no;

			DEVLOG(dev->dev_id, TLKM_LF_IRQ,
			       "requesting platform irq #%d", irq_no);
			if ((err = request_irq(
				     pci_irq_vector(pdev->pdev, irq_no),
				     intr_handler_platform, IRQF_EARLY_RESUME,
				     TLKM_PCI_NAME,
				     (void *)&pdev->irq_handler_helper[i]))) {
				DEVERR(dev->dev_id,
				       "could not request interrupt #%d: %d",
				       irq_no, err);
				return err;
			}
			pdev->irq_mapping[irq_no] =
				pci_irq_vector(pdev->pdev, irq_no);
			DEVLOG(dev->dev_id, TLKM_LF_IRQ,
			       "created mapping from interrupt %d -> %d",
			       irq_no, pdev->irq_mapping[irq_no]);
			DEVLOG(dev->dev_id, TLKM_LF_IRQ,
			       "interrupt line %d/%d assigned with return value %d",
			       irq_no, pci_irq_vector(pdev->pdev, irq_no), err);
			return err;
		}
	}

	DEVERR(dev->dev_id, "No interrupt mapping available.");
	return -ENOSPC;
}

void pcie_irqs_release_platform_irq(struct tlkm_device *dev, int irq_no)
{
	struct tlkm_pcie_device *pdev =
		(struct tlkm_pcie_device *)dev->private_data;
	int i;
	if (irq_no >= dev->cls->npirqs) {
		DEVERR(dev->dev_id,
		       "invalid platform interrupt number: %d (must be < %zd)",
		       irq_no, pcie_cls.npirqs);
		return;
	}

	for (i = 0; i < TLKM_PLATFORM_INTERRUPTS; ++i) {
		if (pdev->irq_handler_helper[i].irq_no == irq_no) {
			break;
		}
	}

	if (i == TLKM_PLATFORM_INTERRUPTS) {
		DEVERR(dev->dev_id, "Could not find mapping for interrupt %d",
		       irq_no);
		return;
	}

	DEVLOG(dev->dev_id, TLKM_LF_IRQ,
	       "freeing platform interrupt #%d with mapping %d", irq_no,
	       pdev->irq_mapping[irq_no]);
	if (pdev->irq_mapping[irq_no] != -1) {
		free_irq(pdev->irq_mapping[irq_no],
			 (void *)&pdev->irq_handler_helper[i]);
		pdev->irq_mapping[irq_no] = -1;
		pdev->irq_handler_helper[i].dev = 0;
		pdev->irq_handler_helper[i].irq_no = 0;
	}
}
