#ifndef __IPROXYLIB_H__
#define __IPROXYLIB_H__

int iproxyd_connect(void);
int iproxy_set(char *key, char *value);
int iproxy_get(char *key, char *value);
int iproxy_sub(char *key, void (*func)(char *));
int iproxy_sub_and_get(char *key, char *value);
int iproxy_unsub();
int iproxy_unset(char *key);
int iproxy_commit();
int iproxy_list(void);


#endif