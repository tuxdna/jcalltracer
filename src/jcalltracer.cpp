#include "jcalltracer.h"
#include "keystore.h"

#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <map>

static jvmtiEnv *g_jvmti_env;

static keyValueType currentKey = 0;

threadEntriesType threadKeyIds = {0};
LOCK_TYPE SHARED_LOCK = 1;
LOCK_TYPE EXCLUSIVE_LOCK = 2;
LOCK_OBJECT classAccess = 0;
LOCK_OBJECT callTraceAccess = 0;
LOCK_OBJECT assignThreadAccess = 0;
jthread threads[MAX_THREADS];
struct CTD **callStart [MAX_THREADS];
struct CTD *currentCall [MAX_THREADS];
int callStartIdx [MAX_THREADS];
int callThershold = -1;
int nextThreadIdx = 0;
int maxThreadIdx = 0;
int maxClassIdx = 0;

char **incFilters;
char **excFilters;
int incFilterLen = 0;
int excFilterLen = 0;

const char *traceFile = "call.trace";
const char *output_type = "xml";
const char *usage = "uncontrolled";

mapDef optionsMap[100];

std::map<char*, char *> options_map;

jrawMonitorID monitor_lock;

keyValueType nextKey() {
  return ++currentKey;
}

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


/*To be called on JVM load.*/
void setup(char* options) {

  printf("setting up libcalltracer5 ...");
  for(int i = 0; i < MAX_THREADS; i ++) {
    callStart[i] = NULL;
    callStartIdx[i] = 0;
    currentCall[i] = NULL;
    threads[i] = NULL;
  }

  excFilters = NULL;
  incFilters = NULL;

  /* Parse options passed to the agent */
  if(options == NULL || strcmp(options, "") == 0) {
    // no options specified, exit!
    exit(-1);
  }

  char *ptrOptions = strtok(options, OPTIONS_SEPARATOR);
  for(int i = 0; i < 100 && ptrOptions != NULL; i ++) {
    optionsMap[i].name = ptrOptions;
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
    }
    else if ( strcmp(key, "filterList") == 0 ) {
      setupFiltersFromString(value);
    }
    else if ( strcmp(key, "filterFile") == 0 ) {
      setupFiltersFromFile(value);
    }
    else if ( strcmp(key, "outputType") == 0 ) {
      output_type = strdup(value);
    }
    else if ( strcmp(key, "usage") == 0 ) {
      usage = strdup(value);
    }
    else if ( strcmp(key, "callThershold") == 0 ) {
      callThershold = atoi(value);
    }
  }

  monitor_lock = createMonitor("monitor_lock");
  printf("DONE\n");
}

#include <map>
#include <stack>

void setupDataStructures() {
  
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

int getLock(LOCK_TYPE lock, LOCK_OBJECT *lockObj) {
  switch (lock) {
  case 1: getMonitor(monitor_lock); {
      if((*lockObj) > -1) {
	(*lockObj) += 1;
      } else {
	releaseMonitor(monitor_lock);
	return -1;
      }
    } releaseMonitor(monitor_lock);
    break;
  case 2: getMonitor(monitor_lock); {
      if((*lockObj) == 0) {
	(*lockObj) = -1;
      } else {
	releaseMonitor(monitor_lock);
	return -1;
      }
    } releaseMonitor(monitor_lock);
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

CTD *newCallTrace() {
  return (CTD*) malloc(sizeof(CTD));
}

jthread getThreadId(int threadIdx) {
  jthread threadId = NULL;
  if(threadIdx < 0 && threadIdx > 999)
    return NULL;
  while(getLock(SHARED_LOCK, &assignThreadAccess) == -1) {
    delay(10);
  }{
    threadId = threads[threadIdx];
  } releaseLock(SHARED_LOCK, &assignThreadAccess);
  return threadId;
}

/*Method to get the index of the current thread.*/
int getThreadIdx(jthread threadId, JNIEnv* jni_env) {
  int i = 0;
  if(threadId == NULL)
    return -1;
  while(getLock(SHARED_LOCK, &assignThreadAccess) == -1) {
    delay(10);
  }{
    for(i = 0; i < MAX_THREADS; i++) {
      if(isSameThread(jni_env, threads[i], threadId)) {
	releaseLock(SHARED_LOCK, &assignThreadAccess);
	return i;
      }
    }
  } releaseLock(SHARED_LOCK, &assignThreadAccess);
  return -1;
}

/*To be called before calling setCall/endCall, and only if it returns 1 we should call setCall/endCall.*/
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

void releaseCallTrace(CTD* headNode) {
  int i = 0;
  if(headNode == NULL)
    return;
  for(i = 0; i < headNode->callIdx; i++) {
    releaseCallTrace(headNode->called[i]);
  }
  free(headNode->called);
  free(headNode->methodName);
  free(headNode->methodSignature);
  free(headNode->className);
  free(headNode);
}

void releaseFullThreadTrace(jthread threadId, JNIEnv* jni_env) {
  int threadIdx = getThreadIdx(threadId, jni_env);
  int i;
  if(threadIdx == -1)
    return;
  for(i = 0; i < callStartIdx[threadIdx]; i++) {
    releaseCallTrace(callStart[threadIdx][i]);
  }
  free(callStart[threadIdx]);
  callStartIdx[threadIdx] = 0;
  currentCall[threadIdx] = NULL;
  threads[threadIdx] = NULL;
}

/* To be called from a JNI method or on JVM shut down.*/
void releaseFullTrace(JNIEnv* jni_env) {
  int i;
  jthread threadId;

  while(getLock(EXCLUSIVE_LOCK, &callTraceAccess) == -1) {
    delay(10);
  }

  {
    for(i = 0; i < maxThreadIdx; i++) {
      threadId = getThreadId(i);
      if(threadId != NULL)
	releaseFullThreadTrace(threadId, jni_env);
    }
    maxThreadIdx = 0;
    nextThreadIdx = 0;
  }

  releaseLock(EXCLUSIVE_LOCK, &callTraceAccess);
}

void printFullThreadTrace(jthread threadId, FILE *out, JNIEnv* jni_env);

int assignThreadIdx(jthread threadId, JNIEnv* jni_env) {
  int threadIdx = -1;
  FILE *out;
  char * newFile;
  jthread oldThreadId = NULL;
  jthread threadRef = getThreadRef(jni_env, threadId);
  while(getLock(EXCLUSIVE_LOCK, &assignThreadAccess) == -1) {
    delay(10);
  }{
    threadIdx = nextThreadIdx;
    oldThreadId = threads[nextThreadIdx];
    if(nextThreadIdx < maxThreadIdx) {
      newFile = (char *) calloc(strlen(traceFile) + 100, sizeof(char));
      sprintf(newFile, "%s.%p", traceFile, oldThreadId);
      out = fopen(newFile, "a");
      if( out == NULL )
	out = stderr;
      fprintf(out, "libcalltracer5\n");
      printFullThreadTrace(oldThreadId, out, jni_env);
      fclose(out);
      free(newFile);
      releaseFullThreadTrace(oldThreadId, jni_env);
    }
    threads[nextThreadIdx++] =  threadRef;
    if(maxThreadIdx < nextThreadIdx) {
      maxThreadIdx = nextThreadIdx;
    }
    nextThreadIdx = nextThreadIdx%MAX_THREADS;
  } releaseLock(EXCLUSIVE_LOCK, &assignThreadAccess);
  return threadIdx;
}

// #if defined JVMTI_TYPE
CTD *setCall(char* methodName, char* methodSignature, char* className, CTD* calledFrom, CTD* call, int threadIdx) {
  CTD ** temp;
  while(getLock(SHARED_LOCK, &callTraceAccess) == -1) {
    delay(10);
  }

  {
    if(threadIdx == -1 || (callStartIdx[threadIdx] >= callThershold && callThershold > -1)) {
      free(call);
      releaseLock(SHARED_LOCK, &callTraceAccess);
      return NULL;
    }
    if(calledFrom != NULL && (calledFrom->callIdx >= callThershold && callThershold > -1)) {
      free(call);
      calledFrom->offset++;
      releaseLock(SHARED_LOCK, &callTraceAccess);
      return NULL;
    }
    call->callIdx = 0;
    call->offset = 0;
    call->methodName = methodName;
    call->methodSignature = methodSignature;
    call->className = className;
    call->calledFrom = calledFrom;
    call->called = NULL;
    if(calledFrom == NULL) {
      temp = (CTD **)realloc(callStart[threadIdx], (++ (callStartIdx[threadIdx])) * sizeof(CTD *));
      if (temp != NULL) {
	callStart[threadIdx] = temp;
	callStart[threadIdx][callStartIdx[threadIdx] - 1] = call;
      }
    } else {
      temp = (CTD **)realloc(calledFrom->called, (++ (calledFrom->callIdx)) * sizeof(CTD *));
      if (temp != NULL) {
	calledFrom->called = temp; /* OK, assign new, larger storage to pointer */
	calledFrom->called[calledFrom->callIdx - 1] = call;
      } else {
	free(call);
	calledFrom->callIdx --;
	calledFrom->offset++;
	releaseLock(SHARED_LOCK, &callTraceAccess);
	return NULL;
      }
    }
    currentCall[threadIdx] = call;
  }

  releaseLock(SHARED_LOCK, &callTraceAccess);

  return call;
}

/*To be called when any method of a thread returns.*/
CTD *endCall(jmethodID methodId, jthread threadId, JNIEnv* jni_env) {
  int threadIdx = -1;
  jclass classId;
  while(getLock(SHARED_LOCK, &callTraceAccess) == -1) {
    delay(10);
  }

  {
    classId = getMethodClass(methodId);
    if(classId == NULL) {
      releaseLock(SHARED_LOCK, &callTraceAccess);
      return NULL;
    }
    
    threadIdx = getThreadIdx(threadId, jni_env);

    if(threadIdx == -1 
       || callStartIdx[threadIdx] <= 0 
       || (callStartIdx[threadIdx] >= callThershold 
	   && callThershold > -1)
       ) {
      releaseLock(SHARED_LOCK, &callTraceAccess);
      return NULL;
    }
    
    if (currentCall[threadIdx] != NULL
	&& currentCall[threadIdx]->offset == 0) {
      currentCall[threadIdx] = currentCall[threadIdx]->calledFrom;
    }
    else if (currentCall[threadIdx] != NULL
	     && currentCall[threadIdx]->offset > 0) {
      currentCall[threadIdx]->offset --;
    }

    releaseLock(SHARED_LOCK, &callTraceAccess);
    return currentCall[threadIdx];

  }
  releaseLock(SHARED_LOCK, &callTraceAccess);

  return NULL;
}

/*To be called when a new method is invoked in a flow of a thread.*/
CTD *newMethodCall(jmethodID methodId, jthread threadId, JNIEnv* jni_env) {
  jclass classId;
  int threadIdx = -1;
  CTD *callTrace;
  while(getLock(SHARED_LOCK, &callTraceAccess) == -1) {
    delay(10);
  }

  {
    classId = getMethodClass(methodId);
    if(classId == NULL) {
      releaseLock(SHARED_LOCK, &callTraceAccess);
      return NULL;
    }

    threadIdx = getThreadIdx(threadId, jni_env);
    if(threadIdx == -1) {
      threadIdx = assignThreadIdx(threadId, jni_env);
    }

    /* char *methodName = getMethodName(methodId); */
    /* char *methodSignature = getMethodSignature(methodId); */

    char *className = getClassName(classId);
    char *methodName = NULL;
    char *methodSignature = NULL;
    getMethodNameAndSignature(methodId, 
			      &methodName,
			      &methodSignature);

    callTrace = setCall(methodName,
			methodSignature,
			className,
			currentCall[threadIdx],
			newCallTrace(),
			threadIdx);

    releaseLock(SHARED_LOCK, &callTraceAccess);
    return callTrace;
  }

  releaseLock(SHARED_LOCK, &callTraceAccess);
  return NULL;
}

void printCallTrace(CTD* headNode, int depth, FILE *out) {
  int i = 0;

  if(headNode == NULL)
    return;

  depth ++;
  for(i = 0; i < depth; i++) {
    fprintf(out, "\t");
  }
  if(strcmp(output_type, "xml") == 0) {
    fprintf(out, "<call>\n");
    for(i = 0; i < depth; i++) {
      fprintf(out, "\t");
    }
    fprintf(out, "\t<class><![CDATA[%s]]></class>\n", headNode->className);
    for(i = 0; i < depth; i++) {
      fprintf(out, "\t");
    }
    fprintf(out, "\t<method><![CDATA[%s %s]]></method>\n", headNode->methodName, headNode->methodSignature);
  } else {
    if(depth > 0)
      fprintf(out, "-->");
    fprintf(out, "[%s{%s %s}]\n", headNode->className, headNode->methodName, headNode->methodSignature);
  }
  for(i = 0; i < headNode->callIdx; i++) {
    printCallTrace(headNode->called[i], depth, out);
  }
  if(strcmp(output_type, "xml") == 0) {
    for(i = 0; i < depth; i++) {
      fprintf(out, "\t");
    }
    fprintf(out, "</call>\n");
  }
}

void printFullThreadTrace(jthread threadId, FILE *out, JNIEnv* jni_env) {
  int threadIdx = getThreadIdx(threadId, jni_env);
  int i;
  if(threadIdx == -1)
    return;
  if(strcmp(output_type, "xml") == 0) {
    fprintf(out, "\n<Thread id=\"%p\">\n", threadId);
    for(i = 0; i < callStartIdx[threadIdx]; i++)
      printCallTrace(callStart[threadIdx][i], 0, out);
    fprintf(out, "</Thread>\n");
  } else {
    fprintf(out, "\n------------Thread %p--------------\n", threadId);
    for(i = 0; i < callStartIdx[threadIdx]; i++)
      printCallTrace(callStart[threadIdx][i], -1, out);
    fprintf(out, "\n\n");
  }
}

/*This method will print the trace into the traceFile. To be called from a JNI method.*/
void printFullTrace(JNIEnv* jni_env) {
  int i;
  jthread threadId;
  FILE *out;

  while(getLock(EXCLUSIVE_LOCK, &callTraceAccess) == -1) {
    delay(10);
  }
  
  {
    out = fopen(traceFile, "w");

    if( out == NULL )
      out = stderr;

    for(i = 0; i < maxThreadIdx; i++) {
      threadId = getThreadId(i);
      if(threadId != NULL) {
	printFullThreadTrace(threadId, out, jni_env);
      }
    }
    fclose(out);
  }

  releaseLock(EXCLUSIVE_LOCK, &callTraceAccess);
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

jclass getMethodClass(jmethodID methodId) {
  jclass declaring_class_ptr;
  g_jvmti_env->GetMethodDeclaringClass(methodId, &declaring_class_ptr);
  if(passFilter(getClassName(declaring_class_ptr)) == 0) {
    declaring_class_ptr = NULL;
  }
  return declaring_class_ptr;
}

bool isSameThread(JNIEnv* jni_env, jthread threadId1, jthread threadId2) {
  return jni_env->IsSameObject(threadId1, threadId2);
}

bool isSameClass(JNIEnv* jni_env, jclass classId1, jclass classId2) {
  return jni_env->IsSameObject(classId1, classId2);
}

jthread getThreadRef(JNIEnv* jni_env, jthread threadId) {
  return (jthread) jni_env->NewWeakGlobalRef(threadId);
}

jclass getClassRef(JNIEnv* jni_env, jclass classId) {
  return (jclass) jni_env->NewWeakGlobalRef(classId);
}

char * getClassName(jclass klass) {
  char *className;
  char *gclassName;
  char *tmp;
  g_jvmti_env->GetClassSignature(klass,
				 &className,
				 &gclassName);
  tmp = strdup(className);

  g_jvmti_env->Deallocate( (unsigned char*) className);
  g_jvmti_env->Deallocate( (unsigned char*) gclassName);

  return tmp;
}

char * getMethodName(jmethodID methodId) {
  char *methodName = NULL;
  char *methodSignature = NULL;
  char *gmethodSignature = NULL;
  char *tmp = NULL;
  g_jvmti_env->GetMethodName(methodId, 
			     &methodName,
			     &methodSignature,
			     &gmethodSignature);
  tmp = strdup(methodName);

  g_jvmti_env->Deallocate((unsigned char*) methodName);
  g_jvmti_env->Deallocate((unsigned char*) methodSignature);
  g_jvmti_env->Deallocate((unsigned char*) gmethodSignature);

  return tmp;
}

char * getMethodSignature(jmethodID methodId) {
  char *methodName;
  char *methodSignature;
  char *gmethodSignature;
  char *tmp;
  g_jvmti_env->GetMethodName(methodId,
			     &methodName,
			     &methodSignature,
			     &gmethodSignature);
  tmp = strdup(methodSignature);

  g_jvmti_env->Deallocate( (unsigned char*) methodName);
  g_jvmti_env->Deallocate( (unsigned char*) methodSignature);
  g_jvmti_env->Deallocate( (unsigned char*) gmethodSignature);

  return tmp;
}

int getMethodNameAndSignature(jmethodID methodId,
				 char **name, 
				 char **signature) {
  char *methodName;
  char *methodSignature;
  char *gmethodSignature;
  g_jvmti_env->GetMethodName(methodId,
			     &methodName,
			     &methodSignature,
			     &gmethodSignature);
  if( NULL != name) {
    *name = strdup(methodName);
  }
  if( NULL != signature) {
    *signature = strdup(methodSignature);
  }

  g_jvmti_env->Deallocate( (unsigned char*) methodName);
  g_jvmti_env->Deallocate( (unsigned char*) methodSignature);
  g_jvmti_env->Deallocate( (unsigned char*) gmethodSignature);

  return 0;
}

void JNICALL vmDeath(jvmtiEnv* jvmti_env, JNIEnv* jni_env) {
  keystore_destroy();
  printFullTrace(jni_env);
  releaseFullTrace(jni_env);
  destroyMonitor(monitor_lock);
  clearAllFilters();
}

void JNICALL threadStart(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread) {
  // initialize thread variables
  // startnodeid = 0
  // currentnodeid = 
}

void JNICALL threadEnd(jvmtiEnv* jvmti_env, JNIEnv* jni_env, jthread thread) {
  // cleanup thread variables
}

void JNICALL methodEntry(jvmtiEnv* jvmti_env, JNIEnv* jni_env, jthread thread, jmethodID method) {

  // class = method_class
  // return if NOT class passes filter
  // fetch method_name, method_signature, class_name
  // keyV = next()
  // thread_key = thread_handle_id_map[thread]
  // stack = thread_handle_stack_map[thread]
  // key = if stack.empty() then thread_key else stack.top().keyV
  // metadata = { method_name, method_signature, class_name }
  // value = { keyV, metadata }
  // keystore[key] = value
  // stack.push(keyV)

  CTD *c = newMethodCall(method, thread, jni_env);

  if(NULL != c) {

    int lenMethodName = strlen(c->methodName) + 1;
    int lenMethodSignature = strlen(c->methodSignature) + 1;
    int lenClassName = strlen(c->className) + 1;
    int vsize = lenMethodName + lenMethodSignature + lenClassName;

    char *value = (char*) malloc(vsize);
    memcpy(value,
	   c->methodName, lenMethodName);
    memcpy(value+lenMethodName,
	   c->methodSignature, lenMethodSignature);
    memcpy(value+lenMethodName+lenMethodSignature,
	   c->className, lenClassName);

    keystore_put((void *)method, sizeof(void*),
		 (void *)value, vsize
		 );
  }
  // create method call object
  // store a key-value pair for this call
  // nodeid = currentnodeid
  // currentnodeid ++
  // caller_id = 
  // called_id = 
}

void JNICALL methodExit(jvmtiEnv* jvmti_env, JNIEnv* jni_env, jthread thread, jmethodID method, jboolean was_popped_by_exception, jvalue return_value) {
  endCall(method, thread, jni_env);
}

/* This is the startup method for JVMTI agent */
JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM *vm, char *options, void *reserved) {
  jint rc;
  jvmtiCapabilities capabilities;
  jvmtiEventCallbacks callbacks;
  jvmtiEnv *jvmti;

  printf("Loading agent: libcalltracer5 ...\n");

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

  keystore_initialize("keystore.db", "links");
  setup(options);
  
  if(strcmp(usage, "uncontrolled") == 0) {
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_DEATH, NULL);
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_THREAD_START, NULL);
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_THREAD_END, NULL);
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_METHOD_ENTRY, NULL);
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_METHOD_EXIT, NULL);
  }

  return JNI_OK;
}
