Installation
============

Requirements
------------

- MATLAB R2014b or newer (for MEX compilation)
- A C compiler configured for MATLAB (``mex -setup C``)
- MATLAB R2022b or newer for ``buildtool`` support

Quick install
-------------

Clone or download the CToon repository, then from MATLAB:

.. code-block:: matlab

   cd src/bindings/matlab
   ctoon_install

``ctoon_install`` compiles the MEX gateway and permanently adds the binding
to your MATLAB path via ``savepath``.

Manual build
------------

.. code-block:: matlab

   cd src/bindings/matlab
   ctoon_build
   addpath(pwd)

Build with CMake
----------------

.. code-block:: bash

   cmake -B build -DCTOON_BUILD_MATLAB=ON -DMatlab_ROOT_DIR=/path/to/matlab
   cmake --build build --target ctoon_mex

Self-contained export
---------------------

To distribute the binding without requiring a full CToon build environment:

.. code-block:: bash

   cmake \
     -DEXPORT_DIR=/tmp/ctoon-matlab \
     -DMEX_SOURCES_PATH=/path/to/ctoon/src \
     -DMEX_INCLUDE_DIR=/path/to/ctoon/include \
     -P src/bindings/matlab/CToonMatlabExport.cmake

The export directory contains everything needed — ``ctoon.c``, ``ctoon.h``,
``ctoon_mex.c``, and all ``.m`` wrappers. No CMake or git required on the
target machine.

Verify
------

.. code-block:: matlab

   ctoon_encode(struct('hello', 'world'))
