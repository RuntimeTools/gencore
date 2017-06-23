# gencore

Creates a core dump for the current process without terminating the process or attaching a debugger.

# Background

This module is intended for use creating non-destructive core dumps of running processes in environments where they cannot be created via gcore or may be deleted as soon as they are written.

The usual technique for creating a core dump without terminating a process is to run gcore against the process. gcore invokes gdb and attaches to the process using ptrace.

gcore is a better interface for creating core dumps if ptrace is available however in many configurations its use is restricted. You can check the availability of ptrace by looking at the value of /proc/sys/kernel/yama/ptrace_scope. For more details see: https://www.kernel.org/doc/Documentation/security/Yama.txt
gcore will also cause your process to pause while it creates the core dump, the pause time will be proportional to the size of your process and how long it takes to write the core file to disk. This module offers more asynchronous behaviour and may be more suitable in cases where this causing your Node.js process to pause is unacceptable.

# How it works:

To get around the restrictions on attaching a debugger this module lets the kernel to create a core dump in the standard way for a process crashing via SIGSEGV. To avoid destroying the running process this module forks a child process and that child process raises SIGSEGV.

Steps (Linux):
- [Main process] Create a temporary working directory for the process to crash in. This is necessary as the name of the core dump created by the kernel may simply be "core". This is indistinguishable from other core dumps that may have been created.
- [Main Process] Call fork() creating the child process and use waitpid() to wait for it to terminate.
- [Child Process] Change to the temporary directory.
- [Child Process] Raise the core dump and file size ulimits to their maximum.
- [Child Process] Write 0xFF to /proc/self/coredump_filter to ensure all the processes memory is included in the dump.
- [Child Process] Call raise(SIGSEGV) to terminate the process and create a core dump.
- [Main Process] Resume as soon as the child process exists.
- [Main Process] List the files in the temporary directory and set the file name found as the return value from createCore()

The resulting core file can be analysed using standard debugging tools such as lldb or gdb. For Node.js debugging using the llnode plugin ( [npm installer](https://www.npmjs.com/package/llnode) / [github repository](https://github.com/nodejs/llnode) ) will allow you to access JavaScript objects and stacks.

# API

Note: This API is asynchronous, after the point at which the state of the process has been saved. (When fork() returns in the parent process.) It is synchronous before that, which isn't long, to prevent the process state changing between the point a core dump and is requested and the point it is created. After that point this API waits asynchronously for the child process to finish and perform any other operations asynchronously.

- `gencore.createCore(callback)`

Creates a core dump. The callback signature is (err, filename) where err is set if an error occurred and filename is the core dump that was created.

- `gencore.collectCore(callback)`

Creates a core dump and collects that and all the libraries loaded by the process into a tar.gz file. The tar.gz is named "core_" followed by a timestamp, the pid of the Node.js process and a sequence number to ensure multiple files are unique. The callback signature is (err, filename) where err is set if an error occured and filename is the file containing the core and libraries.
All the files in the tar file are under a top level directory with the same name as the tar.gz file but without the .tar.gz extension. The libraries are stored with their paths intact but relative to the top level directory of the tar file. The core dump will be stored under the top level directory of the tar.gz file.
This function is intended to support analysis taking place on a different system to the one that generated the core dump. For example using lldb and llnode on a Mac to analyse a core from your production system.

*Note:* Core files are large (approximately your processes memory size) so you should ensure the files created by these APIs are deleted when you have finished with them. Repeatedly calling this API will without deleting the files it creates consume a large amount of disk space.

# Limitations (possible bugs/enhancements):
- Because fork() only copies the calling thread into the process only the calling threads stack will be available from the core dump. This is not a major restriction on Node.js as that is the only thread that will be running JavaScript code. The Node.js callback based design makes thread stacks a slightly less useful than they are on heavily threaded languages like Java.
- On Linux the setting of /proc/sys/kernel/core_pattern allows the file name for the core dump to be anything. It can even divert the core dump data to another process which may not even write the core file to disk. This module makes no attempt to guess the name and simply relies on picking up whatever file is created in the crashing directory.


# Other platforms:

Non Linux platform support is provided to allow applications to be developed and tested without code changes.
Linux and Mac support is implemented - Mac uses a simpler version of fork and abort as there are fewer configuration issues to work around.
Support for each platform should be implemented by adding a <platform>.cc file that handles the platform specific code.

AIX support would be implemented via the gencore API: https://www.ibm.com/support/knowledgecenter/ssw_aix_72/com.ibm.aix.basetrf1/gencore.htm

Windows support would be implemented via MiniDumpWriteDump:
https://msdn.microsoft.com/en-us/library/windows/desktop/ms680360(v=vs.85).aspx

Note: The lack of an API for requesting a core dump from the kernel on Linux is what necessitates the existence of this module. If there were an API this module could be rewritten to be far simpler as could gcore.


## License

[Licensed under the Apache 2.0 License.](LICENSE.md)
