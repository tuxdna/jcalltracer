#ifndef JCALLTRACER_H
#define JCALLTRACER_H

#include <jni.h>
#include <jvmti.h>
#include <time.h>

#if !defined MAX_THREADS
#define MAX_THREADS 1000
#endif

#define OPT_VAL_SEPARATOR "="
#define OPT_VAL_SEPARATOR_CHAR '='

#define OPTIONS_SEPARATOR ","
#define OPTIONS_SEPARATOR_CHAR ','

enum LOCK_TYPE {SHARED_LOCK=1, EXCLUSIVE_LOCK=2};

typedef int LOCK_OBJECT;

typedef struct CTD {
  char *methodName;
  char *methodSignature;
  char *className;
  struct CTD *calledFrom;
  struct CTD **called;
  int offset;
  int callIdx;
} CTD;

typedef unsigned int KeyType;

typedef struct threadEntries {
  int size;
  KeyType threads[MAX_THREADS];
} ThreadEntriesType;

void setup(char* options);

// process options
int passFilter(const char * input);
void clearFilter(char *filter) ;
void clearAllFilters() ;

int getLock(LOCK_TYPE lock, LOCK_OBJECT *lockObj) ;
int releaseLock(LOCK_TYPE lock, LOCK_OBJECT *lockObj) ;
jrawMonitorID createMonitor(const char *name);
void getMonitor(jrawMonitorID monitor);
void releaseMonitor(jrawMonitorID monitor);
void destroyMonitor(jrawMonitorID monitor);

void printFullThreadTrace(jthread threadId, FILE *out, JNIEnv* jni_env);
int assignThreadIdx(jthread threadId, JNIEnv* jni_env);

void JNICALL vmDeath(jvmtiEnv* jvmti_env, JNIEnv* jni_env);
void JNICALL threadStart(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread);
void JNICALL threadEnd(jvmtiEnv* jvmti_env, JNIEnv* jni_env, jthread thread);
void JNICALL methodEntry(jvmtiEnv* jvmti_env, JNIEnv* jni_env, jthread thread, jmethodID method);
void JNICALL methodExit(jvmtiEnv* jvmti_env, JNIEnv* jni_env, jthread thread,
			jmethodID method, jboolean exception, jvalue retval);

#ifdef _WIN32

#include <windows.h>
void sleep(unsigned milliseconds)
{
  Sleep(milliseconds);
}

#else
#include <unistd.h>
#endif

#endif
