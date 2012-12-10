#include <cstring>

char* translateFilter(char* filter) {
  char * c;
  int i;
  int tmp = 0;
  while((c = strchr(filter, '.')) != NULL) {
    *c = '/';
    tmp = 1;
  }
  if(tmp == 1) {
    tmp = strlen(filter);
    filter = (char *)realloc(filter, ((++ tmp) * sizeof(char)));
    for(i = tmp; i > 0; i--) {
      filter[i] = filter[i - 1];
    }
    filter[0] = 'L';
  }
  return filter;
}

char* translateFilter1(char* filter) {
  char * c;
  while((c = strchr(filter, '.')) != NULL) {
    *c = '/';
  }
  return filter;
}

char* translateFilter2(char* filter) {
  char * c;
  int i;
  int tmp = 0;
  while((c = strchr(filter, '/')) != NULL) {
    *c = '.';
    tmp = 1;
  }
  if(tmp == 1) {
    tmp = strlen(filter);
    for(i = 0; i < tmp; i++) {
      filter[i] = filter[i + 1];
    }
  }
  return filter;
}
