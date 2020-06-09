/*
 * This file is part of the IOTA Access Distribution
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

#include "user.h"

int User_init()
{
    return UserImplementation_init_cb();
}

void User_deinit()
{
    UserImplementation_deinit_cb();
}

void User_get_all(char* json_response)
{
    UserImplementation_get_all_cb(json_response);
}

void User_get_obj(const char* username, char* json_response)
{
    UserImplementation_get_obj_cb(username, json_response);
}

void User_put_obj(const char* user_object, char* json_response)
{
    UserImplementation_put_obj_cb(user_object, json_response);
}
void User_get_user_id(const char* username, char* json_response)
{
    UserImplementation_get_user_id_cb(username, json_response);
}
void User_clear_all(char* json_response)
{
    UserImplementation_clear_all_cb(json_response);
}
