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
 * \file pep_plugin_print.h
 * \brief
 * PEP plugin that only prints on the terminal (for development purposes).
 *
 * @Author Bernardo Araujo
 *
 * \notes
 *
 * \history
 * 17.07.2020. Initial version.
 ****************************************************************************/

#ifndef _PEP_PLUGIN_PRINT_H_
#define _PEP_PLUGIN_PRINT_H_

#include "pep_plugin.h"

/**
 * @fn void pep_plugin_print_initializer(plugin_t* plugin)
 * @brief resolver plugin initializer callback
 */
int pep_plugin_print_initializer(plugin_t* plugin, void* options);

#endif //_PEP_PLUGIN_PRINT_H_
