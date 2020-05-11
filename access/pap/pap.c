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
/****************************************************************************
 * \project Decentralized Access Control
 * \file pap.c
 * \brief
 * Implementation of Policy Administration Point
 *
 * @Author Strahinja Golic
 *
 * \notes
 *
 * \history
 * 28.04.2020. Initial version.
 ****************************************************************************/

/****************************************************************************
 * INCLUDES
 ****************************************************************************/
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "pap.h"
#include "jsmn.h"
#include "utils_string.h"
#include "apiorig.h"
#include "sha256.h"

/****************************************************************************
 * MACROS
 ****************************************************************************/
#define PAP_MAX_TOKENS 1024
#define PAP_ECDSA_PK_SIZE 32
#define PAP_ASCII_SPACE 32
#define PAP_ASCII_TAB 9
#define PAP_ASCII_CR 13
#define PAP_ASCII_LF 10
#define PAP_SERVER_IP "127.0.0.1" //TODO: Set server IP
#define PAP_PORT 9998
#define PAP_PK_REQUEST "get_private_key"
#define PAP_PK_REQUEST_LEN strlen("get_private_key")
#define PAP_WAIT_TIME_S 10

#define PAP_CHECK_WHITESPACE(x) ((x == PAP_ASCII_SPACE) || (x == PAP_ASCII_TAB) || (x == PAP_ASCII_CR) || (x == PAP_ASCII_LF) ? TRUE : FALSE)

/****************************************************************************
 * GLOBAL VARIABLES
 ****************************************************************************/
static unsigned char public_key[PAP_PUBLIC_KEY_LEN];
static unsigned char private_key[PAP_PRIVATE_KEY_LEN];
static pthread_mutex_t pap_mutex;

/****************************************************************************
 * CALLBACK FUNCTIONS
 ****************************************************************************/
static put_fn callback_put = NULL;
static get_fn callback_get = NULL;
static has_fn callback_has = NULL;
static del_fn callback_del = NULL;

/****************************************************************************
 * LOCAL FUNCTIONS
 ****************************************************************************/
static void get_public_key_from_user(char *pk)
{
	int wait = PAP_WAIT_TIME_S;
	int sockfd = 0;
	struct sockaddr_in serv_addr;

	//Check input parameter
	if (pk == NULL)
	{
		printf("\nERROR[%s]: Bad input parameters.\n", __FUNCTION__);
		return;
	}

	//Create socket
	sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sockfd < 0)
	{
		printf("\nERROR[%s]: Socket creating failed.\n", __FUNCTION__);
		return;
	}

	memset(&serv_addr, '0', sizeof(serv_addr));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PAP_PORT);

	if (inet_pton(AF_INET, PAP_SERVER_IP, &serv_addr.sin_addr) <= 0)
	{
		printf("\nERROR[%s]: inet_pton failed.\n", __FUNCTION__);
		return;
	}

	//Connect to socket
	if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
	{
		printf("\nERROR[%s]: Connection failed.\n", __FUNCTION__);
		return;
	}

	while (wait)
	{
		//Request public key
		write(sockfd, PAP_PK_REQUEST, PAP_PK_REQUEST_LEN);

		//Read the response
		read(sockfd, pk, PAP_PUBLIC_KEY_LEN);

		//If no response is received, wait for 1s, then, try another request
		if (pk == NULL)
		{
			wait--;
			sleep(1);
		}
		else
		{
			wait = 0;
		}
	}
}

static int normalize_JSON_object(char *json_object, int object_len, char **json_object_normalized)
{
	char temp[object_len];
	int charCnt = 0;

	//Check input parameters
	if (json_object == NULL || object_len == 0)
	{
		printf("\nERROR[%s]: Bad input parameters.\n", __FUNCTION__);
		return 0;
	}

	//Normalize JSON object and save it to temp
	memset(temp, 0, object_len);

	for (int i = 0; i < object_len; i++)
	{
		if (!PAP_CHECK_WHITESPACE(json_object[i]))
		{
			temp[charCnt] = json_object[i];
			charCnt++;
		}
	}

	//Allocate memory for json_object_normalized
	if (*json_object_normalized)
	{
		free(*json_object_normalized);
		*json_object_normalized = NULL;
	}

	*json_object_normalized = malloc(charCnt * sizeof(char));
	if (*json_object_normalized == NULL)
	{
		return 0;
	}

	//Copy temp to json_object_normalized
	memcpy(*json_object_normalized, temp, charCnt * sizeof(char));

	return charCnt;
}

static void get_SHA256_hash(char *msg, int msg_len, char *hash_val)
{
	SHA256_CTX ctx;
	sha256_init(&ctx);
	sha256_update(&ctx, msg, msg_len);
	sha256_final(&ctx, hash_val);
}

/****************************************************************************
 * API FUNCTIONS
 ****************************************************************************/
PAP_error_e PAP_init(void)
{
	//Generate keypair
	crypto_sign_keypair(public_key, private_key);

	//Initalize mutex
	if (pthread_mutex_init(&pap_mutex, NULL) != 0)
	{
		printf("\nERROR[%s]: Mutex init failed.\n", __FUNCTION__);
		return PAP_ERROR;
	}
}

PAP_error_e PAP_term(void)
{
	//Destroy mutex
	pthread_mutex_destroy(&pap_mutex);
}

PAP_error_e PAP_register_callbacks(put_fn put, get_fn get, has_fn has, del_fn del)
{
	/*
	Plugin doesn't need to use all callbacks, so any of parrameters can be NULL.
	Therefore, we will not check input parameters at function entry.
	*/

	pthread_mutex_lock(&pap_mutex);

	callback_put = put;
	callback_get = get;
	callback_has = has;
	callback_del = del;

	pthread_mutex_unlock(&pap_mutex);

	return PAP_NO_ERROR;
}

PAP_error_e PAP_unregister_callbacks(void)
{
	pthread_mutex_lock(&pap_mutex);

	callback_put = NULL;
	callback_get = NULL;
	callback_has = NULL;
	callback_del = NULL;

	pthread_mutex_unlock(&pap_mutex);

	return PAP_NO_ERROR;
}

PAP_error_e PAP_add_policy(char *signed_policy, int signed_policy_size)
{
	char policy_id[PAP_POL_ID_MAX_LEN + 1] = {0};
	char policy_obj_hash[PAP_POL_ID_MAX_LEN + 1] = {0};
	char public_key[PAP_ECDSA_PK_SIZE] = {0};
	char signed_policy_id[PAP_SIGNATURE_LEN + PAP_POL_ID_MAX_LEN + 1] = {0};
	char *policy = NULL;
	char *policy_object_norm = NULL;
	int policy_object_norm_len = 0;
	int tok_num = 0;
	unsigned long long policy_size = 0;
	unsigned long long smlen;
	PAP_policy_object_t policy_object;
	PAP_policy_id_signature_t policy_id_signature;
	PAP_hash_functions_e hash_fn;
	jsmn_parser parser;
	jsmntok_t tokens[PAP_MAX_TOKENS];

	//Check input parameters
	if (signed_policy == NULL || signed_policy_size == 0)
	{
		printf("\nERROR[%s]: Bad input parameters.\n", __FUNCTION__);
		return PAP_ERROR;
	}

	pthread_mutex_lock(&pap_mutex);

	policy = malloc(signed_policy_size * sizeof(char)); //Worst case scenario

	//Verify policy signature
	get_public_key_from_user(public_key);
	if (crypto_sign_open(policy, &policy_size, signed_policy, signed_policy_size, public_key) != 0)
	{
		//Signature verification failed
		printf("\nERROR[%s]: Policy signature can not be verified.\n", __FUNCTION__);
		free(policy);
		pthread_mutex_unlock(&pap_mutex);
		return PAP_ERROR;

	}

	//Parse policy
	jsmn_init(&parser);

	tok_num = jsmn_parse(&parser, policy, policy_size, tokens, PAP_MAX_TOKENS);
	if (tok_num <= 0)
	{
		printf("\nERROR[%s]: Parsing policy failed.\n", __FUNCTION__);
		free(policy);
		pthread_mutex_unlock(&pap_mutex);
		return PAP_ERROR;
	}

	for (int i = 0; i < tok_num; i++)
	{
		//Get policy_id
		if (strncmp(&policy[tokens[i].start], "policy_id", strlen("policy_id")) == 0)
		{
			if ((tokens[i + 1].end - tokens[i + 1].start) <= PAP_POL_ID_MAX_LEN * 2)
			{
				if (str_to_hex(&policy[tokens[i + 1].start], policy_id, (tokens[i + 1].end - tokens[i + 1].start)) != UTILS_STRING_SUCCESS)
				{
					printf("\nERROR[%s]: Could not convert string to hex value.\n", __FUNCTION__);
					free(policy);
					pthread_mutex_unlock(&pap_mutex);
					return PAP_ERROR;
				}

				//If policy with this ID is already in the storage, abort the action
				if (callback_has)
				{
					if (callback_has(policy_id))
					{
						printf("\nERROR[%s]: This policy is already stored.\n", __FUNCTION__);
						free(policy);
						pthread_mutex_unlock(&pap_mutex);
						return PAP_ERROR;
					}
				}
			}
			else
			{
				printf("\nERROR[%s]: Size of policy id does not match supported hash functions.\n", __FUNCTION__);
				free(policy);
				pthread_mutex_unlock(&pap_mutex);
				return PAP_ERROR;
			}
		}

		//Get policy_object
		if (strncmp(&policy[tokens[i].start], "policy_object", strlen("policy_object")) == 0)
		{
			//Policy object shoudl be normalized
			policy_object_norm_len = normalize_JSON_object(&policy[tokens[i + 1].start], tokens[i + 1].end - tokens[i + 1].start, &policy_object_norm);

			policy_object.policy_object = policy_object_norm;
			policy_object.policy_object_size = policy_object_norm_len;
		}

		//Get hash_fn
		if (strncmp(&policy[tokens[i].start], "hash_function", strlen("hash_function")) == 0)
		{
			if (strncmp(&policy[tokens[i + 1].start], "sha-256", strlen("sha-256")) == 0)
			{
				hash_fn = PAP_SHA_256;

			}
			else
			{
				printf("\nERROR[%s]: Hash function not supported.\n", __FUNCTION__);
				free(policy_object_norm);
				free(policy);
				pthread_mutex_unlock(&pap_mutex);
				return PAP_ERROR;
			}
		}
	}

	//Check if policy ID responds to policy object
	get_SHA256_hash(policy_object_norm, policy_object_norm_len, policy_obj_hash);

	if (memcmp(policy_id, policy_obj_hash, PAP_POL_ID_MAX_LEN * sizeof(char)) != 0)
	{
		printf("\nERROR[%s]: Policy ID is not coresponding to policy object.\n", __FUNCTION__);
		free(policy_object_norm);
		free(policy);
		pthread_mutex_unlock(&pap_mutex);
		return PAP_ERROR;
	}

	//Signe policy ID and save signature, this will be use in order to verify policy on acquisition from storage
	crypto_sign(signed_policy_id, &smlen, policy_id, PAP_POL_ID_MAX_LEN, private_key);

	memcpy(policy_id_signature.public_key, public_key, PAP_PUBLIC_KEY_LEN * sizeof(char));
	memcpy(policy_id_signature.signature, signed_policy_id, PAP_SIGNATURE_LEN * sizeof(char)); //Signature is preppend to message
	policy_id_signature.signature_algorithm = PAP_ECDSA;

	//Put policy in storage
	if (callback_put != NULL)
	{
		callback_put(policy_id, policy_object, policy_id_signature, hash_fn);
	}
	else
	{
		printf("\nERROR[%s]: Callback is not registered.\n", __FUNCTION__);
		free(policy_object_norm);
		free(policy);
		pthread_mutex_unlock(&pap_mutex);
		return PAP_ERROR;
	}

	free(policy);
	pthread_mutex_unlock(&pap_mutex);
	return PAP_NO_ERROR;
}

PAP_error_e PAP_get_policy(char *policy_id, int policy_id_len, PAP_policy_t *policy)
{
	char policy_id_hex[PAP_POL_ID_MAX_LEN + 1] = {0};
	char calc_policy_id[PAP_POL_ID_MAX_LEN + 1] = {0};
	char signed_policy_id[PAP_SIGNATURE_LEN + PAP_POL_ID_MAX_LEN + 1] = {0};
	unsigned long long smlen;

	//Check input parameters
	if (policy_id == NULL || policy_id_len == 0 || policy == NULL)
	{
		printf("\nERROR[%s]: Bad input parameters.\n", __FUNCTION__);
		return PAP_ERROR;
	}

	pthread_mutex_lock(&pap_mutex);

	//Get policy ID hex value
	if (str_to_hex(policy_id, policy_id_hex, policy_id_len) != UTILS_STRING_SUCCESS)
	{
		printf("\nERROR[%s]: Could not convert string to hex value.\n", __FUNCTION__);
		pthread_mutex_unlock(&pap_mutex);
		return PAP_ERROR;
	}

	memcpy(policy->policy_ID, policy_id_hex, PAP_POL_ID_MAX_LEN + 1);

	//Get policy from storage
	if (callback_get != NULL)
	{
		callback_get(policy_id_hex, &(policy->policy_object), &(policy->policy_id_signature), &(policy->hash_function));
	}
	else
	{
		printf("\nERROR[%s]: Callback is not registered.\n", __FUNCTION__);
		pthread_mutex_unlock(&pap_mutex);
		return PAP_ERROR;
	}

	//Check if policy_id_signature is valid
	if (policy->policy_id_signature.signature_algorithm == PAP_ECDSA)
	{
		//Calculate policy object hash
		if (policy->hash_function == PAP_SHA_256)
		{
			get_SHA256_hash(policy->policy_object.policy_object, policy->policy_object.policy_object_size, calc_policy_id);

			//Sign policy ID with module private key
			crypto_sign(signed_policy_id, &smlen, calc_policy_id, PAP_POL_ID_MAX_LEN, private_key);

			//Check if that signature matches with acquired one
			if (memcmp(signed_policy_id, policy->policy_id_signature.signature, PAP_SIGNATURE_LEN) != 0)
			{
				printf("\nERROR[%s]: Invalid policy ID signature.\n", __FUNCTION__);
				pthread_mutex_unlock(&pap_mutex);
				return PAP_ERROR;
			}
		}
		else
		{
			printf("\nERROR[%s]: Invalid policy ID signature.\n", __FUNCTION__);
			pthread_mutex_unlock(&pap_mutex);
			return PAP_ERROR;
		}
	}
	else
	{
		printf("\nERROR[%s]: Invalid policy ID signature.\n", __FUNCTION__);
		pthread_mutex_unlock(&pap_mutex);
		return PAP_ERROR;
	}

	pthread_mutex_unlock(&pap_mutex);
	return PAP_NO_ERROR;
}

bool PAP_has_policy(char *policy_id, int policy_id_len)
{
	char policy_id_hex[PAP_POL_ID_MAX_LEN + 1] = {0};

	//Check input parameters
	if (policy_id == NULL || policy_id_len == 0)
	{
		printf("\nERROR[%s]: Bad input parameters.\n", __FUNCTION__);
		return FALSE;
	}

	pthread_mutex_lock(&pap_mutex);

	//Get policy ID hex value
	if (str_to_hex(policy_id, policy_id_hex, policy_id_len) != UTILS_STRING_SUCCESS)
	{
		printf("\nERROR[%s]: Could not convert string to hex value.\n", __FUNCTION__);
		pthread_mutex_unlock(&pap_mutex);
		return FALSE;
	}

	//Check if policy is already in the storage
	if (callback_has != NULL)
	{
		pthread_mutex_unlock(&pap_mutex);
		return callback_has(policy_id_hex);
	}
	else
	{
		printf("\nERROR[%s]: Callback is not registered.\n", __FUNCTION__);
		pthread_mutex_unlock(&pap_mutex);
		return FALSE;
	}
}

PAP_error_e PAP_remove_policy(char *policy_id, int policy_id_len)
{
	char policy_id_hex[PAP_POL_ID_MAX_LEN + 1] = {0};
	PAP_policy_t policy;

	//Check input parameters
	if (policy_id == NULL || policy_id_len == 0)
	{
		printf("\nERROR[%s]: Bad input parameters.\n", __FUNCTION__);
		return PAP_ERROR;
	}

	pthread_mutex_lock(&pap_mutex);

	//Get policy ID hex value
	if (str_to_hex(policy_id, policy_id_hex, policy_id_len) != UTILS_STRING_SUCCESS)
	{
		printf("\nERROR[%s]: Could not convert string to hex value.\n", __FUNCTION__);
		pthread_mutex_unlock(&pap_mutex);
		return PAP_ERROR;
	}

	//Get policy from storage
	if (callback_get != NULL)
	{
		callback_get(policy_id_hex, &(policy.policy_object), &(policy.policy_id_signature), &(policy.hash_function));
		//Free policy object
		free(policy.policy_object.policy_object);
	}

	//Delete policy from storage
	if (callback_del != NULL)
	{
		callback_del(policy_id_hex);
	}
	else
	{
		printf("\nERROR[%s]: Callback is not registered.\n", __FUNCTION__);
		pthread_mutex_unlock(&pap_mutex);
		return PAP_ERROR;
	}

	pthread_mutex_unlock(&pap_mutex);
	return PAP_NO_ERROR;
}