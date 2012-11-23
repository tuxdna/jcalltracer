/*
 * Copyright 2009 Syed Ali Jafar Naqvi
 *
 * This file is part of Java Call Tracer.
 *
 * Java Call Tracer is free software: you can redistribute it and/or modify
 * it under the terms of the Lesser GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Java Call Tracer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * Lesser GNU General Public License for more details.
 *
 * You should have received a copy of the Lesser GNU General Public License
 * along with Java Call Tracer.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if !defined OPTION_SEP
#define OPTION_SEP "="
#endif

#if !defined MAX_THREADS
#define MAX_THREADS 1000
#endif

typedef struct callTraceDef {
  char *methodName;
  char *methodSignature;
  char *className;
  struct callTraceDef *calledFrom;
  struct callTraceDef **called;
  int offset;
  int callIdx;
} callTraceDef;

//#if !defined JVMTI_TYPE
//typedef struct methodDef {
//  methodIdType methodId;
//  char *methodName;
//  char *methodSignature;
//} methodDef;
//
//typedef struct classDef {
//  classIdType classId;
//  char *className;
//  int num_methods;
//  struct methodDef *methods;
//} classDef;
//
//struct classDef **classes;
//#endif

typedef int LOCK_TYPE;
typedef int LOCK_OBJECT;

LOCK_TYPE SHARED_LOCK = 1;
LOCK_TYPE EXCLUSIVE_LOCK = 2;

LOCK_OBJECT classAccess = 0;
LOCK_OBJECT callTraceAccess = 0;
LOCK_OBJECT assignThreadAccess = 0;

threadIdType threads[MAX_THREADS];
struct callTraceDef **callStart [MAX_THREADS];
struct callTraceDef *currentCall [MAX_THREADS];
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

typedef struct mapDef {
  char* name;
  char* value;
}mapDef;

mapDef optionsMap[100];

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

/*To be called on JVM load.*/
void setup(char* options) {
  int i, j;
  char *ptrOptions;
  char *ptrfilter;
  char **tmp;
  FILE *filterFile;
  char line[128];

  printf("Loading agent: libcalltracer5\n\n");
  for(i = 0; i < MAX_THREADS; i ++) {
    callStart[i] = NULL;
    callStartIdx[i] = 0;
    currentCall[i] = NULL;
    threads[i] = NULL;
  }

  excFilters = NULL;
  incFilters = NULL;
#if !defined JVMTI_TYPE
  classes = NULL;
#endif
  if(options != NULL && strcmp(options, "") != 0) {
    ptrOptions = strtok(options, ",");
    for(i = 0; i < 100 && ptrOptions != NULL; i ++) {
      optionsMap[i].name = ptrOptions;
      ptrOptions = strtok(NULL, ",");
    }
    if(i == 0) {
      optionsMap[i].name = options;
      i++;
    }

    for(j = 0; j < i; j ++) {
      ptrOptions = strtok(optionsMap[j].name, OPTION_SEP);
      optionsMap[j].value = strtok(NULL, OPTION_SEP);
      if(optionsMap[j].value == NULL) {
	continue;
      }
      optionsMap[j].name = ptrOptions;

      if(strcmp(optionsMap[j].name, "traceFile") == 0) {
	traceFile = strdup(optionsMap[j].value);
      } else if(strcmp(optionsMap[j].name, "filterList") == 0) {
	ptrfilter = strtok(optionsMap[j].value, "|");
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
      } else if(strcmp(optionsMap[j].name, "filterFile") == 0) {
	filterFile = fopen(optionsMap[j].value, "r");
	if(filterFile != NULL) {
	  while(fgets(line, 128, filterFile) != NULL) {
	    if(line[strlen(line) - 1] == '\n' || line[strlen(line) - 1] == '\r') {
	      if(line[strlen(line) - 1] == '\r' && line[strlen(line) - 2] == '\n') {
		line[strlen(line) - 2] = '\0';
	      } else if(line[strlen(line) - 1] == '\n' && line[strlen(line) - 2] == '\r') {
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
      } else if(strcmp(optionsMap[j].name, "outputType") == 0) {
	output_type = strdup(optionsMap[j].value);
      } else if(strcmp(optionsMap[j].name, "usage") == 0) {
	usage = strdup(optionsMap[j].value);
      } else if(strcmp(optionsMap[j].name, "callThershold") == 0) {
	callThershold = atoi(strdup(optionsMap[j].value));
      }
    }
  }
  monitor_lock = createMonitor("monitor_lock");
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

callTraceDef *newCallTrace() {
  return (callTraceDef*) malloc(sizeof(callTraceDef));
}

threadIdType getThreadId(int threadIdx) {
  threadIdType threadId = NULL;
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
int getThreadIdx(threadIdType threadId, JNIEnv* jni_env) {
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

void releaseCallTrace(callTraceDef* headNode) {
  int i = 0;
  if(headNode == NULL)
    return;
  for(i = 0; i < headNode->callIdx; i++) {
    releaseCallTrace(headNode->called[i]);
  }
  free(headNode->called);
  free(headNode->methodName);
  free(headNode->className);
  free(headNode);
}

void releaseFullThreadTrace(threadIdType threadId, JNIEnv* jni_env) {
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
  threadIdType threadId;

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

void printFullThreadTrace(threadIdType threadId, FILE *out, JNIEnv* jni_env);

int assignThreadIdx(threadIdType threadId, JNIEnv* jni_env) {
  int threadIdx = -1;
  FILE *out;
  char * newFile;
  threadIdType oldThreadId = NULL;
  threadIdType threadRef = getThreadRef(jni_env, threadId);
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
callTraceDef *setCall(char* methodName, char* methodSignature, char* className, callTraceDef* calledFrom, callTraceDef* call, int threadIdx) {
  callTraceDef ** temp;
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
      temp = (callTraceDef **)realloc(callStart[threadIdx], (++ (callStartIdx[threadIdx])) * sizeof(callTraceDef *));
      if (temp != NULL) {
	callStart[threadIdx] = temp;
	callStart[threadIdx][callStartIdx[threadIdx] - 1] = call;
      }
    } else {
      temp = (callTraceDef **)realloc(calledFrom->called, (++ (calledFrom->callIdx)) * sizeof(callTraceDef *));
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
callTraceDef *endCall(methodIdType methodId, threadIdType threadId, JNIEnv* jni_env) {
  int threadIdx = -1;
  classIdType classId;
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
    if(threadIdx == -1 || callStartIdx[threadIdx] <= 0 || (callStartIdx[threadIdx] >= callThershold && callThershold > -1)) {
      releaseLock(SHARED_LOCK, &callTraceAccess);
      return NULL;
    }
    
    if(currentCall[threadIdx] != NULL && currentCall[threadIdx]->offset == 0) {
      currentCall[threadIdx] = currentCall[threadIdx]->calledFrom;
    } else if(currentCall[threadIdx] != NULL && currentCall[threadIdx]->offset > 0) {
      currentCall[threadIdx]->offset --;
    }
    
    releaseLock(SHARED_LOCK, &callTraceAccess);
    return currentCall[threadIdx];

  }
  releaseLock(SHARED_LOCK, &callTraceAccess);

  return NULL;
}

/*To be called when a new method is invoked in a flow of a thread.*/
callTraceDef *newMethodCall(methodIdType methodId, threadIdType threadId, JNIEnv* jni_env) {
  classIdType classId;
  int threadIdx = -1;
  callTraceDef *callTrace;
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

    callTrace = setCall(getMethodName(methodId), getMethodSignature(methodId), getClassName(classId), currentCall[threadIdx], newCallTrace(), threadIdx);
    releaseLock(SHARED_LOCK, &callTraceAccess);
    return callTrace;
  }

  releaseLock(SHARED_LOCK, &callTraceAccess);
  return NULL;
}

// #else
//callTraceDef *setCall(char* methodName, char* methodSignature, char* className, callTraceDef* calledFrom, callTraceDef* call, int threadIdx) {
//  callTraceDef ** temp;
//  while(getLock(SHARED_LOCK, &callTraceAccess) == -1) {
//    delay(10);
//  }{
//    if(threadIdx == -1 || (callStartIdx[threadIdx] >= callThershold && callThershold > -1)) {
//      free(call);
//      releaseLock(SHARED_LOCK, &callTraceAccess);
//      return NULL;
//    }
//    if(calledFrom != NULL && (calledFrom->callIdx >= callThershold && callThershold > -1)) {
//      free(call);
//      calledFrom->offset++;
//      releaseLock(SHARED_LOCK, &callTraceAccess);
//      return NULL;
//    }
//    call->callIdx = 0;
//    call->offset = 0;
//    call->methodName = strdup(methodName);
//    call->methodSignature = strdup(methodSignature);
//    call->className = strdup(className);
//    call->calledFrom = calledFrom;
//    call->called = NULL;
//    if(calledFrom == NULL) {
//      temp = (callTraceDef **)realloc(callStart[threadIdx], (++ (callStartIdx[threadIdx])) * sizeof(callTraceDef *));
//      if (temp != NULL) {
//	callStart[threadIdx] = temp;
//	callStart[threadIdx][callStartIdx[threadIdx] - 1] = call;
//      }
//    } else {
//      temp = (callTraceDef **)realloc(calledFrom->called, (++ (calledFrom->callIdx)) * sizeof(callTraceDef *));
//      if (temp != NULL) {
//	calledFrom->called = temp; /* OK, assign new, larger storage to pointer */
//	calledFrom->called[calledFrom->callIdx - 1] = call;
//      } else {
//	free(call);
//	calledFrom->callIdx --;
//	calledFrom->offset++;
//	releaseLock(SHARED_LOCK, &callTraceAccess);
//	return NULL;
//      }
//    }
//    currentCall[threadIdx] = call;
//  } releaseLock(SHARED_LOCK, &callTraceAccess);
//  return call;
//}
//
///*To be called when any method of a thread returns.*/
//callTraceDef *endCall(methodIdType methodId, threadIdType threadId, JNIEnv* jni_env) {
//  int threadIdx = -1;
//  int i = 0;
//  classIdType classId;
//
//  while(getLock(SHARED_LOCK, &classAccess) == -1) {
//    delay(10);
//  }
//
//  {
//    classId = getMethodClass(methodId);
//    for(i = 0; i < maxClassIdx; i ++) {
//      if(classes[i] != NULL && isSameClass(jni_env, classes[i]->classId, classId)) {
//
//	while(getLock(SHARED_LOCK, &callTraceAccess) == -1) {
//	  delay(10);
//	}
//
//	{
//	  threadIdx = getThreadIdx(threadId, jni_env);
//	  if(threadIdx == -1 || callStartIdx[threadIdx] <= 0 || (callStartIdx[threadIdx] >= callThershold && callThershold > -1)) {
//	    releaseLock(SHARED_LOCK, &callTraceAccess);
//	    releaseLock(SHARED_LOCK, &classAccess);
//	    return NULL;
//	  }
//	  if(currentCall[threadIdx] != NULL && currentCall[threadIdx]->offset == 0) {
//	    currentCall[threadIdx] = currentCall[threadIdx]->calledFrom;
//	  } else if(currentCall[threadIdx] != NULL && currentCall[threadIdx]->offset > 0) {
//	    currentCall[threadIdx]->offset --;
//	  }
//	}
//
//	releaseLock(SHARED_LOCK, &callTraceAccess);
//	releaseLock(SHARED_LOCK, &classAccess);
//	return currentCall[threadIdx];
//      }
//    }
//  }
//
//  releaseLock(SHARED_LOCK, &classAccess);
//  return NULL;
//}
//
///*To be called when a new method is invoked in a flow of a thread.*/
//callTraceDef *newMethodCall(methodIdType methodId, threadIdType threadId, JNIEnv* jni_env) {
//  classIdType classId;
//  int i = 0, j = 0, threadIdx = -1;
//  callTraceDef *callTrace;
//  while(getLock(SHARED_LOCK, &classAccess) == -1) {
//    delay(10);
//  }{
//    classId = getMethodClass(methodId);
//    for(i = 0; i < maxClassIdx; i ++) {
//      if(classes[i] != NULL && isSameClass(jni_env, classes[i]->classId, classId)) {
//	for(j = 0; j < classes[i]->num_methods; j ++) {
//	  if(classes[i]->methods != NULL && classes[i]->methods[j].methodId == methodId) {
//	    while(getLock(SHARED_LOCK, &callTraceAccess) == -1) {
//	      delay(10);
//	    }{
//	      threadIdx = getThreadIdx(threadId, jni_env);
//	      if(threadIdx == -1) {
//		threadIdx = assignThreadIdx(threadId, jni_env);
//	      }
//	      callTrace = setCall(classes[i]->methods[j].methodName, classes[i]->methods[j].methodSignature, classes[i]->className, currentCall[threadIdx], newCallTrace(), threadIdx);
//	      releaseLock(SHARED_LOCK, &callTraceAccess);
//	      releaseLock(SHARED_LOCK, &classAccess);
//	      return callTrace;
//	    } releaseLock(SHARED_LOCK, &callTraceAccess);
//	  }
//	}
//      }
//    }
//  } releaseLock(SHARED_LOCK, &classAccess);
//  return NULL;
//}

// #endif

void printCallTrace(callTraceDef* headNode, int depth, FILE *out) {
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

void printFullThreadTrace(threadIdType threadId, FILE *out, JNIEnv* jni_env) {
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
  threadIdType threadId;
  FILE *out;

  while(getLock(EXCLUSIVE_LOCK, &callTraceAccess) == -1) {
    delay(10);
  }
  
  {
    out = fopen(traceFile, "w");
    if( out == NULL )
      out = stderr;
    /* fprintf(out, "libcalltracer5\n"); */
    /* fprintf(out, "********************************\n"); */
    /* fprintf(out, "          Call Trace\n"); */
    /* fprintf(out, "********************************\n\n"); */

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

//#if !defined JVMTI_TYPE
//int getFreeIdx() {
//  int i;
//  for(i = 0; i < maxClassIdx; i+=100) {
//    if(classes[i] == NULL) {
//      break;
//    }
//  }
//  for(; i < maxClassIdx && i > 0; i-=10) {
//    if(classes[i] != NULL) {
//      break;
//    }
//  }
//  for(; i < maxClassIdx; i++) {
//    if(classes[i] == NULL) {
//      break;
//    }
//  }
//  if (i >= maxClassIdx)
//    return -1;
//  return i;
//}
//
//void freeClass(classDef *thisClass) {
//  int j;
//  if(thisClass != NULL) {
//    for(j = 0; j < thisClass->num_methods; j++) {
//      free(thisClass->methods[j].methodName);
//      free(thisClass->methods[j].methodSignature);
//    }
//    free(thisClass->methods);
//    free(thisClass->className);
//    free(thisClass);
//  }
//}
//
///* To be called on class load.*/
//classDef *newClass(classIdType classId, const char * className, int num_methods, methodType *methods, JNIEnv* jni_env) {
//  classDef *thisClass = NULL;
//  classDef **tmp = NULL;
//  int idx = -1;
//  int i = 0;
//  classIdType classRef = getClassRef(jni_env, classId);
//  if(passFilter(className) == 1) {
//
//    thisClass = (classDef *)malloc(sizeof(classDef));
//    thisClass->classId = classRef;
//    thisClass->num_methods = num_methods;
//    thisClass->className = strdup(className);
//    thisClass->methods = (methodDef*)malloc(sizeof(methodDef) * (thisClass->num_methods));
//    for(i = 0; i < num_methods; i ++) {
//      thisClass->methods[i].methodId = methods[i].method_id;
//      thisClass->methods[i].methodName = strdup(methods[i].method_name);
//      thisClass->methods[i].methodSignature = strdup(methods[i].method_signature);
//    }
//    while(getLock(EXCLUSIVE_LOCK, &classAccess) == -1) {
//      delay(10);
//    }{
//      idx = getFreeIdx();
//      if(idx == -1) {
//	tmp = (classDef **) realloc(classes, (++ maxClassIdx) * sizeof(classDef *));
//	if(tmp != NULL) {
//	  classes = tmp;
//	  idx = maxClassIdx - 1;
//	} else {
//	  freeClass(thisClass);
//	  releaseLock(EXCLUSIVE_LOCK, &classAccess);
//	  return NULL;
//	}
//      }
//      classes[idx] = thisClass;
//    } releaseLock(EXCLUSIVE_LOCK, &classAccess);
//  }
//  return thisClass;
//}
//
///* To be called on class unload.*/
//void freeClassId(classIdType classId, JNIEnv* jni_env) {
//  int j = 0;
//  int i = 0;
//  for(i = 0; i < maxClassIdx; i ++) {
//    if(classes[i] != NULL && isSameClass(jni_env, classes[i]->classId, classId)) {
//      freeClass(classes[i]);
//      while(getLock(EXCLUSIVE_LOCK, &classAccess) == -1) {
//	delay(10);
//      }{
//	for(j = i; (classes[j] != NULL || classes[j + 1] != NULL) && (i%100) != 0 && j < (((i/100)*100) + 100); j++) {
//	  classes[j] = classes[j + 1];
//	}
//	classes[j] = NULL;
//      } releaseLock(EXCLUSIVE_LOCK, &classAccess);
//      break;
//    }
//  }
//}
//
///* To be called on JVM unload.*/
//void freeAllClasses() {
//  int j = 0;
//  int i = 0;
//  for(i = 0; i < maxClassIdx; i ++) {
//    for(j = 0; classes[i] != NULL && j < classes[i]->num_methods; j++) {
//      free(classes[i]->methods[j].methodName);
//      free(classes[i]->methods[j].methodSignature);
//    }
//    free(classes[i]->methods);
//    free(classes[i]->className);
//    free(classes[i]);
//  }
//  free(classes);
//}
//#endif
