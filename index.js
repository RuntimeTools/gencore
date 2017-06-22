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

// Main module entry point for gencore
'use strict';

const gencore = require('./gencore');
const fs = require('fs');
const path = require('path');
const fstream = require('fstream');
const exec = require('child_process').exec;

let seq = 0;

exports.collectCore = collectCore;
exports.createCore = createCore;

/* Fork and create the core dump then callback is called
 * with the path of the core file created so it can be
 * opened on the current machine with a standard debugger
 * such as gdb or lldb.
 */
function createCore(callback) {
  if (process.platform == 'win32') {
    callback(new Error('Function not supported on Windows.'));
    return;
  }

  // Create a directory for the child process to crash in.
  // Use the timestamp to create a (hopefully) unique namespace
  // that allows the core to be related back to which process
  // crashed and when.
  const timestamp = generateTimestamp();

  // Run synchronously until fork() returns.
  const work_dir = `core_${timestamp}`;
  fs.mkdirSync(work_dir);

  let result = null;
  try {
    result = gencore.forkCore(work_dir);
  } catch (err) {
    setImmediate(callback, err);
    return;
  }
  result.work_dir = work_dir;
  setImmediate(waitForCore, result, callback);
}

function waitForCore(result, callback) {
  try {
    if(!gencore.checkChild(result.child_pid)) {
      // Check on the next tick round the event loop.
      setImmediate(waitForCore, result, callback);
      return;
    }
  } catch (err) {
    setImmediate(callback, err);
    return;
  }
  //console.error('Core file created!');
  const work_dir = result.work_dir;
  const pid = result.child_pid;

  let core_name = findCore(work_dir, pid);
  if( core_name !== undefined ) {
    callback(null, core_name);
  } else {
    callback(new Error('No core file created.'));
  }
}

/* Fork and create the core dump then collect the libraries
 * that were in use at the same time and package the core
 * and the libraries in a tar.gz file for analysis on another
 * box.
 * The uncompressed core is deleted.
 * callback is called with the name of the tar.gz file created.
 */

function collectCore(callback) {
  if (process.platform == 'win32') {
    callback(new Error('Function not supported on Windows.'));
    return;
  }
  // Up until we fork the child process this funciton needs to
  // work synchronously to minimise how much the state of the
  // process changes between requesting a core and the core
  // being created.

  // Create a directory for the child process to crash in.
  // Use the timestamp to create a (hopefully) unique namespace
  // that allows the core to be related back to which process
  // crashed and when.
  const timestamp = generateTimestamp();

  // Run synchronously until fork() returns.
  const work_dir = `core_${timestamp}`;
  fs.mkdirSync(work_dir);

  // Gather the library list before we allow async work
  // that might change the list to run.
  const libraries = gencore.findLibraries();

  let result = null;
  try {
    result = gencore.forkCore(work_dir);
  } catch (err) {
    setImmediate(callback, err);
    return;
  }

  // Now we can let other things run asyncrhonously!
  result.libraries = libraries;
  result.work_dir = work_dir;
  setImmediate(waitForCoreAndCollect, result, callback);
}

function waitForCoreAndCollect(result, callback) {
  try {
    if(!gencore.checkChild(result.child_pid)) {
      // Check on the next tick round the event loop.
      setImmediate(waitForCoreAndCollect, result, callback);
      // Return so we don't need to indent the rest of
      // this function in an else.
      return;
    }
  } catch (err) {
    setImmediate(callback, err);
    return;
  }

  // Zip up the core file and libraries.
  // The core file should have been created or
  // copied to the work_dir.
  const work_dir = result.work_dir;
  const pid = result.child_pid;
  let file_count = result.libraries.length;

  // We will only return the last error if multiple files failed to copy.
  let copy_err = null;

  // Declare triggerZip here to give shared access
  // to lib_count.
  function triggerZip(error) {
    // No need for locking to update libCount,
    // only happens on the main node loop.
    file_count--;
    if( error ) {
      copy_err = error;
    }
    if( file_count == 0 ) {
      if( copy_err == null ) {
        tarGzDir(work_dir, result, callback);
      } else {
        callback(copy_err);
      }
    }
  }

  let core_name = findCore(work_dir, pid);
  if( core_name === undefined ) {
    callback(new Error('Unable to locate core file'));
    return;
  } else if (!path.dirname(core_name).endsWith(work_dir)) {
    // Mac OS X puts cores in /cores/core.<pid> by default so
    // we have an extra file to copy into our tar.gz.
    file_count++;
    copyFile(core_name, `${work_dir}/core.${result.child_pid}`,
      triggerZip);
  }

  for( let library of result.libraries ) {
    let dest;
    // Make all the paths relative to make it less likely to overwrite
    // system libraries when the tar is extracted.
    if( library.startsWith('/') ) {
      dest = work_dir + '/' + library.substr(1);
    } else {
      dest = work_dir + '/';
    }

    // console.log(library + " -> " + dest);
    let library_writer = fstream.Writer(dest);
    let library_reader = fstream.Reader({ path: library, follow: true });
    // Now copy the data, don't let failed copies
    // stop us creating the zip.
    library_reader.pipe(library_writer)
      .on('close', (e) => triggerZip(e))
      .on('error', (e) => triggerZip(e));
  }
}

function copyFile(source, dest, closeCb) {
  const read = fs.createReadStream(source);
  const write = fs.createWriteStream(dest);

  function error_func(e) {
    read.close();
    write.close();
    closeCb(e);
  }

  read.on('error', error_func);
  write.on('error', error_func);

  read.pipe(write).on('close', closeCb);
}

function tarGzDir(work_dir, result, callback) {
  let tar_file = `${work_dir}.tar.gz`;

  // Use ls to obtain a list of files in work dir so the
  // resulting paths don't start with "./"
  exec(`tar -czf ${tar_file} ${work_dir}`,
    (error, stdout, stderr) => {
      exec(`rm -r ${work_dir}`);
      callback(error, tar_file);
    }
  );
}

function findCore(work_dir, pid) {
  let possible_names = [];
  if( process.platform == 'darwin') {
    // Mac OS X puts cores in /cores/core.<pid> by default so
    // we have an extra file to copy into our zip.
    // TODO - To be totally correct we should use:
    // sysctlbyname("kern.corefile", in C to find the exact
    // location.
    possible_names.push(path.resolve(`${work_dir}/core.${pid}`));
    possible_names.push(path.resolve(`/cores/core.${pid}`));
  } else if (process.platform == 'linux') {
    // Check the likely locations for a linux core dump.
    possible_names.push(path.resolve(`${work_dir}/core.${pid}`));
    possible_names.push(path.resolve(`${work_dir}/core`));
  }
  for(let name of possible_names) {
    if( fs.existsSync(name) ) {
      return name;
    }
  }
  return undefined;
}

function generateTimestamp() {

  const now = new Date();
  function pad(n, len) {
    if( len === undefined ) {
      len = 2;
    }
    let str = `${n}`;
    while(str.length < len) {
      str = '0' + str;
    }
    return str;
  }

  // Create a time stamp that include the process id and a sequence number
  // to make the core identifiable and unique.
  const timestamp = `${pad(now.getFullYear())}${pad(now.getMonth()+1)}` +
    `${pad(now.getDate())}.${pad(now.getHours())}${pad(now.getMinutes())}` +
    `${pad(now.getSeconds())}.${process.pid}.${pad(++seq,3)}`;

  return timestamp;
}
