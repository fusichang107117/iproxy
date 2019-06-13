/*
 * Generic map implementation.
 */
#include "hashmap.h"
#include "iproxy.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define INITIAL_SIZE (256)
#define MAX_CHAIN_LENGTH (8)

/* We need to keep keys and values */
typedef struct _hashmap_element{
	char* key;
	int in_use;
	any_t data;
} hashmap_element;

/* A hashmap has some maximum size and current size,
 * as well as the data to hold. */
typedef struct _hashmap_map{
	int table_size;
	int size;
	hashmap_element *data;
} hashmap_map;

/*
 * Return an empty hashmap, or NULL on failure.
 */
map_t hashmap_new() {
	hashmap_map* m = (hashmap_map*) malloc(sizeof(hashmap_map));
	if(!m) goto err;

	m->data = (hashmap_element*) calloc(INITIAL_SIZE, sizeof(hashmap_element));
	if(!m->data) goto err;

	m->table_size = INITIAL_SIZE;
	m->size = 0;

	return m;
	err:
		if (m)
			hashmap_free(m);
		return NULL;
}

/* The implementation here was originally done by Gary S. Brown.  I have
   borrowed the tables directly, and made some minor changes to the
   crc32-function (including changing the interface). //ylo */

  /* ============================================================= */
  /*  COPYRIGHT (C) 1986 Gary S. Brown.  You may use this program, or       */
  /*  code or tables extracted from it, as desired without restriction.     */
  /*                                                                        */
  /*  First, the polynomial itself and its table of feedback terms.  The    */
  /*  polynomial is                                                         */
  /*  X^32+X^26+X^23+X^22+X^16+X^12+X^11+X^10+X^8+X^7+X^5+X^4+X^2+X^1+X^0   */
  /*                                                                        */
  /*  Note that we take it "backwards" and put the highest-order term in    */
  /*  the lowest-order bit.  The X^32 term is "implied"; the LSB is the     */
  /*  X^31 term, etc.  The X^0 term (usually shown as "+1") results in      */
  /*  the MSB being 1.                                                      */
  /*                                                                        */
  /*  Note that the usual hardware shift register implementation, which     */
  /*  is what we're using (we're merely optimizing it by doing eight-bit    */
  /*  chunks at a time) shifts bits into the lowest-order term.  In our     */
  /*  implementation, that means shifting towards the right.  Why do we     */
  /*  do it this way?  Because the calculated CRC must be transmitted in    */
  /*  order from highest-order term to lowest-order term.  UARTs transmit   */
  /*  characters in order from LSB to MSB.  By storing the CRC this way,    */
  /*  we hand it to the UART in the order low-byte to high-byte; the UART   */
  /*  sends each low-bit to hight-bit; and the result is transmission bit   */
  /*  by bit from highest- to lowest-order term without requiring any bit   */
  /*  shuffling on our part.  Reception works similarly.                    */
  /*                                                                        */
  /*  The feedback terms table consists of 256, 32-bit entries.  Notes:     */
  /*                                                                        */
  /*      The table can be generated at runtime if desired; code to do so   */
  /*      is shown later.  It might not be obvious, but the feedback        */
  /*      terms simply represent the results of eight shift/xor opera-      */
  /*      tions for all combinations of data and CRC register values.       */
  /*                                                                        */
  /*      The values must be right-shifted by eight bits by the "updcrc"    */
  /*      logic; the shift must be unsigned (bring in zeroes).  On some     */
  /*      hardware you could probably optimize the shift in assembler by    */
  /*      using byte-swap instructions.                                     */
  /*      polynomial $edb88320                                              */
  /*                                                                        */
  /*  --------------------------------------------------------------------  */

static unsigned long crc32_tab[] = {
	  0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL, 0x076dc419L,
	  0x706af48fL, 0xe963a535L, 0x9e6495a3L, 0x0edb8832L, 0x79dcb8a4L,
	  0xe0d5e91eL, 0x97d2d988L, 0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L,
	  0x90bf1d91L, 0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
	  0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L, 0x136c9856L,
	  0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL, 0x14015c4fL, 0x63066cd9L,
	  0xfa0f3d63L, 0x8d080df5L, 0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L,
	  0xa2677172L, 0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
	  0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L, 0x32d86ce3L,
	  0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L, 0x26d930acL, 0x51de003aL,
	  0xc8d75180L, 0xbfd06116L, 0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L,
	  0xb8bda50fL, 0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
	  0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL, 0x76dc4190L,
	  0x01db7106L, 0x98d220bcL, 0xefd5102aL, 0x71b18589L, 0x06b6b51fL,
	  0x9fbfe4a5L, 0xe8b8d433L, 0x7807c9a2L, 0x0f00f934L, 0x9609a88eL,
	  0xe10e9818L, 0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
	  0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL, 0x6c0695edL,
	  0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L, 0x65b0d9c6L, 0x12b7e950L,
	  0x8bbeb8eaL, 0xfcb9887cL, 0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L,
	  0xfbd44c65L, 0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
	  0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL, 0x4369e96aL,
	  0x346ed9fcL, 0xad678846L, 0xda60b8d0L, 0x44042d73L, 0x33031de5L,
	  0xaa0a4c5fL, 0xdd0d7cc9L, 0x5005713cL, 0x270241aaL, 0xbe0b1010L,
	  0xc90c2086L, 0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
	  0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L, 0x59b33d17L,
	  0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL, 0xedb88320L, 0x9abfb3b6L,
	  0x03b6e20cL, 0x74b1d29aL, 0xead54739L, 0x9dd277afL, 0x04db2615L,
	  0x73dc1683L, 0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
	  0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L, 0xf00f9344L,
	  0x8708a3d2L, 0x1e01f268L, 0x6906c2feL, 0xf762575dL, 0x806567cbL,
	  0x196c3671L, 0x6e6b06e7L, 0xfed41b76L, 0x89d32be0L, 0x10da7a5aL,
	  0x67dd4accL, 0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
	  0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L, 0xd1bb67f1L,
	  0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL, 0xd80d2bdaL, 0xaf0a1b4cL,
	  0x36034af6L, 0x41047a60L, 0xdf60efc3L, 0xa867df55L, 0x316e8eefL,
	  0x4669be79L, 0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
	  0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL, 0xc5ba3bbeL,
	  0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L, 0xc2d7ffa7L, 0xb5d0cf31L,
	  0x2cd99e8bL, 0x5bdeae1dL, 0x9b64c2b0L, 0xec63f226L, 0x756aa39cL,
	  0x026d930aL, 0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
	  0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L, 0x92d28e9bL,
	  0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L, 0x86d3d2d4L, 0xf1d4e242L,
	  0x68ddb3f8L, 0x1fda836eL, 0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L,
	  0x18b74777L, 0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
	  0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L, 0xa00ae278L,
	  0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L, 0xa7672661L, 0xd06016f7L,
	  0x4969474dL, 0x3e6e77dbL, 0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L,
	  0x37d83bf0L, 0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
	  0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L, 0xbad03605L,
	  0xcdd70693L, 0x54de5729L, 0x23d967bfL, 0xb3667a2eL, 0xc4614ab8L,
	  0x5d681b02L, 0x2a6f2b94L, 0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL,
	  0x2d02ef8dL
};

static const unsigned char crc8_table[256] = { 
	0x00, 0xF7, 0xB9, 0x4E, 0x25, 0xD2, 0x9C, 0x6B,
	0x4A, 0xBD, 0xF3, 0x04, 0x6F, 0x98, 0xD6, 0x21,
	0x94, 0x63, 0x2D, 0xDA, 0xB1, 0x46, 0x08, 0xFF,
	0xDE, 0x29, 0x67, 0x90, 0xFB, 0x0C, 0x42, 0xB5,
	0x7F, 0x88, 0xC6, 0x31, 0x5A, 0xAD, 0xE3, 0x14,
	0x35, 0xC2, 0x8C, 0x7B, 0x10, 0xE7, 0xA9, 0x5E,
	0xEB, 0x1C, 0x52, 0xA5, 0xCE, 0x39, 0x77, 0x80,
	0xA1, 0x56, 0x18, 0xEF, 0x84, 0x73, 0x3D, 0xCA,
	0xFE, 0x09, 0x47, 0xB0, 0xDB, 0x2C, 0x62, 0x95,
	0xB4, 0x43, 0x0D, 0xFA, 0x91, 0x66, 0x28, 0xDF,
	0x6A, 0x9D, 0xD3, 0x24, 0x4F, 0xB8, 0xF6, 0x01,
	0x20, 0xD7, 0x99, 0x6E, 0x05, 0xF2, 0xBC, 0x4B,
	0x81, 0x76, 0x38, 0xCF, 0xA4, 0x53, 0x1D, 0xEA,
	0xCB, 0x3C, 0x72, 0x85, 0xEE, 0x19, 0x57, 0xA0,
	0x15, 0xE2, 0xAC, 0x5B, 0x30, 0xC7, 0x89, 0x7E,
	0x5F, 0xA8, 0xE6, 0x11, 0x7A, 0x8D, 0xC3, 0x34,
	0xAB, 0x5C, 0x12, 0xE5, 0x8E, 0x79, 0x37, 0xC0,
	0xE1, 0x16, 0x58, 0xAF, 0xC4, 0x33, 0x7D, 0x8A,
	0x3F, 0xC8, 0x86, 0x71, 0x1A, 0xED, 0xA3, 0x54,
	0x75, 0x82, 0xCC, 0x3B, 0x50, 0xA7, 0xE9, 0x1E,
	0xD4, 0x23, 0x6D, 0x9A, 0xF1, 0x06, 0x48, 0xBF,
	0x9E, 0x69, 0x27, 0xD0, 0xBB, 0x4C, 0x02, 0xF5,
	0x40, 0xB7, 0xF9, 0x0E, 0x65, 0x92, 0xDC, 0x2B,
	0x0A, 0xFD, 0xB3, 0x44, 0x2F, 0xD8, 0x96, 0x61,
	0x55, 0xA2, 0xEC, 0x1B, 0x70, 0x87, 0xC9, 0x3E,
	0x1F, 0xE8, 0xA6, 0x51, 0x3A, 0xCD, 0x83, 0x74,
	0xC1, 0x36, 0x78, 0x8F, 0xE4, 0x13, 0x5D, 0xAA,
	0x8B, 0x7C, 0x32, 0xC5, 0xAE, 0x59, 0x17, 0xE0,
	0x2A, 0xDD, 0x93, 0x64, 0x0F, 0xF8, 0xB6, 0x41,
	0x60, 0x97, 0xD9, 0x2E, 0x45, 0xB2, 0xFC, 0x0B,
	0xBE, 0x49, 0x07, 0xF0, 0x9B, 0x6C, 0x22, 0xD5,
	0xF4, 0x03, 0x4D, 0xBA, 0xD1, 0x26, 0x68, 0x9F
};

/* Return a 32-bit CRC of the contents of the buffer. */
unsigned long crc32(const unsigned char *s, unsigned int len)
{
	unsigned int i;
	unsigned long crc32val;
  
	crc32val = 0;
	for (i = 0;  i < len;  i ++)
	{
		crc32val =
		crc32_tab[(crc32val ^ s[i]) & 0xff] ^(crc32val >> 8);
	}
	return crc32val;
}

/* Return a 32-bit CRC of the contents of the buffer. */
unsigned long crc32_start(const unsigned char *s, unsigned int len, unsigned long last)
{
	unsigned int i;
	unsigned long crc32val = last;

	//crc32val = 0;
	for (i = 0;  i < len;  i ++)
	{
		crc32val =
		crc32_tab[(crc32val ^ s[i]) & 0xff] ^(crc32val >> 8);
	}
	return crc32val;
}

/* Return a 8-bit CRC of the contents of the buffer. */
unsigned char crc8(const unsigned char *s, unsigned int len)
{
	unsigned int i;
	unsigned char crc8val;
  
	crc8val = 0;
	for (i = 0;  i < len;  i ++)
	{
		crc8val =
		crc8_table[(crc8val ^ s[i]) & 0xff] ^(crc8val >> 8);
	}
	return crc8val;
}

/* Return a 32-bit CRC of the contents of the buffer. */
unsigned char crc8_start(const unsigned char *s, unsigned int len, unsigned long last)
{
	unsigned int i;
	unsigned char crc8val = last;

	//crc8val = 0;
	for (i = 0;  i < len;  i ++)
	{
		crc8val =
		crc8_table[(crc8val ^ s[i]) & 0xff] ^(crc8val >> 8);
	}
	return crc8val;
}

/*
 * Hashing function for a string
 */
unsigned int hashmap_hash_int(hashmap_map * m, char* keystring){

	unsigned hash = 5381;
	const signed char *c;

	/* DJB's hash function */

	for (c = keystring; *c; c++)
		hash = (hash << 5) + hash + (unsigned) *c;
	return hash % m->table_size;;
}

/*
 * Return the integer of the location in data
 * to store the point to the item, or MAP_FULL.
 */
int hashmap_hash(map_t in, char* key){
	int curr;
	int i;

	/* Cast the hashmap */
	hashmap_map* m = (hashmap_map *) in;

	/* If full, return immediately */
	if(m->size >= (m->table_size/2)) return MAP_FULL;

	/* Find the best index */
	curr = hashmap_hash_int(m, key);

	/* Linear probing */
	for(i = 0; i< MAX_CHAIN_LENGTH; i++){
		if(m->data[curr].in_use == 0)
			return curr;

		if(m->data[curr].in_use == 1 && (strcmp(m->data[curr].key,key)==0))
			return curr;

		curr = (curr + 1) % m->table_size;
	}

	return MAP_FULL;
}

/*
 * Doubles the size of the hashmap, and rehashes all the elements
 */
int hashmap_rehash(map_t in){
	int i;
	int old_size;
	hashmap_element* curr;

	/* Setup the new elements */
	hashmap_map *m = (hashmap_map *) in;
	hashmap_element* temp = (hashmap_element *)
		calloc(2 * m->table_size, sizeof(hashmap_element));
	if(!temp) return MAP_OMEM;

	/* Update the array */
	curr = m->data;
	m->data = temp;

	/* Update the size */
	old_size = m->table_size;
	m->table_size = 2 * m->table_size;
	m->size = 0;

	/* Rehash the elements */
	for(i = 0; i < old_size; i++){
		int status;

		if (curr[i].in_use == 0)
			continue;
			
		status = hashmap_put(m, curr[i].key, curr[i].data);
		if (status != MAP_OK)
			return status;
	}

	free(curr);

	return MAP_OK;
}

/*
 * Add a pointer to the hashmap with some key
 */
int hashmap_put(map_t in, char* key, any_t value){
	int index;
	hashmap_map* m;

	/* Cast the hashmap */
	m = (hashmap_map *) in;

	/* Find a place to put our value */
	index = hashmap_hash(in, key);
	while(index == MAP_FULL){
		if (hashmap_rehash(in) == MAP_OMEM) {
			return MAP_OMEM;
		}
		index = hashmap_hash(in, key);
	}

	/* Set the data */
	m->data[index].data = value;
	m->data[index].key = key;
	m->data[index].in_use = 1;
	m->size++; 

	return MAP_OK;
}

/*
 * Get your pointer out of the hashmap with a key
 */
int hashmap_get(map_t in, char* key, any_t *arg){
	int curr;
	int i;
	hashmap_map* m;

	/* Cast the hashmap */
	m = (hashmap_map *) in;

	/* Find data location */
	curr = hashmap_hash_int(m, key);

	/* Linear probing, if necessary */
	for(i = 0; i<MAX_CHAIN_LENGTH; i++){

		int in_use = m->data[curr].in_use;
		if (in_use == 1){
			if (strcmp(m->data[curr].key,key)==0){
				*arg = (m->data[curr].data);
				return MAP_OK;
			}
		}

		curr = (curr + 1) % m->table_size;
	}

	*arg = NULL;

	/* Not found */
	return MAP_MISSING;
}

/*
 * Update the value of the hashmap with a key
 */
int hashmap_update_one_value(map_t in, char* key, char* value){
	int curr;
	int i;
	hashmap_map* m;

	/* Cast the hashmap */
	m = (hashmap_map *) in;

	/* Find data location */
	curr = hashmap_hash_int(m, key);

	/* Linear probing, if necessary */
	for(i = 0; i<MAX_CHAIN_LENGTH; i++){

		int in_use = m->data[curr].in_use;
		if (in_use == 1){
			if (strcmp(m->data[curr].key,key)==0){
				iproxy_data_node_t *data = m->data[curr].data;

				//printf("%s(), %d, %s, %s\n", __func__, __LINE__, data->value, value);
				free(data->value);
				data->value = value;
				return MAP_OK;
			}
		}

		curr = (curr + 1) % m->table_size;
	}

	/* Not found */
	return MAP_MISSING;
}

/*
 * Add the fd in the hashmap with a key
 */
int hashmap_add_one_fd(map_t in, char* key, int fd){
	int curr;
	int i,j;
	hashmap_map* m;

	/* Cast the hashmap */
	m = (hashmap_map *) in;

	/* Find data location */
	curr = hashmap_hash_int(m, key);

	/* Linear probing, if necessary */
	for(i = 0; i<MAX_CHAIN_LENGTH; i++){

		int in_use = m->data[curr].in_use;
		if (in_use == 1){
			if (strcmp(m->data[curr].key,key)==0){
				iproxy_data_node_t *data = m->data[curr].data;
				for (j = 1; j <= data->fd[0]; j++) {
					if (fd == data->fd[j]) {
						return MAP_REPEAT;
					}
				}
				if (data->fd[0] >= MAX_REGISTER_FD)
					return MAP_FULL;
				data->fd[0]++;
				data->fd[data->fd[0]] = fd;
				return MAP_OK;
			}
		}

		curr = (curr + 1) % m->table_size;
	}
	/* Not found */
	return MAP_MISSING;
}

/*
 * Remove the fd from the hashmap with a key
 */
int hashmap_remove_one_fd(map_t in, char* key, int fd){
	int curr;
	int i,j,k;
	hashmap_map* m;

	/* Cast the hashmap */
	m = (hashmap_map *) in;

	/* Find data location */
	curr = hashmap_hash_int(m, key);

	/* Linear probing, if necessary */
	for(i = 0; i<MAX_CHAIN_LENGTH; i++){

		int in_use = m->data[curr].in_use;
		if (in_use == 1){
			if (strcmp(m->data[curr].key,key)==0){
				iproxy_data_node_t *data = m->data[curr].data;
				for (j = 1; j <= data->fd[0]; j++) {
					if (data->fd[j] == fd) {
						for (k = j; k < data->fd[0]; k++)
							data->fd[k] = data->fd[k + 1];
						data->fd[k] = -1;
						data->fd[0]--;
						return MAP_OK;
					}
				}
				return MAP_MISSING;
			}
		}

		curr = (curr + 1) % m->table_size;
	}
	/* Not found */
	return MAP_MISSING;
}

/*
 * Remove the spec fd from the hashmap with all key
 */
int hashmap_remove_spec_fd(map_t in,int fd){
	int curr;
	int i,j,k;
	hashmap_map* m;

	/* Cast the hashmap */
	m = (hashmap_map *) in;

	/* On empty hashmap, return immediately */
	if (hashmap_length(m) <= 0)
		return MAP_MISSING;

	/* Linear probing, if necessary */
	for(i = 0; i<m->table_size; i++){
		int in_use = m->data[i].in_use;
		if (in_use == 1){
			iproxy_data_node_t *data = m->data[i].data;
			for (j = 1; j <= data->fd[0]; j++) {
				if (data->fd[j] == fd) {
					for (k = j; k < data->fd[0]; k++)
						data->fd[k] = data->fd[k + 1];
					data->fd[k] = -1;
					data->fd[0]--;
					return MAP_OK;
				}
			}
		}
	}
	/* Not found */
	return MAP_MISSING;
}

/*
 * get element from index in the hashmap
 */
int hashmap_get_from_index(map_t in, int  index, char *buf, int buf_len, int *real_len) {
	int i;
	int offset = 0;

	/* Cast the hashmap */
	hashmap_map* m = (hashmap_map*) in;

	*real_len = 0;
	if (index >= m->table_size || m->size <= 0) {
		return MAP_END;
	}

	/* Linear probing */
	for(i = index; i< m->table_size; i++) {
		if(m->data[i].in_use != 0) {
			iproxy_data_node_t *data = m->data[i].data;
			char *key = m->data[i].key;
			int key_len = strlen(key) + 1;
			int value_len = strlen(data->value) + 1;

			//printf("i: %d, offset: %d,  key_len: %d, value_len: %d\n", i, offset, key_len, value_len);
			if (offset + key_len + value_len > buf_len) {
				if (i == index) {
					//log_err("buf is not enough %d(%d)\n", buf_len, key_len + value_len);
					return MAP_END;
				}
				return i;
			}
			memcpy(buf + offset, key, key_len);
			offset += key_len;
			memcpy(buf + offset, data->value, value_len);
			offset += value_len;
			*real_len = offset;
			//printf("<%s>\t\t\t<%s>\n", key, data->value);
		}
	}

	return MAP_END;
}

/*
 * get key value from spec index in the hashmap
 */
int hashmap_get_key_value_index(map_t in, int  i, char **key, void **data) {

	int ret;
	/* Cast the hashmap */
	hashmap_map* m = (hashmap_map*) in;

	if( i >= m->table_size || m->size <= 0 )
		ret = MAP_END;
	else if(m->data[i].in_use != 0) {
		ret = MAP_OK;
		*key = m->data[i].key;
		*data = (void *)m->data[i].data;
		//log_debug("%s(), %d, key: %s,\n", __func__, __LINE__,  *key);
	} else {
		ret = MAP_MISSING;
	}
	return ret;
}

/*
 * get element from index in the hashmap
 */
int hashmap_get_list_all(map_t in) {
	int i;

	/* Cast the hashmap */
	hashmap_map* m = (hashmap_map*) in;

	if (m->size <= 0) {
		return MAP_END;
	}

	/* Linear probing */
	for(i = 0; i< m->table_size; i++) {
		if(m->data[i].in_use != 0) {
			//printf("%s(), %d, i: %d\n", __func__, __LINE__, i);
			iproxy_data_node_t *data = m->data[i].data;
			char *key = m->data[i].key;
			fprintf(stderr, "%-32s:\t%s\n", key, data->value);
		}
	}
	return MAP_END;
}


/*
 * Iterate the function parameter over each element in the hashmap.  The
 * additional any_t argument is passed to the function as its first
 * argument and the hashmap element is the second.
 */
int hashmap_iterate(map_t in, PFany f, any_t item) {
	int i;

	/* Cast the hashmap */
	hashmap_map* m = (hashmap_map*) in;

	/* On empty hashmap, return immediately */
	if (hashmap_length(m) <= 0)
		return MAP_MISSING;	

	/* Linear probing */
	for(i = 0; i< m->table_size; i++)
		if(m->data[i].in_use != 0) {
			any_t data = (any_t) (m->data[i].data);
			int status = f(item, data);
			if (status != MAP_OK) {
				return status;
			}
		}

	return MAP_OK;
}

/*
 * Remove an element with that key from the map
 */
int hashmap_remove_free(map_t in, char* key){
	int i;
	int curr;
	hashmap_map* m;

	/* Cast the hashmap */
	m = (hashmap_map *) in;

	/* Find key */
	curr = hashmap_hash_int(m, key);

	/* Linear probing, if necessary */
	for(i = 0; i<MAX_CHAIN_LENGTH; i++){

		int in_use = m->data[curr].in_use;
		if (in_use == 1){
			if (strcmp(m->data[curr].key,key)==0){
				/* Blank out the fields */
				iproxy_data_node_t *data = m->data[curr].data;
				free(data->value);
				free(data);
				free(m->data[curr].key);

				m->data[curr].in_use = 0;
				m->data[curr].key = NULL;

				/* Reduce the size */
				m->size--;
				return MAP_OK;
			}
		}
		curr = (curr + 1) % m->table_size;
	}

	/* Data not found */
	return MAP_MISSING;
}

/*
 * Remove an element with that key from the map
 */
int hashmap_remove(map_t in, char* key){
	int i;
	int curr;
	hashmap_map* m;

	/* Cast the hashmap */
	m = (hashmap_map *) in;

	/* Find key */
	curr = hashmap_hash_int(m, key);

	/* Linear probing, if necessary */
	for(i = 0; i<MAX_CHAIN_LENGTH; i++){

		int in_use = m->data[curr].in_use;
		if (in_use == 1){
			if (strcmp(m->data[curr].key,key)==0){
				/* Blank out the fields */
				iproxy_data_node_t *data = m->data[curr].data;
				free(data);
				free(m->data[curr].key);

				m->data[curr].in_use = 0;
				m->data[curr].key = NULL;

				/* Reduce the size */
				m->size--;
				return MAP_OK;
			}
		}
		curr = (curr + 1) % m->table_size;
	}

	/* Data not found */
	return MAP_MISSING;
}

/* clear hashmap Deallocate all key,data,value*/
void hashmap_clear(map_t in){
	int i;
	hashmap_map* m = (hashmap_map*) in;

	/* Linear probing */
	for(i = 0; i< m->table_size; i++) {
		if(m->data[i].in_use != 0) {
				iproxy_data_node_t *data = m->data[i].data;
				free(data);
				free(m->data[i].key);
				m->data[i].in_use = 0;
				m->data[i].key = NULL;
		}
	}
	m->size = 0;
}

/* Deallocate the hashmap */
void hashmap_free(map_t in){
	hashmap_map* m = (hashmap_map*) in;
	free(m->data);
	free(m);
}

/* Deallocate the hashmap and key,data*/
void hashmap_free_free(map_t in){
	int i;
	hashmap_map* m = (hashmap_map*) in;

	/* Linear probing */
	for(i = 0; i< m->table_size; i++) {
		if(m->data[i].in_use != 0) {
			iproxy_data_node_t *data = m->data[i].data;
			free(m->data[i].key);
			free(data);
		}
	}

	free(m->data);
	free(m);
}

/* Deallocate the hashmap and key,data,value*/
void hashmap_free_free_free(map_t in){
	int i;
	hashmap_map* m = (hashmap_map*) in;

	/* Linear probing */
	for(i = 0; i< m->table_size; i++) {
		if(m->data[i].in_use != 0) {
			iproxy_data_node_t *data = m->data[i].data;
			free(m->data[i].key);
			free(data->value);
			free(data);
		}
	}

	free(m->data);
	free(m);
}

/* Return the length of the hashmap */
int hashmap_length(map_t in){
	hashmap_map* m = (hashmap_map *) in;
	if(m != NULL) return m->size;
	else return 0;
}