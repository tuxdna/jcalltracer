#ifndef JCALLTRACER_H
#define JCALLTRACER_H

#include <jni.h>
#include <jvmti.h>
#include <time.h>

#if !defined MAX_THREADS
#define MAX_THREADS 1000
#endif

#define OPT_VAL_SEPARATOR "-"
#define OPT_VAL_SEPARATOR_CHAR '-'

#define OPTIONS_SEPARATOR ","
#define OPTIONS_SEPARATOR_CHAR ','

typedef int LOCK_TYPE;
typedef int LOCK_OBJECT;

typedef unsigned int keyValueType;

typedef struct CTD {
  char *methodName;
  char *methodSignature;
  char *className;
  struct CTD *calledFrom;
  struct CTD **called;
  int offset;
  int callIdx;
} CTD;

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

typedef struct threadEntries {
  int currentIndex;
  keyValueType threads[MAX_THREADS];
} threadEntriesType;

void setup(char* options) ;

// process options
char* translateFilter(char* filter) ;
char* translateFilter1(char* filter) ;
char* translateFilter2(char* filter) ;
int passFilter(const char * input);
void clearFilter(char *filter) ;
void clearAllFilters() ;

int getLock(LOCK_TYPE lock, LOCK_OBJECT *lockObj) ;
int releaseLock(LOCK_TYPE lock, LOCK_OBJECT *lockObj) ;
jrawMonitorID createMonitor(const char *name);
void getMonitor(jrawMonitorID monitor);
void releaseMonitor(jrawMonitorID monitor);
void destroyMonitor(jrawMonitorID monitor);

jthread getThreadId(int threadIdx) ;
int getThreadIdx(jthread threadId, JNIEnv* jni_env);
void printFullThreadTrace(jthread threadId, FILE *out, JNIEnv* jni_env);
int assignThreadIdx(jthread threadId, JNIEnv* jni_env);

CTD *newCallTrace();
void releaseCallTrace(CTD* headNode);
void releaseFullThreadTrace(jthread threadId, JNIEnv* jni_env);
void releaseFullTrace(JNIEnv* jni_env);
CTD *setCall(char* methodName, char* methodSignature, char* className,
	     CTD* calledFrom, CTD* call, int threadIdx);
CTD *endCall(jmethodID methodId, jthread threadId, JNIEnv* jni_env);
CTD *newMethodCall(jmethodID methodId, jthread threadId, JNIEnv* jni_env);
void printCallTrace(CTD* headNode, int depth, FILE *out);
void printFullThreadTrace(jthread threadId, FILE *out, JNIEnv* jni_env);
void printFullTrace(JNIEnv* jni_env);

void delay(int i);
jclass getMethodClass(jmethodID methodId);
bool isSameThread(JNIEnv* jni_env, jthread threadId1, jthread threadId2);
bool isSameClass(JNIEnv* jni_env, jclass classId1, jclass classId2);
jthread getThreadRef(JNIEnv* jni_env, jthread threadId);
jclass getClassRef(JNIEnv* jni_env, jclass classId);
char* getClassName(jclass klass);
char* getMethodName(jmethodID methodId);
char* getMethodSignature(jmethodID methodId);
int getMethodNameAndSignature(jmethodID id, char** name, char** signature);

void JNICALL vmDeath(jvmtiEnv* jvmti_env, JNIEnv* jni_env);
void JNICALL threadStart(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread);
void JNICALL threadEnd(jvmtiEnv* jvmti_env, JNIEnv* jni_env, jthread thread);
void JNICALL methodEntry(jvmtiEnv* jvmti_env, JNIEnv* jni_env, jthread thread, jmethodID method);
void JNICALL methodExit(jvmtiEnv* jvmti_env, JNIEnv* jni_env, jthread thread,
			jmethodID method, jboolean exception, jvalue retval);

keyValueType nextKey();
void setupDataStructures();

#endif
