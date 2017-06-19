/*******************************************************************************
 * Copyright 2017 IBM Corp.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ******************************************************************************/

#include "gencore.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <nan.h>
#ifndef _WIN32
#include <dirent.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif

namespace gencore {

using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::Number;
using v8::Object;
using v8::String;

// Functions that are used by more than one platform.
#ifndef _WIN32
// This function raises the ulimit to it's maximium value.
static void raiseUlimit(int resource) {
  struct rlimit rlim;
  rlim.rlim_cur = 0;
  rlim.rlim_max = RLIM_INFINITY;
  int rc = getrlimit(resource, &rlim);
  // Guard against RLIM_INFINITY < 0
  // (RLIM_INFINITY is positive on my system but that might not always be true.)
  if (rc == 0 &&
      (rlim.rlim_cur < rlim.rlim_max || rlim.rlim_max == RLIM_INFINITY)) {
    rlim.rlim_cur = rlim.rlim_max;
    rc = setrlimit(resource, &rlim);
  }
}

// Raise the ulimits for core size (obviously) and file size
// which might also block core dumps.
void raiseUlimits() {
  raiseUlimit(RLIMIT_CORE);
  raiseUlimit(RLIMIT_FSIZE);
}
#endif

#ifdef _WIN32
// Dummy functions for Windows build
NAN_METHOD(ForkCore) {}
NAN_METHOD(CheckChild) {}
NAN_METHOD(FindLibraries) {}
#else
// This function forks to create a core file and
// returns the directory we *expect* the core
// to be created in.
// (A number of things may intervene to dash our
// expectations. We check later on!)
NAN_METHOD(ForkCore) {
  // We only expect to get passed a simple temp dir name.
  char directory_path[32];

  if (info[0]->IsString()) {
    // Filename parameter supplied
    Nan::Utf8String directory_parameter(info[0]);
    if (directory_parameter.length() <
        static_cast<int>(sizeof(directory_path))) {
      snprintf(directory_path, sizeof(directory_path), "%s",
               *directory_parameter);
    } else {
      Nan::ThrowError("ForkCore: working directory path too long.");
    }
  } else {
    Nan::ThrowError("ForkCore: no working directory specified.");
  }

  // Fork
  pid_t child_pid = fork();
  if (child_pid == 0) {
    childFunction(directory_path);
  } else if (child_pid == -1) {
    Nan::ThrowError("ForkCore: unable to create child process.");
  } else {
    // If parent:
    // Return the child pid so this process can wait on it.
    Isolate *isolate = info.GetIsolate();
    Local<Object> result = Object::New(isolate);
    Local<String> child_pid_name = String::NewFromUtf8(isolate, "child_pid");
    result->Set(child_pid_name, Number::New(isolate, child_pid));
    info.GetReturnValue().Set(result);
  }
}

// This just returns true if the pid has exitted,
// false if it still running.
NAN_METHOD(CheckChild) {
  int wstatus = 0;

  pid_t child_pid = 0;

  if (info[0]->IsNumber()) {
    child_pid = Integer::Cast(*info[0])->Value();
  }
  // std::cout << "Checking for pid: " << child_pid << "\n";

  pid_t wait_pid = waitpid(child_pid, &wstatus, WNOHANG);
  // wait_pid will be 0 if the child has not exited.
  if (wait_pid == child_pid) {
    info.GetReturnValue().Set(true);
  } else if (wait_pid == -1) {
    Nan::ThrowError("CheckChild: child pid not found.");
  } else {
    info.GetReturnValue().Set(false);
  }
}
#endif

/*******************************************************************************
 * Native module initializer function, called when the module is require'd
 *
 ******************************************************************************/
void Initialize(v8::Local<v8::Object> exports) {
  exports->Set(Nan::New("findLibraries").ToLocalChecked(),
               Nan::New<v8::FunctionTemplate>(FindLibraries)->GetFunction());
  exports->Set(Nan::New("checkChild").ToLocalChecked(),
               Nan::New<v8::FunctionTemplate>(CheckChild)->GetFunction());
  exports->Set(Nan::New("forkCore").ToLocalChecked(),
               Nan::New<v8::FunctionTemplate>(ForkCore)->GetFunction());
}

NODE_MODULE(gencore, Initialize)
}  // namespace gencore
