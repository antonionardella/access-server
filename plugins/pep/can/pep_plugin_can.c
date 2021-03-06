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
 * \file pep_plugin_can.c
 * \brief
 * PEP plugin for CAN demo. It uses relay board directly connected to rpi3 for control
 *
 * @Author Djordje Golubovic, Bernardo Araujo
 *
 * \notes
 *
 * \history
 * 04.03.2020. Initial version.
 * 15.07.2020. Renaming
 ****************************************************************************/

#include "pep_plugin_can.h"

#include <string.h>
#include <unistd.h>

#include "config_manager.h"
#include "dlog.h"
#include "relay_interface.h"
#include "time_manager.h"

#define RES_BUFF_LEN 80
#define MAX_ACTIONS 10
#define ACTION_NAME_SIZE 16
#define POLICY_ID_SIZE 64
#define ADDR_SIZE 128

typedef int (*action_t)(pdp_action_t* action);

typedef struct {
  char action_names[MAX_ACTIONS][ACTION_NAME_SIZE];
  action_t actions[MAX_ACTIONS];
  size_t count;
} action_set_t;

static action_set_t g_action_set;

static int car_lock(pdp_action_t* action) {
  relayinterface_pulse(0);
  return 0;
}

static int car_unlock(pdp_action_t* action) {
  relayinterface_pulse(1);
  return 0;
}

static int start_engine(pdp_action_t* action) {
  relayinterface_pulse(2);
  return 0;
}

static int open_trunk(pdp_action_t* action) {
  relayinterface_pulse(3);
  return 0;
}

static int destroy_cb(plugin_t* plugin, void* data) { free(plugin->callbacks); }

static int action_cb(plugin_t* plugin, void* data) {
  pep_plugin_args_t* args = (pep_plugin_args_t*)data;
  pdp_action_t* action = &args->action;
  char* obligation = args->obligation;
  char buf[RES_BUFF_LEN];
  int status = 0;

  // handle obligations
  // if (0 == memcmp(obligation, "obligation#1", strlen("obligation#1"))) {
  //}

  // execute action
  for (int i = 0; i < g_action_set.count; i++) {
    if (memcmp(action->value, g_action_set.action_names[i], strlen(g_action_set.action_names[i])) == 0) {
      timemanager_get_time_string(buf, RES_BUFF_LEN);
      dlog_printf("%s %s\t<Action performed>\n", buf, action->value);
      status = g_action_set.actions[i](action);
      break;
    }
  }
  return status;
}

int pep_plugin_can_initializer(plugin_t* plugin, void* options) {
  g_action_set.actions[0] = car_unlock;
  g_action_set.actions[1] = car_lock;
  g_action_set.actions[2] = open_trunk;
  g_action_set.actions[3] = start_engine;
  strncpy(g_action_set.action_names[0], "open_door", ACTION_NAME_SIZE);
  strncpy(g_action_set.action_names[1], "close_door", ACTION_NAME_SIZE);
  strncpy(g_action_set.action_names[2], "open_trunk", ACTION_NAME_SIZE);
  strncpy(g_action_set.action_names[3], "start_engine", ACTION_NAME_SIZE);
  g_action_set.count = 4;

  plugin->destroy = destroy_cb;
  plugin->callbacks = malloc(sizeof(void*) * PEP_PLUGIN_CALLBACK_COUNT);
  plugin->callbacks_num = PEP_PLUGIN_CALLBACK_COUNT;
  plugin->plugin_specific_data = NULL;
  plugin->callbacks[PEP_PLUGIN_ACTION_CB] = action_cb;

  return 0;
}
