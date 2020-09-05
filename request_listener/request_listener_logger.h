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

#ifndef REQUEST_LISTENER_LOGGER_H
#define REQUEST_LISTENER_LOGGER_H

#include "utils/logger_helper.h"

/**
 * @brief logger ID
 *
 */
extern logger_id_t request_listener_logger_id;

/**
 * @brief init Request Listener logger
 *
 * @param[in] level A level of the logger
 *
 */
void logger_init_request_listener(logger_level_t level);

/**
 * @brief cleanup Request Listener logger
 *
 */
void logger_destroy_request_listener();

#endif  // REQUEST_LISTENER_LOGGER_H
