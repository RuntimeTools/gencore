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
#include <link.h>
#include <sys/resource.h>
#include <sys/wait.h>

namespace gencore {

  using v8::Array;
  using v8::Isolate;
  using v8::Local;
  using v8::String;

void childFunction(const char* directory_path) {
  // If child:
  // Set ulimits, core_dumpfilter, raise(SIGSEGV)
  int rc = chdir(directory_path);
  if (rc != 0) {
    // Error - In child, awkward to handle. Exit without creationg a core?
  }
  raiseUlimits();

  std::fstream coredump_filter;
  coredump_filter.open("/proc/self/coredump_filter", std::ios::out);
  if (coredump_filter.is_open()) {
    coredump_filter << "0xFF";
    coredump_filter.close();
  }

  // Crash with a signal that should create a core dump.
  raise(SIGSEGV);
}

typedef struct library_record {
  char *library_name;
  library_record *next;
} library_record_t;

static int LibraryRecordCallback(struct dl_phdr_info *info, size_t size,
                                void *data) {
  library_record_t **records_ptr = reinterpret_cast<library_record_t **>(data);
  library_record_t *new_record =
      reinterpret_cast<library_record_t *>(malloc(sizeof(library_record_t)));
  if (new_record == nullptr) {
    return 1;  // Abort the iteration.
  }
  if (info->dlpi_name != nullptr && *info->dlpi_name != '\0') {
    size_t name_len = strlen(info->dlpi_name) + 1;
    new_record->library_name = reinterpret_cast<char *>(malloc(name_len));
    if (new_record->library_name == nullptr) {
      return 1;
    }
    snprintf(new_record->library_name, name_len, "%s", info->dlpi_name);
  } else {
    new_record->library_name = nullptr;
  }
  new_record->next = *records_ptr;
  *records_ptr = new_record;
  return 0;
}

NAN_METHOD(FindLibraries) {
  Isolate *isolate = Isolate::GetCurrent();
  Local<Array> library_names = Array::New(isolate);
  uint32_t index = 0;
  library_record_t *record_ptr = nullptr;
  dl_iterate_phdr(LibraryRecordCallback, &record_ptr);
  bool have_exe = false;
  while (record_ptr != nullptr) {
    library_record_t *next = record_ptr->next;
    if (record_ptr->library_name != nullptr) {
      Local<String> libName =
          String::NewFromUtf8(isolate, record_ptr->library_name);
      library_names->Set(index, libName);
    } else if (!have_exe) {
      char buffer[PATH_MAX + 1];
      memset(buffer, 0, sizeof(buffer));
      ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer));
      if (len > 0) {
        Local<String> exeName = String::NewFromUtf8(isolate, buffer);
        library_names->Set(index, exeName);
        have_exe = true;
      }
    }
    free(record_ptr->library_name);
    free(record_ptr);
    record_ptr = next;
    index++;
  }

  info.GetReturnValue().Set(library_names);
}
}  // namespace gencore
