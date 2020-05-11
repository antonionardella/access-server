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
 * \file pdp.c
 * \brief
 * Implementation of Policy Decision Point
 *
 * @Author Dejan Nedic, Milivoje Knezevic, Strahinja Golic
 *
 *
 * \notes
 *
 * \history
 * 04.09.2018. Initial version.
 * 09.11.2018 Modified to work together with PIP module
 * 21.02.2020. Added obligations functionality.
 * 05.05.20200 Refactoring
 ****************************************************************************/

/****************************************************************************
 * INCLUDES
 ****************************************************************************/
#include <string.h>
#include <stdlib.h>
#include "pdp.h"
#include "json_parser.h"
#include "Dlog.h"
#include "pip.h"
#include "pap.h"

/****************************************************************************
 * MACROS
 ****************************************************************************/
#define PDP_DATA_VAL_SIZE 131
#define PDP_DATA_TYPE_SIZE 21
#define PDP_STRTOUL_BASE 10

/* If any hash function, which provides hashes longer than 256 bits, is to be used,
this will have to be adjusted accordingly. */
#define PDP_POL_ID_MAX_LEN 32

/****************************************************************************
 * LOCAL FUNCTIONS DECLARATION
 ****************************************************************************/
static int resolve_attribute(policy_t *pol, int atribute_position);

/****************************************************************************
 * GLOBAL VARIABLES
 ****************************************************************************/
static char data_value[PDP_DATA_VAL_SIZE];
static char data_type[PDP_DATA_TYPE_SIZE];

/****************************************************************************
 * LOCAL FUNCTIONS DEFINITION
 ****************************************************************************/
static PDP_operation_e get_operation_new(const char *operation, int size)
{
	PDP_operation_e ret = UNDEFINED;

	if (size == 2)
	{
		if(memcmp("or", operation, size) == 0)
			ret = OR;
		else if(memcmp("eq", operation, size) == 0)
			ret = EQ;
		else if (memcmp("lt", operation, size) == 0)
			ret = LT;
		else if (memcmp("gt", operation, size) == 0)
			ret = GT;
		else if (memcmp("if", operation, size) == 0)
			ret = IF;
	}
	else if (size == 3)
	{
		if(memcmp("and", operation, size) == 0)
			ret = AND;
		else if(memcmp("not", operation, size) == 0)
			ret = NOT;
		else if(memcmp("leq", operation, size) == 0)
			ret = LEQ;
		else if(memcmp("geq", operation, size) == 0)
			ret = GEQ;
	}

	return ret;
}

static int and(policy_t *pol, int attribute_list)
{
	int decision = TRUE;

	int i = 0;
	int res;
	int attribute_count = 0;
	int attribute = -1;

	attribute_count = get_array_size(attribute_list);

	for (i = 0; i < attribute_count; i++)
	{
		attribute = get_attribute_from_array(attribute_list, i);

		res = resolve_attribute(pol, attribute_list + attribute);

		if(res != ERROR)
		{
			decision = decision && res;
		}
		else
		{
			decision = FALSE;
			break;
		}
	}

	return decision;
}

static int or(policy_t *pol, int attribute_list)
{
	int decision = FALSE;

	int i = 0;
	int res;
	int attribute_count = 0;
	int attribute = -1;

	attribute_count = get_array_size(attribute_list);

	for (i = 0; i < attribute_count; i++)
	{
		attribute = get_attribute_from_array(attribute_list, i);

		res = resolve_attribute(pol, attribute_list + attribute);

		if(res != ERROR)
		{
			decision = decision || res;
		}
		else
		{
			decision = FALSE;
			break;
		}
	}

	return decision;
}

static int eq(policy_t *pol, int attribute_list)
{
	int ret = FALSE;

	int attr1 = -1;
	int attr2 = -1;

	attr1 = get_attribute_from_array(attribute_list, 0);
	attr2 = get_attribute_from_array(attribute_list, 1);

	int type1 = json_get_value(pol->policy_c, attribute_list + attr1, "type");
	int type2 = json_get_value(pol->policy_c, attribute_list + attr2, "type");
	int value1 = json_get_value(pol->policy_c, attribute_list + attr1, "value");
	int value2 = json_get_value(pol->policy_c, attribute_list + attr2, "value");

	char *url_type = pol->policy_c + get_start_of_token(type2);
	char *url_value = pol->policy_c + get_start_of_token(value2);

	int url_size = get_size_of_token(value2);
	int url_sizet = get_size_of_token(type2);


	int data_length = 0;

	data_value[PDP_DATA_VAL_SIZE - 1] = '\0';
	data_length = pip_get_data(pol, url_value, data_value, url_size);

	if(data_length == -1)
	{
		memcpy(data_value, url_value, get_size_of_token(value2) );
		data_value[get_size_of_token(value2)] = '\0';
	}

	int type_length = 0;

	data_type[PDP_DATA_TYPE_SIZE - 1] = '\0';
	type_length = pip_get_data(pol, url_type, data_type, url_sizet);

	if(type_length == -1)
	{
		memcpy(data_type, url_type, get_size_of_token(type2));
		data_type[get_size_of_token(type2)] = '\0';
	}

	//*******************************************************************
	//PIP FOR SUBJECT REQUEST (this should be modified after PIP module is changed so it can return subject value and type
	if(data_length == -2)
	{
		memcpy(data_value, pol->policy_c + get_start_of_token(value1), get_size_of_token(value1) );
		data_value[get_size_of_token(value1)] = '\0';
	}

	if(type_length == -2)
	{
		memcpy(data_type, pol->policy_c + get_start_of_token(type1),get_size_of_token(type1));
		data_type[get_size_of_token(type1)] = '\0';
	}
	//*********************************************************************

	int size_of_type1 = get_size_of_token(type1);
	int size_of_type2 = strlen(data_type);
	int size_of_value1 = get_size_of_token(value1);
	int size_of_value2 = strlen(data_value);


	if(((size_of_type1 == size_of_type2) && (strncasecmp(pol->policy_c + get_start_of_token(type1), data_type, size_of_type1) == 0 )) &&
			((size_of_value1 == size_of_value2) && (strncasecmp(pol->policy_c + get_start_of_token(value1), data_value, size_of_value1) == 0 )))
	{
		ret = TRUE;
	}
	else
	{
		Dlog_printf("\n FALSE \n");
	}

	return ret;
}

static int leq(policy_t *pol, int attribute_list)
{
	int ret = FALSE;

	int attr1 = -1;
	int attr2 = -1;

	attr1 = get_attribute_from_array(attribute_list, 0);
	attr2 = get_attribute_from_array(attribute_list, 1);

	int type1 = json_get_value(pol->policy_c, attribute_list + attr1, "type");
	int type2 = json_get_value(pol->policy_c, attribute_list + attr2, "type");
	int value1 = json_get_value(pol->policy_c, attribute_list + attr1, "value");
	int value2 = json_get_value(pol->policy_c, attribute_list + attr2, "value");

	char *url_type = pol->policy_c + get_start_of_token(type2);
	char *url_value = pol->policy_c + get_start_of_token(value2);

	int url_size = get_size_of_token(value2);
	int url_sizet = get_size_of_token(type2);


	int data_length = 0;

	data_value[PDP_DATA_VAL_SIZE - 1] = '\0';
	data_length = pip_get_data(pol, url_value, data_value, url_size);



	if(data_length == -1)
	{
		memcpy(data_value, url_value, get_size_of_token(value2) );
		data_value[get_size_of_token(value2)] = '\0';
	}

	int type_length = 0;


	data_type[PDP_DATA_TYPE_SIZE - 1] = '\0';
	type_length = pip_get_data(pol, url_type,data_type,url_sizet);


	if(type_length == -1)
	{
		memcpy(data_type,url_type,get_size_of_token(type2));
		data_type[get_size_of_token(type2)] = '\0';
	}

	//*******************************************************************
	//PIP FOR SUBJECT REQUEST (this should be modified after PIP module is changed so it can return subject value and type
	if(data_length == -2)
	{
		memcpy(data_value, pol->policy_c + get_start_of_token(value1), get_size_of_token(value1) );
		data_value[get_size_of_token(value1)] = '\0';
	}

	if(type_length == -2)
	{
		memcpy(data_type, pol->policy_c + get_start_of_token(type1),get_size_of_token(type1));
		data_type[get_size_of_token(type1)] = '\0';
	}
	//*********************************************************************

	int size_of_type1 = get_size_of_token(type1);
	int size_of_type2 = strlen(data_type);
	int size_of_value1 = get_size_of_token(value1);
	int size_of_value2 = strlen(data_value);

	if(((size_of_type1 == size_of_type2) && (strncasecmp(pol->policy_c + get_start_of_token(type1), data_type, size_of_type1) == 0 )))
	{
		if(size_of_value1 < size_of_value2)
		{
			ret = TRUE;
		}
		else if((size_of_value1 == size_of_value2) && strncasecmp(pol->policy_c + get_start_of_token(value1), data_value, size_of_value1) <= 0 )
		{
			ret = TRUE;
		}
	}

	return ret;
}

static int lt(policy_t *pol, int attribute_list)
{
	int ret = FALSE;

	int attr1 = -1;
	int attr2 = -1;

	attr1 = get_attribute_from_array(attribute_list, 0);
	attr2 = get_attribute_from_array(attribute_list, 1);

	int type1 = json_get_value(pol->policy_c, attribute_list + attr1, "type");
	int type2 = json_get_value(pol->policy_c, attribute_list + attr2, "type");
	int value1 = json_get_value(pol->policy_c, attribute_list + attr1, "value");
	int value2 = json_get_value(pol->policy_c, attribute_list + attr2, "value");

	char *url_type = pol->policy_c + get_start_of_token(type2);
	char *url_value = pol->policy_c + get_start_of_token(value2);

	int url_size = get_size_of_token(value2);
	int url_sizet = get_size_of_token(type2);


	int data_length = 0;

	data_value[PDP_DATA_VAL_SIZE - 1] = '\0';
	data_length = pip_get_data(pol, url_value, data_value, url_size);



	if(data_length == -1)
	{
		memcpy(data_value, url_value, get_size_of_token(value2) );
		data_value[get_size_of_token(value2)] = '\0';
	}

	int type_length = 0;


	data_type[PDP_DATA_TYPE_SIZE - 1] = '\0';
	type_length = pip_get_data(pol, url_type,data_type,url_sizet);


	if(type_length == -1)
	{
		memcpy(data_type,url_type,get_size_of_token(type2));
		data_type[get_size_of_token(type2)] = '\0';
	}

	//*******************************************************************
	//PIP FOR SUBJECT REQUEST (this should be modified after PIP module is changed so it can return subject value and type
	if(data_length == -2)
	{
		memcpy(data_value, pol->policy_c + get_start_of_token(value1), get_size_of_token(value1) );
		data_value[get_size_of_token(value1)] = '\0';
	}

	if(type_length == -2)
	{
		memcpy(data_type, pol->policy_c + get_start_of_token(type1),get_size_of_token(type1));
		data_type[get_size_of_token(type1)] = '\0';
	}
	//*********************************************************************

	int size_of_type1 = get_size_of_token(type1);
	int size_of_type2 = strlen(data_type);
	int size_of_value1 = get_size_of_token(value1);
	int size_of_value2 = strlen(data_value);

	if(((size_of_type1 == size_of_type2) && (strncasecmp(pol->policy_c + get_start_of_token(type1), data_type, size_of_type1) == 0 )))
	{
		if(size_of_value1 < size_of_value2)
		{
			ret = TRUE;
		}
		else if((size_of_value1 == size_of_value2) && strncasecmp(pol->policy_c + get_start_of_token(value1), data_value, size_of_value1) < 0 )
		{
			ret = TRUE;
		}
	}

	return ret;
}

static int geq(policy_t *pol, int attribute_list)
{
	int ret = FALSE;

	int attr1 = -1;
	int attr2 = -1;

	attr1 = get_attribute_from_array(attribute_list, 0);
	attr2 = get_attribute_from_array(attribute_list, 1);

	int type1 = json_get_value(pol->policy_c, attribute_list + attr1, "type");
	int type2 = json_get_value(pol->policy_c, attribute_list + attr2, "type");
	int value1 = json_get_value(pol->policy_c, attribute_list + attr1, "value");
	int value2 = json_get_value(pol->policy_c, attribute_list + attr2, "value");

	char *url_type = pol->policy_c + get_start_of_token(type2);
	char *url_value = pol->policy_c + get_start_of_token(value2);

	int url_size = get_size_of_token(value2);
	int url_sizet = get_size_of_token(type2);


	int data_length = 0;

	data_value[PDP_DATA_VAL_SIZE - 1] = '\0';
	data_length = pip_get_data(pol, url_value, data_value, url_size);



	if(data_length == -1)
	{
		memcpy(data_value, url_value, get_size_of_token(value2) );
		data_value[get_size_of_token(value2)] = '\0';
	}

	int type_length = 0;


	data_type[PDP_DATA_TYPE_SIZE - 1] = '\0';
	type_length = pip_get_data(pol, url_type,data_type,url_sizet);


	if(type_length == -1)
	{
		memcpy(data_type,url_type,get_size_of_token(type2));
		data_type[get_size_of_token(type2)] = '\0';
	}

	//*******************************************************************
	//PIP FOR SUBJECT REQUEST (this should be modified after PIP module is changed so it can return subject value and type
	if(data_length == -2)
	{
		memcpy(data_value, pol->policy_c + get_start_of_token(value1), get_size_of_token(value1) );
		data_value[get_size_of_token(value1)] = '\0';
	}

	if(type_length == -2)
	{
		memcpy(data_type, pol->policy_c + get_start_of_token(type1),get_size_of_token(type1));
		data_type[get_size_of_token(type1)] = '\0';
	}
	//*********************************************************************

	int size_of_type1 = get_size_of_token(type1);
	int size_of_type2 = strlen(data_type);
	int size_of_value1 = get_size_of_token(value1);
	int size_of_value2 = strlen(data_value);

	if(((size_of_type1 == size_of_type2) && (strncasecmp(pol->policy_c + get_start_of_token(type1), data_type, size_of_type1) == 0 )))
	{
		if(size_of_value1 > size_of_value2)
		{
			ret = TRUE;
		}
		else if((size_of_value1 == size_of_value2) && strncasecmp(pol->policy_c + get_start_of_token(value1), data_value, size_of_value1) >= 0 )
		{
			ret = TRUE;
		}
	}

	return ret;
}

static int gt(policy_t *pol, int attribute_list)
{
	int ret = FALSE;

	int attr1 = -1;
	int attr2 = -1;

	attr1 = get_attribute_from_array(attribute_list, 0);
	attr2 = get_attribute_from_array(attribute_list, 1);

	int type1 = json_get_value(pol->policy_c, attribute_list + attr1, "type");
	int type2 = json_get_value(pol->policy_c, attribute_list + attr2, "type");
	int value1 = json_get_value(pol->policy_c, attribute_list + attr1, "value");
	int value2 = json_get_value(pol->policy_c, attribute_list + attr2, "value");

	char *url_type = pol->policy_c + get_start_of_token(type2);
	char *url_value = pol->policy_c + get_start_of_token(value2);

	int url_size = get_size_of_token(value2);
	int url_sizet = get_size_of_token(type2);


	int data_length = 0;

	data_value[PDP_DATA_VAL_SIZE - 1] = '\0';
	data_length = pip_get_data(pol, url_value,data_value, url_size);



	if(data_length == -1)
	{
		memcpy(data_value, url_value, get_size_of_token(value2) );
		data_value[get_size_of_token(value2)] = '\0';
	}

	int type_length = 0;


	data_type[PDP_DATA_TYPE_SIZE - 1] = '\0';
	type_length = pip_get_data(pol, url_type,data_type,url_sizet);


	if(type_length == -1)
	{
		memcpy(data_type,url_type,get_size_of_token(type2));
		data_type[get_size_of_token(type2)] = '\0';
	}

	//*******************************************************************
	//PIP FOR SUBJECT REQUEST (this should be modified after PIP module is changed so it can return subject value and type
	if(data_length == -2)
	{
		memcpy(data_value, pol->policy_c + get_start_of_token(value1), get_size_of_token(value1) );
		data_value[get_size_of_token(value1)] = '\0';
	}

	if(type_length == -2)
	{
		memcpy(data_type, pol->policy_c + get_start_of_token(type1),get_size_of_token(type1));
		data_type[get_size_of_token(type1)] = '\0';
	}
	//*********************************************************************

	int size_of_type1 = get_size_of_token(type1);
	int size_of_type2 = strlen(data_type);
	int size_of_value1 = get_size_of_token(value1);
	int size_of_value2 = strlen(data_value);

	if(((size_of_type1 == size_of_type2) && (strncasecmp(pol->policy_c + get_start_of_token(type1), data_type, size_of_type1) == 0 )))
	{
		if(size_of_value1 > size_of_value2)
		{
			ret = TRUE;
		}
		else if((size_of_value1 == size_of_value2) && strncasecmp(pol->policy_c + get_start_of_token(value1), data_value, size_of_value1) > 0 )
		{
			ret = TRUE;
		}
	}

	return ret;
}

static void get_time_from_attr(policy_t *pol, int atribute_position, PDP_operation_e attr_operation, unsigned long *start_time, unsigned long *end_time)
{
	if(pol == NULL)
	{
		Dlog_printf("\n\nERROR[%s]: Wrong input parameters\n\n", __FUNCTION__);
		return;
	}

	if(start_time == NULL || end_time == NULL)
	{
		// Not an error. Sometimes, time is not needed
		return;
	}

	int operation = json_get_token_index_from_pos(pol->policy_c, atribute_position, "operation");
	int operation_start = -1;
	int operation_end = -1;
	int attribute_list = -1;
	int i = 0;
	PDP_operation_e opt;

	if((operation != -1) && (get_start_of_token(operation) < get_end_of_token(atribute_position)))// Check only operations within this json object
	{
		attribute_list = json_get_token_index_from_pos(pol->policy_c, atribute_position, "attribute_list");

		// Check if operation is listed before or after attribure list
		if(attribute_list >= 0 && operation > attribute_list)
		{
			// If attribute list is listed first, get operation given after att. list

			int number_of_tokens = get_token_num();
			int tok_cnt = attribute_list;

			while((get_end_of_token(attribute_list) > get_start_of_token(operation)) && (tok_cnt <= number_of_tokens) && (tok_cnt >= 0))
			{
				operation = json_get_token_index_from_pos(pol->policy_c, tok_cnt, "operation");
				tok_cnt = operation;
			}
		}

		operation_start = get_start_of_token(operation);
		operation_end = get_end_of_token(operation);

		opt = get_operation_new(pol->policy_c + operation_start, operation_end - operation_start);

		i = 0;
		while(i < get_array_size(attribute_list))
		{
			get_time_from_attr(pol, get_array_member(attribute_list, i), opt, start_time, end_time);
			i++;
		}
	}
	else
	{
		int type = json_get_token_index_from_pos(pol->policy_c, atribute_position, "type");
		if((type != -1) && (get_start_of_token(type) < get_end_of_token(atribute_position)))// Check only type within this json object
		{
			int start = get_start_of_token(type);
			int size_of_type = get_size_of_token(type);

			if((size_of_type == strlen("time")) && (strncasecmp(pol->policy_c + start, "time", strlen("time")) == 0))
			{
				int value = json_get_token_index_from_pos(pol->policy_c, atribute_position, "value");
				int start_of_value = get_start_of_token(value);
				int size_of_value = get_size_of_token(value);
				char *val_str = malloc(sizeof(char) * size_of_value + 1);
				memcpy(val_str, pol->policy_c + start_of_value, size_of_value);
				val_str[size_of_value] = '\0';

				switch(attr_operation)
				{
					case EQ:
					{
						*start_time = strtoul(val_str, NULL, PDP_STRTOUL_BASE);
						*end_time = *start_time;
						break;
					}
					case LEQ:
					{
						*end_time = strtoul(val_str, NULL, PDP_STRTOUL_BASE);
						break;
					}
					case GEQ:
					{
						*start_time = strtoul(val_str, NULL, PDP_STRTOUL_BASE);
						break;
					}
					case LT:
					{
						*end_time = strtoul(val_str, NULL, PDP_STRTOUL_BASE) - 1; // Must be less then value
						break;
					}
					case GT:
					{
						*start_time = strtoul(val_str, NULL, PDP_STRTOUL_BASE) + 1; // Must be greater then value
						break;
					}
					default:
					{
						break;
					}
				}

				free(val_str);
			}
		}
	}
}

static int resolve_attribute(policy_t *pol, int atribute_position)
{
	int ret = -1;
	PDP_operation_e opt;

	int operation = json_get_token_index_from_pos(pol->policy_c, atribute_position, "operation");
	int operation_start = -1;
	int operation_end = -1;

	int attribute_list = -1;

	if((operation != -1) && (get_start_of_token(operation) < get_end_of_token(atribute_position)))// Check only operations within this json object
	{
		attribute_list = json_get_token_index_from_pos(pol->policy_c, atribute_position, "attribute_list");

		// Check if operation is listed before or after attribure list
		if(attribute_list >= 0 && operation > attribute_list)
		{
			// If attribute list is listed first, get operation given after att. list

			int number_of_tokens = get_token_num();
			int tok_cnt = attribute_list;

			while((get_end_of_token(attribute_list) > get_start_of_token(operation)) && (tok_cnt <= number_of_tokens) && (tok_cnt >= 0))
			{
				operation = json_get_token_index_from_pos(pol->policy_c, tok_cnt, "operation");
				tok_cnt = operation;
			}
		}

		operation_start = get_start_of_token(operation);
		operation_end = get_end_of_token(operation);

		opt = get_operation_new(pol->policy_c + operation_start, operation_end - operation_start);

		switch(opt)
		{
			case OR:
			{
				ret = or(pol, attribute_list);
				break;
			}
			case AND:
			{
				ret = and(pol, attribute_list);
				break;
			}
			case EQ:
			{
				ret = eq(pol, attribute_list);
				break;
			}
			case LEQ:
			{
				ret = leq(pol, attribute_list);
				break;
			}
			case GEQ:{
				ret = geq(pol, attribute_list);
				break;
			}
			case LT:
			{
				ret = lt(pol, attribute_list);
				break;
			}
			case GT:
			{
				ret = gt(pol, attribute_list);
				break;
			}
			default:
			{
				ret = FALSE;
				break;
			}
		}
	}
	else
	{
		int type = json_get_token_index_from_pos(pol->policy_c, atribute_position, "type");
		if((type != -1) && (get_start_of_token(type) < get_end_of_token(atribute_position)))// Check only type within this json object
		{
			int start = get_start_of_token(type);
			int size_of_type = get_size_of_token(type);

			if((size_of_type == strlen("boolean")) && (strncasecmp(pol->policy_c + start, "boolean", strlen("boolean")) == 0))
			{
				int value = json_get_token_index_from_pos(pol->policy_c, atribute_position, "value");
				int start_of_value = get_start_of_token(value);
				int size_of_value = get_size_of_token(value);
				if((size_of_value >= strlen("true")) && (memcmp(pol->policy_c + start_of_value, "true", strlen("true")) == 0))
				{
					ret = TRUE;
				}
				else
				{
					ret = FALSE;
				}
			}
		}
	}

	return ret;
}

static int get_obligation(policy_t *pol, int obl_position, char *obligation)
{
	int ret = PDP_ERROR_RET;
	
	int type = json_get_token_index_from_pos(pol->policy_c, obl_position, "type");
	if((type != -1) && (get_start_of_token(type) < get_end_of_token(obl_position)))// Check only type within this json object
	{
		int start = get_start_of_token(type);
		int size_of_type = get_size_of_token(type);

		if((size_of_type == strlen("obligation")) &&
		(!strncasecmp(pol->policy_c + start, "obligation", strlen("obligation"))))
		{
			int value = json_get_token_index_from_pos(pol->policy_c, obl_position, "value");
			int start_of_value = get_start_of_token(value);
			int size_of_value = get_size_of_token(value);

			if(size_of_value > PDP_OBLIGATION_LEN)
			{
				size_of_value = PDP_OBLIGATION_LEN;
			}
			
			if(value >= 0)
			{
				memcpy(obligation, pol->policy_c + start_of_value, size_of_value);
				ret = value;
			}
		}
	}
	
	return ret;
}

//TODO: obligations should be linked list of the elements of the 'obligation_s' structure type
static int resolve_obligation(policy_t *pol, int obl_position, char *obligation)
{
	int ret = PDP_ERROR_RET;
	int operation = -1;
	int attribute_list = -1;
	int operation_start = -1;
	int operation_end = -1;
	int obl_value = -1;
	PDP_operation_e opt;
	
	if(pol == NULL || obligation == NULL)
	{
		Dlog_printf("\n\nERROR[%s]: Wrong input parameters\n\n",__FUNCTION__);
		return ret;
	}
	
	// Size of obligation buff is 15
	memset(obligation, 0, sizeof(char) * PDP_OBLIGATION_LEN);

	operation = json_get_token_index_from_pos(pol->policy_c, obl_position, "operation");
	attribute_list = json_get_token_index_from_pos(pol->policy_c, obl_position, "attribute_list");
	obl_value = json_get_token_index_from_pos(pol->policy_c, obl_position, "obligations");
	
	// In case of IF operation, multiple obligations are available
	if((attribute_list >= 0) &&
		(get_start_of_token(attribute_list) < get_end_of_token(obl_position)) &&
		(operation >= 0) &&
		(get_start_of_token(operation) < get_end_of_token(obl_position)))
	{
		// Check if operation is listed before or after attribure list
		if(operation > attribute_list)
		{
			// If attribute list is listed first, get operation given after att. list
			
			int number_of_tokens = get_token_num();
			int tok_cnt = attribute_list;
			
			while((get_end_of_token(attribute_list) > get_start_of_token(operation)) && (tok_cnt <= number_of_tokens) && (tok_cnt >= 0))
			{
				operation = json_get_token_index_from_pos(pol->policy_c, tok_cnt, "operation");
				tok_cnt = operation;
			}
		}

		operation_start = get_start_of_token(operation);
		operation_end = get_end_of_token(operation);

		opt = get_operation_new(pol->policy_c + operation_start, operation_end - operation_start);

		//TODO: For now, only IF operation is supported
		switch(opt)
		{
			case IF:
				if(!resolve_attribute(pol, attribute_list))
				{
					// Take second obligation if condition is false (else branch)
					obl_value = json_get_token_index_from_pos(pol->policy_c, obl_value + 1, "obligations");
				}

				break;

			default:
				break;
		}
	}
	
	if(obl_value >= 0)
	{
		ret = get_obligation(pol, obl_value, obligation);
	}
	
	return ret;
}

/****************************************************************************
 * API FUNCTIONS
 ****************************************************************************/
//TODO: obligations should be linked list of the elements of the 'obligation_s' structure type
PDP_decision_e PDP_calculate_decision(char *request_norm, char *obligation, PDP_action_t *action)
{
	int request_policy_id = -1;
	int size = -1;
	PDP_decision_e ret = PDP_ERROR;
	PAP_policy_t policy;
	policy_t pol;

	//Check input parameters
	if(request_norm == NULL || obligation == NULL || action == NULL || action->value == NULL)
	{
		Dlog_printf("\n\nERROR[%s]: Invalid input parameters\n\n",__FUNCTION__);
		return ret;
	}

	//Get policy ID from request
	json_parser_init(request_norm);
	request_policy_id = json_get_value(request_norm, 0, "policy_id");
	size = get_size_of_token(request_policy_id);

	//Get policy from PAP
	if (PAP_get_policy(request_norm + get_start_of_token(request_policy_id), size, &policy) == PAP_ERROR)
	{
		Dlog_printf("\nERROR[%s]: Could not get the policy.\n", __FUNCTION__);
		return PDP_ERROR;
	}

	pol.policy_c = policy.policy_object.policy_object;

	//Get circuits
	json_parser_init(pol.policy_c);

	int policy_goc = json_get_token_index(pol.policy_c, "policy_goc");
	int policy_doc = json_get_token_index(pol.policy_c, "policy_doc");
	int policy_gobl = json_get_token_index(pol.policy_c, "obligation_grant");
	int policy_dobl = json_get_token_index(pol.policy_c, "obligation_deny");

	if(policy_goc == -1)
	{
		Dlog_printf("\nPOLICY policy_goc IS NULL\n");
	}

	if(policy_doc == -1)
	{
		Dlog_printf("\nPOLICY policy_doc IS NULL\n");
	}
	
	if(policy_gobl == -1)
	{
		Dlog_printf("\nOBLIGATION obligation_grant IS NULL\n");
	}

	if(policy_dobl == -1)
	{
		Dlog_printf("\nOBLIGATION obligation_deny IS NULL\n");
	}

	//Resolve attributes
	int pol_goc = resolve_attribute(&pol, policy_goc);
	int pol_doc = resolve_attribute(&pol, policy_doc);

	//Calculate decision
	ret = pol_goc + 2 * pol_doc;  // => (0, 1, 2, 3) <=> (gap, grant, deny, conflict)
	
	if(ret == PDP_GRANT)
	{
		//Get action
		//FIXME: Should action be taken for deny case also?
		int number_of_tokens = get_token_num();
		get_action(action->value, pol.policy_c, number_of_tokens);
		action->start_time = 0;
		action->stop_time = 0;
		get_time_from_attr(&pol, policy_goc, UNDEFINED, &(action->start_time), &(action->stop_time));
		
		if(policy_gobl >= 0)
		{
			resolve_obligation(&pol, policy_gobl, obligation);
		}
	}
	else if(ret == PDP_DENY)
	{
		if(policy_dobl >= 0)
		{
			resolve_obligation(&pol, policy_dobl, obligation);
		}
	}

	Dlog_printf("\nPOLICY GOC RESOLVED: %d", pol_goc);
	Dlog_printf("\nPOLICY DOC RESOLVED: %d", pol_doc);
	Dlog_printf("\nPOLICY RESOLVED: %d\n", ret);

	return ret;
}
