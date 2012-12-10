#include "keystore.h"

static int initialized = 0;
static DB *dbp;
static int ret;
static DBT bdb_key, bdb_data;

void keystore_initialize(const char *dbfile, const char *dbname) {
  // printf("Initializing keystore %s:%s ...", dbfile, dbname);
  if(!initialized) {
    
    if ((ret = db_create(&dbp, NULL, 0)) != 0) {
      fprintf(stderr, "db_create: %s\n", db_strerror(ret));
      exit (1);
    }
    if ((ret = dbp->open(dbp, NULL, dbfile, dbname,
			 DB_BTREE, DB_CREATE, 0664)) != 0) {
      dbp->err(dbp, ret, "%s:%s", dbfile, NULL);
      fprintf(stderr, "db_initialize failed: %s\n", db_strerror(ret));
      exit (1);
    }

    initialized = 1;
    // printf("DONE\n");
  }
}

void keystore_destroy() {
  if(initialized) {
    int t_ret = ret;
    if ((t_ret = dbp->close(dbp, 0)) != 0 && ret == 0) {ret = t_ret; }
    initialized = 0;
  }
}

/* returns: 0 on FAILURE 1, on SUCCESS */
int keystore_put(void *k, int ksize, void *v, int vsize) {
  bdb_key.data = k;
  bdb_key.size = ksize;
  bdb_data.data = v;
  bdb_data.size = vsize;

  if ((ret = dbp->put(dbp, NULL, &bdb_key, &bdb_data, 0)) == 0) {
    return 1;
  }
  else {
    dbp->err(dbp, ret, "DB->put");
    return 0;
  }
}

/* returns: 0 on FAILURE 1, on SUCCESS */
int keystore_get(void *k, int ksize, void **p_v, int *p_vsize) {
  if ((ret = dbp->get(dbp, NULL, &bdb_key, &bdb_data, 0)) == 0) {
    *p_v = bdb_data.data;
    *p_vsize = bdb_data.size;
    return 1;
  }
  else {
    dbp->err(dbp, ret, "DB->get");
    return 0;
  }
}

