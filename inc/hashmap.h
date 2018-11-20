/*
 * Generic hashmap manipulation functions
 *
 * Originally by Elliot C Back - http://elliottback.com/wp/hashmap-implementation-in-c/
 *
 * Modified by Pete Warden to fix a serious performance problem, support strings as keys
 * and removed thread synchronization - http://petewarden.typepad.com
 */
#ifndef __HASHMAP_H__
#define __HASHMAP_H__


#define MAP_REPEAT -4 	/* OK */
#define MAP_MISSING -3  /* No such element */
#define MAP_FULL -2 	/* Hashmap is full */
#define MAP_OMEM -1 	/* Out of Memory */
#define MAP_OK 0 	/* OK */

/*
 * any_t is a pointer.  This allows you to put arbitrary structures in
 * the hashmap.
 */
typedef void *any_t;

/*
 * PFany is a pointer to a function that can take two any_t arguments
 * and return an integer. Returns status code..
 */
typedef int (*PFany)(any_t, any_t);

/*
 * map_t is a pointer to an internally maintained data structure.
 * Clients of this package do not need to know how hashmaps are
 * represented.  They see and manipulate only map_t's.
 */
typedef any_t map_t;

/*
 * Return an empty hashmap. Returns NULL if empty.
*/
map_t hashmap_new();

/*
 * Iteratively call f with argument (item, data) for
 * each element data in the hashmap. The function must
 * return a map status code. If it returns anything other
 * than MAP_OK the traversal is terminated. f must
 * not reenter any hashmap functions, or deadlock may arise.
 */
int hashmap_iterate(map_t in, PFany f, any_t item);

/*
 * Add an element to the hashmap. Return MAP_OK or MAP_OMEM.
 */
int hashmap_put(map_t in, char* key, any_t value);

/*
 * Get an element from the hashmap. Return MAP_OK or MAP_MISSING.
 */
int hashmap_get(map_t in, char* key, any_t *arg);

/*
 * Update the value of the hashmap with a key
 */
int hashmap_update_one_value(map_t in, char* key, char* value);

/*
 * Add the fd in the hashmap with a key
 */
int hashmap_add_one_fd(map_t in, char* key, int fd);

/*
 * Remove the fd from the hashmap with a key
 */
int hashmap_remove_one_fd(map_t in, char* key, int fd);

/*
 * Remove the fd from the hashmap with a key
 */
int hashmap_remove_spec_fd(map_t in,int fd);

/*
 * Remove an element from the hashmap. Return MAP_OK or MAP_MISSING.
 */
int hashmap_remove_free(map_t in, char* key);

/*
 * Remove an element from the hashmap. Return MAP_OK or MAP_MISSING.
 */
int hashmap_remove(map_t in, char* key);

/*
 * Free the hashmap
 */
void hashmap_free(map_t in);

/* 
* Deallocate the hashmap and key,data
*/
void hashmap_free_free(map_t in);

/*
* Deallocate the hashmap and key,data,value
*/
void hashmap_free_free_free(map_t in);

/*
 * Get the current size of a hashmap
 */
int hashmap_length(map_t in);

#endif //__HASHMAP_H__
