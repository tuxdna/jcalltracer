#include <db.h>

static int initialized = 0;
static DB *dbp;
static int ret;
static DBT bdb_key, bdb_data;

void bdb_initialize() {
  if(!initialized) {

    if ((ret = db_create(&dbp, NULL, 0)) != 0) {
      fprintf(stderr, "db_create: %s\n", db_strerror(ret));
      exit (1);
    }
    if ((ret = dbp->open(dbp, NULL, DATABASE, NULL, DB_BTREE, DB_CREATE, 0664)) != 0) {
      dbp->err(dbp, ret, "%s", DATABASE);
      fprintf(stderr, "db_initialize failed: %s\n", db_strerror(ret));
      exit (1);
    }

    initialized = 1;
  }
}

/* returns 
   0 on FAILURE
   1 on SUCCESS */
int put_key_value(void *k, int ksize, void *v, int vsize) {
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

/* returns 
   0 on FAILURE
   1 on SUCCESS */
void get_key_value(void *k, int ksize, void **p_v, int *p_vsize) {

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

void bdb_finalize() {
  if(initialized) {
    db_close(&dbp, 0);
    initialized = 0;
  }
}
