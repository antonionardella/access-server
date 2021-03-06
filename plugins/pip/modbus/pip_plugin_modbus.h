/*
 * This file is part of the IOTA Access distribution
 * (https://github.com/iotaledger/access)
 *
 * Copyright (c) 2020 IOTA Stiftung
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/****************************************************************************
 * \project IOTA Access
 * \file pip_plugin_modbus.h
 * \brief
 * Modbus receiver module interface
 *
 * @Author Djordje Golubovic, Bernardo Araujo
 *
 * \notes
 *
 * \history
 * 07.29.2019. Initial version.
 * 15.07.2020. Renaming.
 ****************************************************************************/

#ifndef _PIP_PLUGIN_MODBUS_H_
#define _PIP_PLUGIN_MODBUS_H_

#include <libfastjson/json.h>
#include "modbus.h"
#include "vehicle_dataset.h"

void modbusreceiver_init(canopen01_vehicle_dataset_t *dataset, pthread_mutex_t *json_mutex);
int modbusreceiver_start();
void modbusreceiver_stop();

#endif  //_PIP_PLUGIN_MODBUS_H_
