//
// Copyright (C) 2014 Jens Korinth, TU Darmstadt
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
/**
 *  @file	tapasco_status.h
 *  @brief	
 *  @author	J. Korinth, TU Darmstadt (jk@esa.cs.tu-darmstadt.de)
 **/
#ifndef TAPASCO_STATUS_H__
#define TAPASCO_STATUS_H__

#include <tapasco.h>
#include <tapasco_functions.h>

typedef struct tapasco_status tapasco_status_t;
struct tapasco_status {
	tapasco_func_id_t id[TAPASCO_MAX_INSTANCES];
};

tapasco_res_t tapasco_status_init(tapasco_status_t **status);
void tapasco_status_deinit(tapasco_status_t *status);

#endif /* TAPASCO_STATUS_H__ */
