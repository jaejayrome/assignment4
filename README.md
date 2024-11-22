# SNU CS M1522.00800 : System Programming
## Lab 4 : A Unix Shell


#### Please note that late submission for assignment 4 is NOT allowed.


Purpose
-------

A Unix shell is a program that makes the facilities of the operating system available to interactive users. There are several popular Unix shells: `sh` (the Bourne shell), `csh` (the C shell), and `bash` (the Bourne Again shell) are a few.

The purpose of this assignment is to help you learn about Unix processes, low-level input/output, and signals. It will also give you ample opportunity to define software modules; in that sense the assignment is a capstone for the course.


Your Task
---------

Your task in this assignment is to create a program named `ish`. **If your program name isn't `ish`, you cannot get any score**. Your program should be a minimal but realistic interactive Unix shell.


Building a Program
------------------

Makefile for building a program is already prepared. What you need to do is just build a program with `make` command. The submission you submit should of course also be able to be compiled using the make command. You cannot receive 0 points if it is not compiled with 'make'

Initialization and Termination
------------------------------

Your program should print a **percent sign and a space (% )** before each such line. To facilitate your debugging and our testing, set `export DEBUG=1` before you run your ish. For clean running of your shell do `unset DEBUG`. Make sure the DEBUG environment variable is set by checking with `echo $DEBUG`.

Your program should terminate when the user types Ctrl-d or issues the `exit` command. (See also the section below entitled "Signal Handling.")


Interactive Operation
---------------------

After start-up processing, your program repeatedly should perform these actions:

*   Print a prompt, which is consisting of a percent sign followed by a space, to the standard output stream.
*   Read a line from the standard input stream.
*   Lexically analyze the line to form an array of tokens.
*   Syntactically analyze the token array.
*   Form a command line for execution.
*   Execute the command.

Lexical Analysis
----------------

Informally, a _token_ should be a word. More formally, a token should consist of a sequence of non-white-space characters that are separated from other tokens by white-space characters. There should be two exceptions:

*   The special characters '>', and '<' should form separate tokens.
*   Strings enclosed in double quotes (") or single quotes(') should form part or all of a single token. Special characters inside of strings should not form separate tokens.

Your program should assume that no line of the standard input stream contains more than 1023 characters; the terminating newline character is included in that count. In other words, your program should assume that a string composed from a line of input can fit in an array of characters of length 1024. If a line of the standard input stream is longer than 1023 characters, then your program need not handle it properly; but it should not corrupt memory.

Syntactic Analysis
------------------

A _command_ should be a sequence of tokens, the first of which specifies the command name.

Execution
---------

Your program should interpret four shell built-in commands:

`cd [_dir_]`

Your program should change its working directory to `_dir_`, or to the HOME directory if `_dir_` is omitted.

`exit`

Your program should exit with exit status 0.

Note that those built-in commands should neither read from the standard input stream nor write to the standard output stream. Your program should print an error message if there is any file redirection with those built-in commands.

If the command is not a built-in command, then your program should consider the command name to be the name of a file that contains code to be executed. Your program should fork a child process and pass the file name, along with its arguments, to the `execvp` system call. If the attempt to execute the file fails, then your program should print an error message indicating the reason for the failure.

Process Handling
----------------

All child processes forked by your program should run in the foreground

It is required to call wait for every child that has been created.

Signal Handling
---------------

**\[NOTE\] Ctrl-d represents EOF, not a signal. Do NOT make a signal handler for Ctrl-d.**

When the user types Ctrl-c, Linux sends a SIGINT signal to the parent process and its children. Upon receiving a SIGINT signal:

*   The parent process should ignore the SIGINT signal.
*   A child process should not necessarily ignore the SIGINT signal. That is, unless the child process itself (beyond the control of parent process) has installed a handler for SIGINT signals, the child process should terminate.

Redirection
-----------

You are going to implement redirection of standard input and standard output.

*   The special character '<' and '>' should form separate token in lexical analysis.
*   The '<' token should indicate that the following token is a name of a file. Your program should redirect the command's standard input to that file. It should be an error to redirect a command's standard input stream more than once.
*   The '>' token should indicate that the following token is a name of a file. Your program should redirect the command's standard output to that file. It should be an error to redirect a command's standard output stream more than once.
*   If the standard input stream is redirected to a file that does not exist, then your program should print an appropriate error message.
*   If the standard output stream is redirected to a file that does not exist, then your program should create it. If the standard output stream is redirected to a file that already exists, then your program should destroy the file's contents and rewrite the file from scratch. Your program should set the permissions of the file to 0600.

Error Handling
--------------

Your program should handle an erroneous line gracefully by rejecting the line and writing a descriptive error message to the standard error stream. An error message written by your program should begin with "`_programName_:` " where `_programName_` is `argv[0]`, that is, the name of your program's executable binary file.

**Your program should handle all user errors. It should be impossible for the user's input to cause your program to crash.**

Extra Credit (extra 30 points)
-------------------------------------------------------------------------

To earn extra credit, implement background process support by enabling processes to run in the background, adding them to a job list, and handling their termination efficiently.
You must pass both tests (test11 and test12) for the Background test to receive the 30 points extra credit.


Logistics
---------

Develop on lab machines. Use your favorite edtor to create source code. Use `make` to automate the build process. Use `gdb` to debug.

We also provide the [interface](dynarray.h) and [implementation](dynarray.c) of the `dynarray` ADT. You are welcome to use that ADT in your program.

Your `readme` file should contain:

*   Your name and the name and the student ID.
*   (Optionally) An indication of how much time you spent doing the assignment.
*   (Optionally) Your assessment of the assignment.
*   (Optionally) Any information that will help us to grade your work in the most favorable light. In particular you should describe all known bugs.

Submission
----------

Your submission should be one gzipped tar file whose name is

YourStudentID\_assign4.tar.gz

When you submit the code, submit just one copy to the submission link if you work in a team.

Your submission need to include the following files:

*   Your source code files. (If you used `dynarray` ADT, then submit the `dynarray.h` and `dynarray.c` files as well.)
*   `Makefile`. The first dependency rule should build your entire program. The `Makefile` should maintain object (.o) files to allow for partial builds, and encode the dependencies among the files that comprise your program. As always, use the `gcc800` command to build.
*   A `readme` file.

Your submission file should look like this:

202412345\_assign4.tar.gz 

    202412345\_assign4

        your\_source\_code.c (can be any name or multiple files)

        your\_header.h (can be any name or multiple files)

        Makefile

        readme



Grading
-------

If your submission file does not contain the expected files, or your code cannot be compiled  with `gcc800`, we cannot give you any points. Please double check before you submit.

We will grade your work on quality from the user's point of view and from the programmer's point of view. From the user's point of view, your program has quality if it behaves as it should. From the programmer's point of view, your program has quality if it is well styled and thereby simple to maintain. See the specifications of previous assignments for guidelines concerning style. Proper function-level and file-level modularity will be a prominent part of your grade. To encourage good coding practices, we will deduct points if `gcc800` generates warning messages.


**Names**: You should use a clear and consistent style for variable and function names. One example of such a style is to prefix each variable name with characters that indicate its type. For example, the prefix `c` might indicate that the variable is of type `char`, `i` might indicate `int`, `pc` might mean `char*`, `ui` might mean `unsigned int`, etc. But it is fine to use another style -- a style which does not include the type of a variable in its name -- as long as the result is a readable program.

**Line lengths**: Limit line lengths in your source code to 80 characters. Doing so allows us to print your work in two columns, thus saving paper.

**Comments**: Each source code file should begin with a comment that includes your name, student ID, and the description of the file.

**Comments**: Each function should begin with a comment that describes what the function does from the caller's point of view. The function comment should:

*   Explicitly refer to the function's parameters (by name) and the function's return value.
*   State what, if anything, the function reads from standard input or any other stream, and what, if anything, the function writes to standard output, standard error, or any other stream.
*   State which global variables the function uses or affects.

Please note that you might not get a full credit even if you pass the test with your `./ish`.


| Test                                      | Score |
|-------------------------------------------|-------|
| Redirection input test                    | 5     |
| Redirection output test                   | 10     |
| Finding execution file test               | 5     |
| Single pipe test                          | 10     |
| Multiple pipe test                        | 12    |
| Multiple pipe and output redirection test | 20    |
| Slow pipe test                            | 12    |
| Multiple slow pipe and output redirection test | 22 |
| Interrupt on single process test          | 2     |
| Interrupt on multiple process test        | 2     |
| **Background process tests (2 items)**    | **30** |
| **Total**                                 | **130** |