MATLAB Binding
==============

CToon MATLAB binding provides a MEX-based interface to the CToon C library,
allowing you to encode and decode TOON-format data directly from MATLAB.

.. list-table::
   :widths: 30 70

   * - **Compatibility**
     - MATLAB R2014b or newer — no toolboxes required
   * - **Build**
     - ``ctoon_build`` (manual) or ``buildtool mex`` (R2022b+)
   * - **Functions**
     - ``ctoon_encode``, ``ctoon_decode``, ``ctoon_read``, ``ctoon_write``

Quick start
-----------

.. code-block:: matlab

   % Build and add to path (once)
   cd src/bindings/matlab
   ctoon_install

   % Encode
   s = ctoon_encode(struct('name', 'Alice', 'age', uint64(30)));

   % Decode
   v = ctoon_decode(s);

Sections
--------

- **Installation** — build requirements, CMake integration, self-contained export
- **Usage** — type mapping, error handling, file I/O examples
- **API Reference** — full docstring reference for all public functions