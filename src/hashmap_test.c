/*
 * A unit test and example of how to use the simple C hashmap
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "hashmap.h"
#include "iproxy.h"

#define KEY_MAX_LENGTH (256)
#define VALUE_MAX_LENGTH (256)
#define KEY_PREFIX ("somekey")
#define VALUE_PREFIX ("somevalue")

#define KEY_COUNT (9)

typedef struct data_struct_s
{
	char *value;
	int fd[MAX_REGISTER_FD + 1];
} data_struct_t;


int test_func(void)
{
	int error, index = 10, i = 0, j = 0;
	map_t mymap;

	mymap = hashmap_new();

	data_node_t* data;
	data = malloc(sizeof(data_node_t));
	char *key = malloc(128);
	data->value = malloc(128);
	data->fd[0] = 0;

	snprintf(key, 128, "%s%d", "hello", index);
	snprintf(data->value, 128, "%s%d", "world", index);

	printf("PUT: key_string: %s, value: %s\n", key, data->value);
	error = hashmap_put(mymap, key, data);
	assert(error==MAP_OK);


	data_node_t* tmp;
	error = hashmap_get(mymap, key, (void**)(&tmp));
	assert(error==MAP_OK);

	printf("GET: key_string: %s, value: %s\n", key, tmp->value );

	for (i = 0; i < 10; i++) {
		char *value = malloc(128);
		snprintf(value, 128, "%s%d", "world", i);
		printf("UPDATE: value: %s\n", value );
		hashmap_update_one_value(mymap, key, value);
		printf("GET: value: %s\n", value);
	}

	for (i = 0; i < MAX_REGISTER_FD + 10; i++) {
		error = hashmap_add_one_fd(mymap, "hello10", i + 10);
		printf("Add fd: %d, error: %d\n", i + 10, error);
		error = hashmap_get(mymap, key, (void**)(&tmp));
		if(error==MAP_OK) {
			//printf("GET: key_string: %s, value: %s\n", tmp->key_string, tmp->value);
			printf("GET:fd num: %d\n",  tmp->fd[0]);
			for (j = 1; j <= tmp->fd[0]; j++) {
				printf("%d\t", tmp->fd[j]);
			}
			printf("\n");
		}
	}

	error = hashmap_remove_one_fd(mymap, "hello10", 19);
	printf("Remove fd: %d, error: %d\n", i + 10, error);

	for (i = 0; i < MAX_REGISTER_FD + 10; i++) {
		error = hashmap_get(mymap, key, (void**)(&tmp));
		if(error==MAP_OK) {
			//printf("GET: key_string: %s, value: %s\n", tmp->key_string, tmp->value);
			printf("GET:fd num: %d\n",  tmp->fd[0]);
			for (j = 1; j <= tmp->fd[0]; j++) {
				printf("%d\t", tmp->fd[j]);
			}
			printf("\n");
		}
	}

	hashmap_free_free_free(mymap);

}

int main(int argc, char const *argv[])
{
	int index;
	int error;
	map_t mymap;
	char key_string[KEY_MAX_LENGTH];
	data_struct_t* value;

	test_func();
#if 0
	if (argc != 3) {
		printf("use like this: ./run key value\n");
		return -1;
	}
	printf("key_prefix: %s, value_prefix: %s\n", argv[1], argv[2]);

	mymap = hashmap_new();

	int key_len = strlen(argv[1]) + 6;
	int value_len = strlen(argv[2]) + 6;

	/* First, populate the hash map with ascending values */
	for (index=0; index<KEY_COUNT; index+=1)
	{
		/* Store the key string along side the numerical value so we can free it later */
		value = malloc(sizeof(data_struct_t));
		value->key_string = malloc(key_len);
		value->value = malloc(value_len);

		snprintf(value->key_string, key_len, "%s%d", argv[1], index);
		snprintf(value->value, value_len, "%s%d", argv[2], index);
		//value->number = index;

		printf("PUT: key_string: %s, value: %s\n", value->key_string, value->value );
		error = hashmap_put(mymap, value->key_string, value);
		assert(error==MAP_OK);
	}

	/* Now, check all of the expected values are there */
	for (index=0; index<KEY_COUNT; index+=1)
	{
		snprintf(key_string, key_len, "%s%d", argv[1], index);

		error = hashmap_get(mymap, key_string, (void**)(&value));

		printf("GET: key_string: %s, value: %s\n", key_string, value->value );
		/* Make sure the value was both found and the correct number */
		assert(error==MAP_OK);
		//assert(value->number==index);
	}

	/* Make sure that a value that wasn't in the map can't be found */
	snprintf(key_string, KEY_MAX_LENGTH, "%s%d", KEY_PREFIX, KEY_COUNT);

	error = hashmap_get(mymap, key_string, (void**)(&value));

	/* Make sure the value was not found */
	assert(error==MAP_MISSING);

	/* Free all of the values we allocated and remove them from the map */
	for (index=0; index<KEY_COUNT; index+=1)
	{
		snprintf(key_string, KEY_MAX_LENGTH, "%s%d", KEY_PREFIX, index);

		error = hashmap_get(mymap, key_string, (void**)(&value));
		assert(error==MAP_OK);

		error = hashmap_remove(mymap, key_string);
		assert(error==MAP_OK);

		free(value);
	}

	/* Now, destroy the map */
	hashmap_free(mymap);
#endif
	return 1;
}