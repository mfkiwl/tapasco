//
// Copyright (C) 2014-2018 Jens Korinth, TU Darmstadt
//
// This file is part of Tapasco (TPC).
//
// Tapasco is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Tapasco is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Tapasco.  If not, see <http://www.gnu.org/licenses/>.
//
//! @file	tlkm_logging.h
//! @brief	Kernel logging for TaPaSCo unified loadable kernel module.
//! @authors	J. Korinth, TU Darmstadt (jk@esa.cs.tu-darmstadt.de)
//!
#ifndef TLKM_LOGGING_H__
#define TLKM_LOGGING_H__

#include <linux/printk.h>

#ifdef _LF
	#undef _LF
#endif

#define TLKM_LOGFLAGS \
	_LF(MODULE    , (1 << 1)) \
	_LF(DEVICE    , (1 << 2)) \
	_LF(IOCTL     , (1 << 3)) \
	_LF(DMAMGMT   , (1 << 4)) \
	_LF(FOPS      , (1 << 5)) \
	_LF(IRQ       , (1 << 6)) \
	_LF(ASYNC     , (1 << 7)) \
	_LF(PERFC     , (1 << 8))

typedef enum {
#define _LF(name, level) TLKM_LF_##name = level,
TLKM_LOGFLAGS
#undef _LF
} tlkm_lf_t;

#ifndef NDEBUG
#define SSTR(V)					#V
#define STR(V)					SSTR(V)

extern ulong tlkm_logging_flags;

#define ERR(msg, ...)		tlkm_log(0, msg, ##__VA_ARGS__)
#define WRN(msg, ...)		tlkm_log(1, msg, ##__VA_ARGS__)
#define LOG(l, msg, ...)	tlkm_log((l), msg, ##__VA_ARGS__)

#define tlkm_log(level, fmt, ...) do { \
		switch ((int)level) { \
		case 0: \
			printk(KERN_ERR "tapasco: [%s] " \
					fmt "\n", __func__, \
					##__VA_ARGS__); \
			break; \
		case 1: \
			printk(KERN_WARNING "tapasco: [%s] " \
					fmt "\n", __func__, \
					##__VA_ARGS__); \
			break; \
		default: \
			if (tlkm_logging_flags & level) \
				printk(KERN_NOTICE "tapasco: [%s] " \
						fmt "\n", __func__, \
						##__VA_ARGS__); \
			break; \
		} \
	} while(0)
#else
/* only errors and warnings, no other messages */
#define ERR(fmt, ...)		printk(KERN_ERR "tapasco: [%s] " \
					fmt "\n", __func__, \
					##__VA_ARGS__)

#define WRN(fmt, ...)		printk(KERN_WARNING "tapasco: [%s] " \
					fmt "\n", __func__, \
					##__VA_ARGS__)

#define LOG(l, msg, ...)
#define tlkm_log(level, fmt, ...)
#endif

#endif /* TLKM_LOGGING_H__ */
