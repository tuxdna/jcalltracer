#include "jcalltracer.h"
#include "keystore.h"
#include "utils.h"

#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <map>
#include <stack>
#include <iostream>

static jvmtiEnv *g_jvmti_env;

LOCK_OBJECT classAccess = 0;
LOCK_OBJECT callTraceAccess = 0;
LOCK_OBJECT assignThreadAccess = 0;

char **incFilters;
char **excFilters;
int incFilterLen = 0;
int excFilterLen = 0;

const char *traceFile = "call.trace";

FILE *logfile = NULL;
FILE *metafile = NULL;

std::map<char*, char *> options_map;

jrawMonitorID monitor_lock;

void setupFiltersFromString(char *value) {
  char *ptrfilter = strtok(value, "|");
  char **tmp;
  while (ptrfilter != NULL) {
    if(ptrfilter[0] == '!') {
      ptrfilter = &ptrfilter[1];
      tmp = (char **)realloc(excFilters, ((excFilterLen += 3) * sizeof(char *)));
      if(tmp != NULL) {
	excFilters = tmp;
	excFilters[excFilterLen - 3] = translateFilter(strdup(ptrfilter));
	excFilters[excFilterLen - 2] = translateFilter1(strdup(ptrfilter));
	excFilters[excFilterLen - 1] = translateFilter2(strdup(ptrfilter));
      }
    } else {
      tmp = (char **)realloc(incFilters, ((incFilterLen += 3) * sizeof(char *)));
      if(tmp != NULL) {
	incFilters = tmp;
	incFilters[incFilterLen - 3] = translateFilter(strdup(ptrfilter));
	incFilters[incFilterLen - 2] = translateFilter1(strdup(ptrfilter));
	incFilters[incFilterLen - 1] = translateFilter2(strdup(ptrfilter));
      }
    }
    ptrfilter = strtok(NULL, "|");
  }
}

void setupFiltersFromFile(char *fname) {
  char line[128];
  char *ptrfilter;
  char **tmp;
  FILE * filterFile = fopen(fname, "r");
  if(filterFile != NULL) {
    while(fgets(line, 128, filterFile) != NULL) {
      if(line[strlen(line) - 1] == '\n' || line[strlen(line) - 1] == '\r') {
	if(line[strlen(line) - 1] == '\r' && line[strlen(line) - 2] == '\n') {
	  line[strlen(line) - 2] = '\0';
	} else if(line[strlen(line) - 1] == '\n' 
		  && line[strlen(line) - 2] == '\r') {
	  line[strlen(line) - 2] = '\0';
	} else {
	  line[strlen(line) - 1] = '\0';
	}
      }
      if(line[0] == '\0') {
	continue;
      }
      if(line[0] == '!') {
	ptrfilter = &line[1];
	tmp = (char **)realloc(excFilters, ((excFilterLen += 3) * sizeof(char *)));
	if(tmp != NULL) {
	  excFilters = tmp;
	  excFilters[excFilterLen - 3] = translateFilter(strdup(ptrfilter));
	  excFilters[excFilterLen - 2] = translateFilter1(strdup(ptrfilter));
	  excFilters[excFilterLen - 1] = translateFilter2(strdup(ptrfilter));
	}
      } else {
	ptrfilter = &line[0];
	tmp = (char **)realloc(incFilters, ((incFilterLen += 3) * sizeof(char *)));
	if(tmp != NULL) {
	  incFilters = tmp;
	  incFilters[incFilterLen - 3] = translateFilter(strdup(ptrfilter));
	  incFilters[incFilterLen - 2] = translateFilter1(strdup(ptrfilter));
	  incFilters[incFilterLen - 1] = translateFilter2(strdup(ptrfilter));
	}
      }
    }
    fclose(filterFile);
  }
}

void clearFilter(char *filter) {
  free(filter);
}

void clearAllFilters() {
  int i = 0;
  for(i = 0; i < incFilterLen; i++) {
    clearFilter(incFilters[i]);
  }

  free(incFilters);

  for(i = 0; i < excFilterLen; i++) {
    clearFilter(excFilters[i]);
  }
  free(excFilters);
}


/*
  setup:
   thread_id container
   stacks container
   thread_handle_id_map
   thread_handle_stack_map
 */

KeyType nextKey() {
  static KeyType ck = -1;
  return ++ck;
}
/*
  {
    jthread => [threadId, stack<KeyDegreePair>*]
  }
 */

class KeyDegreePair{
public:
  KeyType key;
  int degree;
  KeyDegreePair(KeyType k, int d) {
    this->key = k;
    this->degree = d;
  }
  KeyDegreePair(KeyType k) {
    this->key = k;
    this->degree = 0;
  }
  KeyDegreePair() {
    this->key = -1;
    this->degree = 0;
  }
};

typedef std::stack<KeyDegreePair *> KeyDegreeStack;

class KeyStackPair {
public:
  KeyType key;
  KeyDegreeStack *stack;

  KeyStackPair(KeyType k) {
    this->key = k;
    this->stack = new KeyDegreeStack;
  }

  KeyStackPair() {
    this->key = -1;
    this->stack = NULL;
  }

  ~KeyStackPair() {
    if(NULL != this->stack) {
      // release all memory held by this stack
      while(!stack->empty()) {
	KeyDegreePair *p = stack->top();
	stack->pop();
	delete p;
      }
      delete stack;
    }
  }

};

static KeyType rootKey = nextKey();
static ThreadEntriesType threadKeyIds = {0};
static std::map<jthread, KeyStackPair *> threads_map;

void updateThreadEntries() {
  keystore_put((void *)&rootKey, sizeof(KeyType),
	       (void *)&threadKeyIds, sizeof(ThreadEntriesType)
	       );
}

void addThreadEntry(KeyType key) {
  int idx = threadKeyIds.size;
  threadKeyIds.threads[idx] = key;
  threadKeyIds.size ++;
}

void startup(char *options) {
  setup(options);
  fprintf(logfile, "rootKey: %d \n", rootKey);
  keystore_initialize("keystore.db", NULL);

  metafile = fopen("edges.out", "w+");
  if(NULL == metafile) {
    fprintf(stderr, "Failed to create logfile: %s", traceFile);
    exit(-1);
  }

  threadKeyIds.size = 0;
  updateThreadEntries();
}

void shutdown() {
  fprintf(logfile, "Shutting down\n");

  keystore_destroy();

  if(NULL != metafile) {
    fclose(metafile);
  }

  if(NULL != logfile) {
    fclose(metafile);
  }
}


void setup(char* options) {

  excFilters = NULL;
  incFilters = NULL;

  /* Parse options passed to the agent */
  if(options == NULL || strcmp(options, "") == 0) {
    // no options specified, exit!
    exit(-1);
  }

  char *ptrOptions = strtok(options, OPTIONS_SEPARATOR);
  for(int i = 0; i < 100 && ptrOptions != NULL; i ++) {
    char *v = strstr(ptrOptions, OPT_VAL_SEPARATOR);
    if(NULL != v) {
      *v = '\0';
      v++;
      char *k = ptrOptions;
      options_map[k] = v;
    }
    ptrOptions = strtok(NULL, OPTIONS_SEPARATOR);
  }

  for( std::map<char*,char*>::iterator iter = options_map.begin();
       iter != options_map.end(); ++iter ) {
    char *key = (*iter).first;
    char *value = (*iter).second;

    if ( strcmp(key, "traceFile") == 0 ) {
      traceFile = strdup(value);
      logfile = fopen(traceFile, "a+");
      if(NULL == logfile) {
	fprintf(stderr, "Failed to create logfile: %s", traceFile);
	exit(-1);
      }
    }
    else if ( strcmp(key, "filterList") == 0 ) {
      setupFiltersFromString(value);
    }
    else if ( strcmp(key, "filterFile") == 0 ) {
      setupFiltersFromFile(value);
    }

  }

  monitor_lock = createMonitor("monitor_lock");

  fprintf(logfile, "DONE\n");
}

int getLock(LOCK_TYPE lock, LOCK_OBJECT *lockObj) {
  switch (lock) {

  case SHARED_LOCK:
    getMonitor(monitor_lock);
    {
      if((*lockObj) > -1) {
	(*lockObj) += 1;
      } else {
	releaseMonitor(monitor_lock);
	return -1;
      }
    }
    releaseMonitor(monitor_lock);
    break;
    
  case EXCLUSIVE_LOCK:
    getMonitor(monitor_lock);
    {
      if((*lockObj) == 0) {
	(*lockObj) = -1;
      } else {
	releaseMonitor(monitor_lock);
	return -1;
      }
    }
    releaseMonitor(monitor_lock);
    break;
  }
  return 1;
}

int releaseLock(LOCK_TYPE lock, LOCK_OBJECT *lockObj) {
  switch (lock) {
  case 1: getMonitor(monitor_lock); {
      if((*lockObj) > 0) {
	(*lockObj) -= 1;
      } else {
	releaseMonitor(monitor_lock);
	return -1;
      }
    } releaseMonitor(monitor_lock);
    break;
  case 2: getMonitor(monitor_lock); {
      if((*lockObj) == -1) {
	(*lockObj) = 0;
      } else {
	releaseMonitor(monitor_lock);
	return -1;
      }
    } releaseMonitor(monitor_lock);
    break;
  }
  return 1;
}

/*
  To be called before calling setCall/endCall, and
  only if it returns 1 we should call setCall/endCall.
*/
int passFilter(const char * input) {
  int i, j;
  int retval = 0;
  for(i = 0; i < incFilterLen; i ++) {
    retval = 1;
    if(strstr(input, incFilters[i]) == &input[0]) {
      for(j = 0; j < excFilterLen && strlen(incFilters[i]) < strlen(excFilters[j]); j ++) {
	if(strstr(input, excFilters[j]) == &input[0]) {
	  retval = 0;
	  break;
	}
      }
      if(retval != 0)
	return 1;
    }
  }
  return 0;
}


jrawMonitorID createMonitor(const char *name) {
  g_jvmti_env->CreateRawMonitor(name, &monitor_lock);
  return monitor_lock;
}

void getMonitor(jrawMonitorID monitor) {
  g_jvmti_env->RawMonitorEnter(monitor);
}

void releaseMonitor(jrawMonitorID monitor) {
  g_jvmti_env->RawMonitorExit(monitor);
}

void destroyMonitor(jrawMonitorID monitor) {
  g_jvmti_env->DestroyRawMonitor(monitor);
}

void delay(int i) {
  i *= 100;
  for(; i > 0; i --) {
    __asm ("pause");
  }
}

void JNICALL vmDeath(jvmtiEnv* jvmti_env, JNIEnv* jni_env) {
  // printFullTrace(jni_env);
  // releaseFullTrace(jni_env);
  destroyMonitor(monitor_lock);
  clearAllFilters();

  shutdown();
}

/*
  This function must be called within a critical section
  since it modifieds a shared datastructure for thread entries
*/
KeyStackPair * newThreadEntry(jthread thread) {
  KeyType threadId = nextKey();

  addThreadEntry(threadId);
  updateThreadEntries();

  KeyStackPair *pair = new KeyStackPair(threadId);
  threads_map[thread] = pair;

  KeyDegreeStack *stack = pair->stack;
  stack->push(new KeyDegreePair(pair->key, 0));

  fprintf(logfile, "Thread entry created: %d %p\n", threadId, thread);
  return pair;
}

/*
  This function must be called within a critical section
  since it modifieds a shared datastructure for thread entries
*/
void threadCleanup(jthread thread) {
  fprintf(logfile, "Thread exiting: %p\n", thread);
  KeyStackPair *pair =  threads_map[thread];
  delete pair;
  threads_map.erase(thread);
}

void printAllThreads() {
  fprintf(logfile, "All threads:\n");
  for(std::map<jthread, KeyStackPair*>::iterator iter
	= threads_map.begin();
      iter !=threads_map.end();
      iter++) {
    jthread thread = iter->first;
    KeyStackPair *pair = iter->second;
    fprintf(logfile, " key: %p , val: %d, stack: %p\n",
	   thread, pair->key, pair->stack);
  }
}

void JNICALL threadStart(jvmtiEnv *jvmti_env, JNIEnv* jni_env,
			 jthread thread) {
  //  obtain lock
  while(getLock(SHARED_LOCK, &assignThreadAccess) == -1) {delay(10);}

  newThreadEntry(thread);

  //  release lock
  releaseLock(SHARED_LOCK, &assignThreadAccess);

}

void JNICALL threadEnd(jvmtiEnv* jvmti_env, JNIEnv* jni_env,
		       jthread thread) {
  //  obtain lock
  while(getLock(SHARED_LOCK, &assignThreadAccess) == -1) {delay(10);} 

  // cleanup
  threadCleanup(thread);

  //  release lock
  releaseLock(SHARED_LOCK, &assignThreadAccess);
}

void JNICALL methodEntry(jvmtiEnv* jvmti_env, JNIEnv* jni_env,
			 jthread thread, jmethodID method) {

  // thread_key = thread_handle_id_map[thread]
  // class = class of this method
  // return if NOT class passes filter
  // fetch method_name, method_signature, class_name
  // keyV = next()
  // stack = thread_handle_stack_map[thread]
  // key = if stack.empty() then thread_key else stack.top().keyV
  // metadata = { method_name, method_signature, class_name }
  // value = { keyV, metadata }
  // store a key-value pair for this call
  // keystore[key] = value
  // stack.push(keyV)


  if(NULL == thread) return;

  // obtain calltrace lock
  while(getLock(SHARED_LOCK, &callTraceAccess) == -1) { delay(10); }

  jclass classId = NULL;
  char *cn = NULL, *gcn = NULL, *mn = NULL, *ms = NULL, *gms = NULL;
  
  jvmti_env->GetMethodName(method, &mn, &ms, &gms);
  jvmti_env->GetMethodDeclaringClass(method, &classId);
  jvmti_env->GetClassSignature(classId, &cn, &gcn);

  // only process the classes that are allowed
  if( passFilter(cn) == 1 ) {
    
    KeyStackPair *pair = NULL;
    
    // obtain thread access lock
    while(getLock(SHARED_LOCK, &assignThreadAccess) == -1) { delay(10); }
    
    if( threads_map.count(thread) == 0 ) {
      pair = newThreadEntry(thread);
    } else {
      pair = threads_map[thread];
    }

    fprintf(logfile, "Method Entry: TID %d: %s / %s\n", pair->key, cn, mn);
    KeyDegreeStack *stack = pair->stack;

    if(stack->empty()) {
      stack->push(new KeyDegreePair(pair->key));
    }

    KeyType parent_key = stack->top()->key;
    KeyType node_key = nextKey();

    fprintf(logfile, " Stack size: %ld\n", stack->size());
    
    int order = ++(stack->top()->degree);

    // int len_key = sizeof(parent_key);
    // int len_order = sizeof(order);
    // int len_mn = strlen(mn) + 1;
    // int len_ms = strlen(ms) + 1;
    // int len_cn = strlen(cn) + 1;

    // int vsize = len_key + len_order + len_mn + len_ms + len_cn;

    // char *value = (char*) malloc(vsize);
    // char *curr = NULL;
    // curr = value;
    // memcpy( curr, &parent_key, len_key); curr += len_key;
    // memcpy( curr, &order, len_order);  curr += len_order;
    // memcpy( curr, mn, len_mn);         curr += len_mn;
    // memcpy( curr, ms, len_ms);         curr += len_ms;
    // memcpy( curr, cn, len_cn);

    // keystore_put((void *)&node_key, sizeof(node_key),
    // 		 (void *)value, vsize);

    fprintf(metafile, "%d\t%d\t%d\t%s\t%s\t%s\n", node_key, parent_key, order, mn, ms, cn);

    // free(value);
    stack->push(new KeyDegreePair(node_key));

    // obtain thread access lock
    releaseLock(SHARED_LOCK, &assignThreadAccess);
    
  }

  
  jvmti_env->Deallocate( (unsigned char*) cn);
  jvmti_env->Deallocate( (unsigned char*) gcn);
  jvmti_env->Deallocate( (unsigned char*) mn);
  jvmti_env->Deallocate( (unsigned char*) ms);
  jvmti_env->Deallocate( (unsigned char*) gms);

  // release calltrace lock
  releaseLock(SHARED_LOCK, &callTraceAccess);
}

void JNICALL methodExit(jvmtiEnv* jvmti_env, JNIEnv* jni_env,
			jthread thread, jmethodID method,
			jboolean was_popped_by_exception,
			jvalue return_value) {

    // obtain thread access lock
    while(getLock(SHARED_LOCK, &assignThreadAccess) == -1) { delay(10); }
    
    if( threads_map.count(thread) == 0 ) {
      // ignore
      releaseLock(SHARED_LOCK, &assignThreadAccess);
      return;
    }

    KeyStackPair *pair = threads_map[thread];

    jclass classId = NULL;
    char *cn = NULL, *gcn = NULL, *mn = NULL, *ms = NULL, *gms = NULL;
  
    jvmti_env->GetMethodName(method, &mn, &ms, &gms);
    jvmti_env->GetMethodDeclaringClass(method, &classId);
    jvmti_env->GetClassSignature(classId, &cn, &gcn);

    KeyType threadId = pair->key;
    
    // only process the classes that are allowed
    if( passFilter(cn) == 1 ) {
      fprintf(logfile, "Method Exit: TID %d: %s / %s\n", threadId, cn, mn);
      // fprintf(logfile, "  Thread %p / Method %p\n", thread, method);
      // fprintf(logfile, "    Class: %s / %s\n", cn, gcn);
      // fprintf(logfile, "    Method: %s / %s / %s\n", mn, ms, gms);
      KeyDegreeStack *stack = pair->stack;
      if(! stack->empty() ) {
	KeyDegreePair *pair = stack->top();
	stack->pop();
	delete pair;
      }
    }


    jvmti_env->Deallocate( (unsigned char*) cn);
    jvmti_env->Deallocate( (unsigned char*) gcn);
    jvmti_env->Deallocate( (unsigned char*) mn);
    jvmti_env->Deallocate( (unsigned char*) ms);
    jvmti_env->Deallocate( (unsigned char*) gms);

    // obtain thread access lock
    releaseLock(SHARED_LOCK, &assignThreadAccess);
}

/* This is the startup method for JVMTI agent */
JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM *vm, char *options, void *reserved) {
  jint rc;
  jvmtiCapabilities capabilities;
  jvmtiEventCallbacks callbacks;
  jvmtiEnv *jvmti;

  printf("libjct.so: setting up ...\n");
  
  rc = vm->GetEnv((void **)&jvmti, JVMTI_VERSION);
  if (rc != JNI_OK) {
    fprintf(stderr, "ERROR: Unable to create jvmtiEnv, GetEnv failed, error=%d\n", rc);
    return -1;
  }

  jvmti->GetCapabilities(&capabilities);
  capabilities.can_generate_method_entry_events = 1;
  capabilities.can_generate_method_exit_events = 1;

  jvmti->AddCapabilities(&capabilities);

  memset(&callbacks, 0, sizeof(callbacks));
  callbacks.VMDeath = &vmDeath;
  callbacks.ThreadStart = &threadStart;
  callbacks.ThreadEnd = &threadEnd;
  callbacks.MethodEntry = &methodEntry;
  callbacks.MethodExit = &methodExit;

  jvmti->SetEventCallbacks(&callbacks, sizeof(callbacks));
  jvmti->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_CLASS_PREPARE, NULL);

  g_jvmti_env = jvmti;

  startup(options);
  
  jvmti->SetEventNotificationMode(JVMTI_ENABLE,
				  JVMTI_EVENT_VM_DEATH, NULL);
  jvmti->SetEventNotificationMode(JVMTI_ENABLE,
				  JVMTI_EVENT_THREAD_START, NULL);
  jvmti->SetEventNotificationMode(JVMTI_ENABLE,
				  JVMTI_EVENT_THREAD_END, NULL);
  jvmti->SetEventNotificationMode(JVMTI_ENABLE,
				  JVMTI_EVENT_METHOD_ENTRY, NULL);
  jvmti->SetEventNotificationMode(JVMTI_ENABLE,
				  JVMTI_EVENT_METHOD_EXIT, NULL);
  
  return JNI_OK;
}
