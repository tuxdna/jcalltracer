#ifndef JCALLTRACER_H
#define JCALLTRACER_H

#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <jni.h>
#include <jvmti.h>
#include <time.h>

#if !defined MAX_THREADS
#define MAX_THREADS 1000
#endif

#define OPTION_SEP "-"

typedef jmethodID methodIdType;
typedef jclass classIdType;
typedef jthread threadIdType;
typedef jrawMonitorID monitorType;
typedef int LOCK_TYPE;
typedef int LOCK_OBJECT;

typedef struct callTraceDef {
  char *methodName;
  char *methodSignature;
  char *className;
  struct callTraceDef *calledFrom;
  struct callTraceDef **called;
  int offset;
  int callIdx;
} callTraceDef;

typedef struct mapDef {
  char* name;
  char* value;
} mapDef;

typedef struct methodType {
    char *method_name;
    char *method_signature;
    int start_lineno;
    int end_lineno;
    jmethodID method_id;
} methodType;

typedef struct threadType {
	int threadId;
} threadType;

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

monitorType monitor_lock;
mapDef optionsMap[100];

char* translateFilter(char* filter) ;
char* translateFilter1(char* filter) ;
char* translateFilter2(char* filter) ;

/*To be called on JVM load.*/
void setup(char* options) ;
void clearFilter(char *filter) ;
void clearAllFilters() ;
int getLock(LOCK_TYPE lock, LOCK_OBJECT *lockObj) ;
int releaseLock(LOCK_TYPE lock, LOCK_OBJECT *lockObj) ;
callTraceDef *newCallTrace();
threadIdType getThreadId(int threadIdx) ;

/*Method to get the index of the current thread.*/
int getThreadIdx(threadIdType threadId, JNIEnv* jni_env);

/*To be called before calling setCall/endCall, and only if it returns 1 we should call setCall/endCall.*/
int passFilter(const char * input);
void releaseCallTrace(callTraceDef* headNode);
void releaseFullThreadTrace(threadIdType threadId, JNIEnv* jni_env);

/* To be called from a JNI method or on JVM shut down.*/
void releaseFullTrace(JNIEnv* jni_env);
void printFullThreadTrace(threadIdType threadId, FILE *out, JNIEnv* jni_env);
int assignThreadIdx(threadIdType threadId, JNIEnv* jni_env);
callTraceDef *setCall(char* methodName, char* methodSignature, char* className, callTraceDef* calledFrom, callTraceDef* call, int threadIdx);

/*To be called when any method of a thread returns.*/
callTraceDef *endCall(methodIdType methodId, threadIdType threadId, JNIEnv* jni_env);

/*To be called when a new method is invoked in a flow of a thread.*/
callTraceDef *newMethodCall(methodIdType methodId, threadIdType threadId, JNIEnv* jni_env);
void printCallTrace(callTraceDef* headNode, int depth, FILE *out);
void printFullThreadTrace(threadIdType threadId, FILE *out, JNIEnv* jni_env);

/*This method will print the trace into the traceFile. To be called from a JNI method.*/
void printFullTrace(JNIEnv* jni_env);

monitorType createMonitor(const char *name);
void getMonitor(monitorType monitor);
void releaseMonitor(monitorType monitor);
void destroyMonitor(monitorType monitor);
void delay(int i);
int passFilter(const char * input);
classIdType getMethodClass(methodIdType methodId);
bool isSameThread(JNIEnv* jni_env, threadIdType threadId1, threadIdType threadId2);
bool isSameClass(JNIEnv* jni_env, classIdType classId1, classIdType classId2);
threadIdType getThreadRef(JNIEnv* jni_env, threadIdType threadId);
classIdType getClassRef(JNIEnv* jni_env, classIdType classId);
char * getClassName(jclass klass);
char * getMethodName(methodIdType methodId);
char * getMethodSignature(methodIdType methodId);
int getMethodNameAndSignature(methodIdType id,
			      char** name, char** signature);
void JNICALL vmDeath(jvmtiEnv* jvmti_env, JNIEnv* jni_env) ;
void JNICALL threadStart(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread) ;
void JNICALL threadEnd(jvmtiEnv* jvmti_env, JNIEnv* jni_env, jthread thread) ;
void JNICALL methodEntry(jvmtiEnv* jvmti_env, JNIEnv* jni_env, jthread thread, jmethodID method) ;
void JNICALL methodExit(jvmtiEnv* jvmti_env, JNIEnv* jni_env, jthread thread, jmethodID method, jboolean was_popped_by_exception, jvalue return_value) ;

#endif
