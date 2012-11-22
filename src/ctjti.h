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

#if defined JVMTI_TYPE
#include <jni.h>
#include <jvmti.h>
#include <time.h>
#include "ctjni.h"

typedef jmethodID methodIdType;
typedef jclass classIdType;
typedef jthread threadIdType;
typedef jrawMonitorID monitorType;
#define OPTION_SEP "-"
monitorType monitor_lock;

static jvmtiEnv *g_jvmti_env;

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

monitorType createMonitor(const char *name) {
	g_jvmti_env->CreateRawMonitor(name, &monitor_lock);
	return monitor_lock;
}

void getMonitor(monitorType monitor) {
	g_jvmti_env->RawMonitorEnter(monitor);
}

void releaseMonitor(monitorType monitor) {
	g_jvmti_env->RawMonitorExit(monitor);
}

void destroyMonitor(monitorType monitor) {
	g_jvmti_env->DestroyRawMonitor(monitor);
}

void delay(int i) {
	i *= 100;
	for(; i > 0; i --) {
		__asm ("pause");
	}
}

int passFilter(const char * input);
char * getClassName(jclass klass);
char * getMethodName(methodIdType methodId);
char * getMethodSignature(methodIdType methodId);

classIdType getMethodClass(methodIdType methodId) {
	jclass declaring_class_ptr;
	g_jvmti_env->GetMethodDeclaringClass(methodId, &declaring_class_ptr);
	if(passFilter(getClassName(declaring_class_ptr)) == 0) {
		declaring_class_ptr = NULL;
	}
	return declaring_class_ptr;
}

bool isSameThread(JNIEnv* jni_env, threadIdType threadId1, threadIdType threadId2) {
	return jni_env->IsSameObject(threadId1, threadId2);
}

bool isSameClass(JNIEnv* jni_env, classIdType classId1, classIdType classId2) {
	return jni_env->IsSameObject(classId1, classId2);
}

threadIdType getThreadRef(JNIEnv* jni_env, threadIdType threadId) {
	return (threadIdType) jni_env->NewWeakGlobalRef(threadId);
}

classIdType getClassRef(JNIEnv* jni_env, classIdType classId) {
	return (classIdType) jni_env->NewWeakGlobalRef(classId);
}

#include "ctcom.h"

char * getClassName(jclass klass) {
	char *className[100];
	char *gclassName[100];
	char *tmp;
	g_jvmti_env->GetClassSignature(klass, className, gclassName);
	tmp = strdup(*className);
	g_jvmti_env->Deallocate((unsigned char*)*className);
	g_jvmti_env->Deallocate((unsigned char*)*gclassName);
	return tmp;
}

char * getMethodName(methodIdType methodId) {
	char *methodName[100];
	char *methodSignature[500];
	char *gmethodSignature[500];
	char *tmp;
	g_jvmti_env->GetMethodName(methodId, methodName, methodSignature, gmethodSignature);
	tmp = strdup(*methodName);
	g_jvmti_env->Deallocate((unsigned char*)*methodName);
	g_jvmti_env->Deallocate((unsigned char*)*methodSignature);
	g_jvmti_env->Deallocate((unsigned char*)*gmethodSignature);
	return tmp;
}

char * getMethodSignature(methodIdType methodId) {
	char *methodName[100];
	char *methodSignature[500];
	char *gmethodSignature[500];
	char *tmp;
	g_jvmti_env->GetMethodName(methodId, methodName, methodSignature, gmethodSignature);
	tmp = strdup(*methodSignature);
	g_jvmti_env->Deallocate((unsigned char*)*methodName);
	g_jvmti_env->Deallocate((unsigned char*)*methodSignature);
	g_jvmti_env->Deallocate((unsigned char*)*gmethodSignature);
	return tmp;
}

void JNICALL vmDeath(jvmtiEnv* jvmti_env, JNIEnv* jni_env) {
	printFullTrace(jni_env);
	releaseFullTrace(jni_env);
	destroyMonitor(monitor_lock);
	clearAllFilters();
}

void JNICALL threadStart(jvmtiEnv *jvmti_env, JNIEnv* jni_env, jthread thread) {
}

void JNICALL threadEnd(jvmtiEnv* jvmti_env, JNIEnv* jni_env, jthread thread) {
}

void JNICALL methodEntry(jvmtiEnv* jvmti_env, JNIEnv* jni_env, jthread thread, jmethodID method) {
	newMethodCall(method, thread, jni_env);
}

void JNICALL methodExit(jvmtiEnv* jvmti_env, JNIEnv* jni_env, jthread thread, jmethodID method, jboolean was_popped_by_exception, jvalue return_value) {
	endCall(method, thread, jni_env);
}

JNIEXPORT void JNICALL Java_com_calltracer_jni_CallTracerJNI_start (JNIEnv *jni_env, jobject obj) {
	g_jvmti_env->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_DEATH, NULL);
	g_jvmti_env->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_THREAD_START, NULL);
	g_jvmti_env->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_THREAD_END, NULL);
	g_jvmti_env->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_METHOD_ENTRY, NULL);
	g_jvmti_env->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_METHOD_EXIT, NULL);
}

JNIEXPORT void JNICALL Java_com_calltracer_jni_CallTracerJNI_stop (JNIEnv *jni_env, jobject obj) {
	g_jvmti_env->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_THREAD_START, NULL);
	g_jvmti_env->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_THREAD_END, NULL);
	g_jvmti_env->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_METHOD_ENTRY, NULL);
	g_jvmti_env->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_METHOD_EXIT, NULL);
}

JNIEXPORT jstring JNICALL Java_com_calltracer_jni_CallTracerJNI_printTrace (JNIEnv *jni_env, jobject obj) {
	printFullTrace(jni_env);
	return jni_env->NewStringUTF(traceFile);
}

JNIEXPORT void JNICALL Java_com_calltracer_jni_CallTracerJNI_flush (JNIEnv *jni_env, jobject obj) {
	releaseFullTrace(jni_env);
}

#endif
