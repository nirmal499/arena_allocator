# Arena Allocator For Lua

A fun project for trying out Embedded Lua in C++.

We will be providing Lua a specified memory pool. From that pool we will allocate memory as per needed by Lua using our global allocator.

Our global allocator will always allocate 8 bytes aligned memory to Lua

### To compile:

`$ ./run.sh 1`

### To build:

`$ ./run.sh 2`

### To run:

`$ ./run.sh 3`


#### A lot of changes will happen in the project in the future