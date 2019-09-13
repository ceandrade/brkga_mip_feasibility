1. Dependencies
---------------

This software depends on the following libraries:
- Boost (developed with current version, should work with any recent version).
  We use the header-only libraries in Boost, so no real installation is necessary.
  Just unzip the Boost sources somewhere and update the makefile accordingly.
- libxml2
- Cplex 12.5

A recent C++ compiler is needed as well, with basic support for C++11.
gcc >= 4.7 and/or clang++ should do the job.

It has been tested on Linux/Mac OS, Windows is NOT supported (and most likely will NOT work!).

2. Installation
---------------

The Makefile in the root folder is responsible for setting the appropriate variables/flags.
You can modify the variables directly there, or create a Makefile.$PLATFORM or Makefile.$HOST
in the make subfolder with the appropriate definitions.

Note that:
PLATFORM	:= $(shell uname -s | tr [:upper:] [:lower:])
HOST		:= $(shell hostname | tr [:upper:] [:lower:])

After setting up the Makefile, just type make at the command prompt.

3. Usage
--------

Libraries and executables are created in a separate build folder (.obj).
A link to each executable is also created in the appropriate subfolder.

For the documentation of each executable, please check the README.txt in its subfolder.
The general pattern is however the following:
- the basic usage is:
./exec_main file
- if you want to load some configuration files (usually put in a settings subfolder):
./exec_main -c settings/file.cfg
- if you want to change a parameter from the command line:
./exec_main -C namespace.parameter=value
- all of the above can be combined

Each run usually creates a log file, to be stored in some folder (default is "./tmp/problem_name").

Have fun!

Domenico
