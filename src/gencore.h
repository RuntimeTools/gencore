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

#ifndef SRC_GENCORE_H_
#define SRC_GENCORE_H_

#include "nan.h"
#ifndef _WIN32
#include <sys/types.h>
#include <unistd.h>
#endif

namespace gencore {

NAN_METHOD(FindLibraries);
NAN_METHOD(CheckChild);
NAN_METHOD(ForkCore);

/* Shared functions (at least on Unix-y platforms.) */

// Setting the ulimits is void as even if we fail we are
// still going to try to create a core dump.
void raiseUlimits();

// The code to run in the child process.
// void, since this function will never return.
void childFunction(const char* working_dir);

}  // namespace gencore
#endif  // SRC_GENCORE_H_
