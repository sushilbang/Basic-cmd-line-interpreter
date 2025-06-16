# Simple C++ Shell (MyShell)

## Overview

Implemented a basic command-line interpreter (shell) in C++. Its primary purpose is to demonstrate fundamental operating system concepts, particularly process management and interaction on POSIX-compliant systems (like Linux and macOS).

It provides a simple interface to execute commands, manage basic background processes, handle I/O redirection, and chain commands using pipes.

**Note:** This shell is designed for POSIX environments and utilizes system calls like `fork`, `execvp`, `waitpid`, `pipe`, `dup2`, etc. It will **not** compile or run on Windows without significant modifications to use the Windows API.

## Features

* **Command Execution:** Executes external commands found in the system's `PATH`.
* **Built-in Commands:**
    * `cd <directory>`: Changes the current working directory.
    * `pwd`: Prints the current working directory.
    * `exit`: Terminates the shell.
* **Background Processes:** Run commands in the background by appending `&`.
    * Basic zombie process reaping is implemented to clean up terminated background processes.
* **Input/Output Redirection:**
    * `< filename`: Redirects standard input from a file.
    * `> filename`: Redirects standard output to a file (truncates or creates).
* **Piping:** Connects the standard output of one command to the standard input of another using `|` (currently supports a single pipe per command line).
* **Basic Prompt:** Displays a `myshell> ` prompt.

## OS Concepts Demonstrated

This project directly utilizes or illustrates the following OS internals:

* **Process Creation:** `fork()` system call to create child processes.
* **Program Loading & Execution:** `execvp()` system call to load and run new programs within a child process.
* **Process Termination & Waiting:** `waitpid()` system call to wait for child processes to complete and manage their lifecycle (including basic zombie reaping with `WNOHANG`).
* **File Descriptors:** Manipulation of standard input (`STDIN_FILENO`), standard output (`STDOUT_FILENO`), and file descriptors obtained via `open()`.
* **I/O Redirection:** Using `dup2()` to duplicate file descriptors and redirect standard streams.
* **Inter-Process Communication (IPC):** Using `pipe()` to create unnamed pipes for command chaining.
* **Working Directory Management:** Using `chdir()` and `getcwd()`.
* **System Call Error Handling:** Using `perror()` to report errors from system calls.

## Building

You need a C++ compiler that supports C++11 (for convenience features like `std::string`, `std::vector`, though the core logic uses C APIs) and a POSIX-compliant system.

```bash
g++ myshell.cpp -o myshell -std=c++11
```

## Running
After successful compilation, execute the shell from your terminal:

```bash
./myshell
