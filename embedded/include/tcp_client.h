/*
 * This file is part of the Frost distribution
 * (https://github.com/xainag/frost)
 *
 * Copyright (c) 2019 XAIN AG.
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
 * \project Decentralized Access Control
 * \file tcp_client.h
 * \brief
 * Communication with user
 *
 * @Author
 *
 * \notes
 *
 * \history
 * XX.YY.ZZZZ. Initial version.
 ****************************************************************************/

int tcp_client_send(char *msg, int msg_length, char *rec, int *rec_length, char *servip, int port);

int read_AWS_response(void *ext, char *recvBuffer);
