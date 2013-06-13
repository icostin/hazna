hazna
=====

This project aims to design and implement a virtual execution environment
that allows fairly fast execution as interpreted code and it makes it easy
to generate just-in-time compiled native code.

The environment is a fusion of concepts from register-based virtual machines
and operating system services.

The whole library depends on a single freestanding library - c41. This makes
it very easy to use the project under any platform as user/kernel module.


World
-----

The library works with instances called 'worlds'. These worlds are completely
isolated, there are no resources shared among them. A typical program would
likely use a single world.

A world consists mainly of code modules, tasks and a set of large buffers
divided in pages.

Execution happens in tasks and the code is separate from data.
It is possible to have a mechanism to load a module from raw data.

Module
------
A module is a collection of procedures plus some optional constant data.

Procedures are divided in instruction blocks, which are executed entirely
before transferring the execution to another block in that procedure, unless
some exception occurs. Procedures also specify the size of local variables
(also called registers).

Instruction blocks contain:
* a list of instructions
* a list of possible 'targets' (other blocks from the same procedure)
* an 'exception target' which receives execution control if there is some
    exception

Instructions have fixed length and they are divided in 4 16-bit words.
First word is the opcode and the other 3 are arguments.

Task
----
The task is the basic execution environment and is roughly equivalent to
single-threaded processes from standard OS'es.

Tasks contain:
* register/locals space
* call stack
* fast linear memory divided in pages
* a table of modules loaded in the world

Registers
---------
The tasks's register space is simply an array of bits accessible directly
through hardcoded offsets in instructions.

Registers have sizes between 1 and 128 bits that are powers of 2:
    1, 2, 4, 8, 16, 32, 64, 128

The offset of a register must be aligned to the size of that register.
For instance, a 2-bit register can be accessed at even offsets.
One can access register data of different sizes at the same offset (as long
as it follows the alignment rule). For instance, writing two 8-bit values
at offsets 2 and 3 can be read as a single 16-bit value at offset 2.
Of course, the result depends on the 'endianess' of the world. By default,
worlds are created with the endianess of the host platform to enhance
performance, but it is possible to simulate other endian conventions.

When a procedure is entered, enough room in the register space is allocated,
according to the size declared by that procedure.

When a procedure is calling another procedure it must specify an offset into
its register space which will become offset 0 in the called procedure.
This allows passing arguments and return values between them.

Call stack
----------
The call stack is an array of code locations. These locations are tuples of
indexes: (module, proc, block, insn).
The call stack is shielded from direct access, it is only modified through
call, return and exception handling.


Linear memory
-------------
Each task has access to one set of pages organised linearly. The whole size is
always a power of 2 and the task can modify at run-time that capacity.
The same physical page can be mapped at multiple addresses in the same space or
even in different task memory spaces - which creates a way of transferring data
in between tasks.

The space always has all addresses pointing to some page, so there is no way of
accessing missing pages (and getting page faults). This design is to ensure
fast execution of reads/writes without having branches in the interpreter.

Licence
===
Licence: ISC (Internet Systems Consortium)

ISC licence is equivalent to Simplified BSD Licence but with simpler wording.

Dependencies
===
* c41: Common C Code Collection
    * freestanding library (no libs imported, not even libc if that is not available)
* hbs1: Host Basic Services
    * implements interfaces defined in c41, such as memory allocators or 
        multithreading support
    * this is the only place where platform-specific functions are used

Build and install
===
I recommend adding these lines to your shell's login profile config file
(.profile for sh/bash or .zprofile for zsh):

    export PATH="$HOME/.local/bin:$PATH"
    export CPATH="$HOME/.local/include"
    export LD_LIBRARY_PATH="$HOME/.local/lib"
    export LIBRARY_PATH="$HOME/.local/lib"

This will enable you to build and install by running:

    make install

You can run the test program with:
  
    make test
    
or

    make install
    hazna test

