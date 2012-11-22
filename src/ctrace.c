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

#define _CRT_SECURE_NO_DEPRECATE

#if defined JVMTI_TYPE

#include "ctjti.h"
JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM *vm, char *options, void *reserved) {
  jint rc;
  jvmtiCapabilities capabilities;
  jvmtiEventCallbacks callbacks;
  jvmtiEnv *jvmti;

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

#endif
