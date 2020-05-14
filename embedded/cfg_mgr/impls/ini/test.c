/*
 * This file is part of the IOTA Access distribution
 * (https://github.com/iotaledger/access)
 *
 * Copyright (c) 2020 IOTA Foundation
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

#include "cfg_mgr.h"
#include "cfg_mgr_cmn.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

const char g_config_ini[] = 
"[module1]\n"
"option1=va1ue\n"
"option2=v4lue\n"
"[module2]\n"
"option1=m2vlue\n"
"option2=v8leu\n"
"option3=third\n";

int main()
{
    // create temporary config.ini file
    char file_name[] = "/tmp/config.ini.XXXXXX";
    int fd = mkstemp(file_name);
    printf("fd = %d\n", fd);
    write(fd, g_config_ini, strlen(g_config_ini));
    
    CfgMgr_t configuration;
    CfgMgr_init(file_name, &configuration);

    printf("config file contents:\n%s\n", configuration.data);
    char cfg_option_value[2048];
    int status = CfgMgr_get_option_string(&configuration, "module1", "option2", cfg_option_value);
    if (status != CFG_MGR_OK) printf("get module1->option2 failed!\nThis is NOT ok!\nstatus = %d\n", status);
    else printf("module1->option2: %s OK\n", cfg_option_value);

    status = CfgMgr_get_option_string(&configuration, "module2", "option3", cfg_option_value);
    if (status != CFG_MGR_OK) printf("get module2->option3 failed!\nThis is NOT ok!\nstatus = %d\n", status);
    else printf("module2->option3: %s OK\n", cfg_option_value);

    status = CfgMgr_get_option_string(&configuration, "module2", "option1", cfg_option_value);
    if (status != CFG_MGR_OK) printf("get module2->option1 failed!\nThis is NOT ok!\nstatus = %d\n", status);
    else printf("module2->option1: %s OK\n", cfg_option_value);
    
    status = CfgMgr_get_option_string(&configuration, "module1", "option3", cfg_option_value);
    if (status != CFG_MGR_OK) printf("get module1->option3 failed!\nThis is ok!\n");
    else printf("module1->option3: %s NOT ok\n", cfg_option_value);
    
    status = CfgMgr_get_option_string(&configuration, "module3", "option4", cfg_option_value);
    if (status != CFG_MGR_OK) printf("get module3->option4 failed!\nThis is ok!\n");
    else printf("module3->option4: %s NOT ok\n", cfg_option_value);
    
    status = CfgMgr_get_option_string(&configuration, "module3", "option1", cfg_option_value);
    if (status != CFG_MGR_OK) printf("get module3->option1 failed!\nThis is ok!\n");
    else printf("module3->option1: %s NOt ok\n", cfg_option_value);

    unlink(file_name);
    return 0;
}
