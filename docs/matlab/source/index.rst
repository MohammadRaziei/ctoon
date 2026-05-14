MATLAB Binding
==============

The CToon MATLAB binding provides a MEX-based interface to the CToon C library,
allowing MATLAB users to encode and decode TOON-format data natively.

Overview
--------

The binding exposes four public functions:

.. list-table::
   :header-rows: 1
   :widths: 30 60

   * - Function
     - Description
   * - ``ctoon_encode``
     - Encode a MATLAB value to a TOON string
   * - ``ctoon_decode``
     - Decode a TOON string to a MATLAB value
   * - ``ctoon_read``
     - Read a ``.toon`` file into a MATLAB value
   * - ``ctoon_write``
     - Write a MATLAB value to a ``.toon`` file

Quick start
-----------

.. code-block:: matlab

   % Install once
   cd src/bindings/matlab
   ctoon_install

   % Encode and decode
   s = ctoon_encode(struct('name', 'Alice', 'age', uint64(30)));
   v = ctoon_decode(s);

   % File I/O
   ctoon_write(v, 'config.toon');
   v = ctoon_read('config.toon');

Requirements
------------

- MATLAB R2014b or newer
- A C compiler configured for MEX (``mex -setup C``)
- MATLAB R2022b or newer for ``buildtool`` support