#ifndef KEYSTORE_H
#define KEYSTORE_H

#include <db.h>
#include <cstdlib>

void keystore_initialize(const char *dbfile, const char *dbname);
void keystore_destroy();
int  keystore_put(void *k, int ksize, void *v, int vsize);
int  keystore_get(void *k, int ksize, void **p_v, int *p_vsize);

#endif
