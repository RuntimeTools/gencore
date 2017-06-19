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
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <mach-o/dyld.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/wait.h>

namespace gencore {

using v8::Array;
using v8::Isolate;
using v8::Local;
using v8::String;

void childFunction(const char *directory_path) {
  // If child:
  // Set ulimits, core_dumpfilter, raise(SIGSEGV)
  int rc = chdir(directory_path);
  if (rc != 0) {
    // Error - In child, awkward to handle. Exit without creationg a core?
  }
  raiseUlimits();

  // Tell Mac OS to create core dumps in the current directory.
  // DISABLED - sysctl setting is system wide and can only be changed
  // if running as root.
  // const char* corefile = "core.%P";
  // std::cout << "Setting kern.corefile to: " << corefile << "\n";
  // int result = sysctlbyname("kern.corefile", NULL, NULL, (void*)corefile,
  // strlen(corefile));
  // if( result != 0 ) {
  //   std::cout << "Failed to set kern.corefile, result " << result << "\n";
  // }

  // Crash with a signal that should create a core dump.
  raise(SIGSEGV);
}

NAN_METHOD(FindLibraries) {
  Isolate *isolate = Isolate::GetCurrent();
  Local<Array> library_names = Array::New(isolate);

  int index = 0;
  const char *name = _dyld_get_image_name(index);
  while (name != nullptr) {
    // std::cout << "Found library: " << name << "\n";
    Local<String> libName = String::NewFromUtf8(isolate, name);
    library_names->Set(index, libName);
    index++;
    name = _dyld_get_image_name(index);
  }

  info.GetReturnValue().Set(library_names);
}
}  // namespace gencore
