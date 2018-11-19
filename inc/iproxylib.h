#ifndef __IPROXYLIB_H__
#define __IPROXYLIB_H__


int iproxy_set(char *key, char *value);
int iproxy_get(char *key, char *value);
int iproxy_register(char *key, void (*func)(char *));
int iproxy_register_and_get(char *key, char *value);
int iproxy_unregister();
int iproxy_commit();


#endif