Usage
=====

Encoding and decoding
---------------------

.. code-block:: matlab

   % Encode a MATLAB struct to a TOON string
   s = ctoon_encode(struct('name', 'Alice', 'age', uint64(30), 'active', true));

   % Decode back
   v = ctoon_decode(s);
   v.name    % → 'Alice'
   v.age     % → uint64(30)
   v.active  % → true

File I/O
--------

.. code-block:: matlab

   % Write
   cfg.host = 'localhost';
   cfg.port = uint64(8080);
   ctoon_write(cfg, 'config.toon');

   % Read
   cfg = ctoon_read('config.toon');

Type mapping
------------

.. list-table::
   :header-rows: 1
   :widths: 40 40

   * - MATLAB type
     - TOON type
   * - ``[]`` (empty double)
     - ``null``
   * - ``logical`` scalar
     - ``bool``
   * - ``double`` scalar
     - ``real``
   * - ``int64`` scalar
     - signed integer
   * - ``uint64`` scalar
     - unsigned integer
   * - ``char`` / ``string``
     - ``str``
   * - ``cell`` array
     - array
   * - ``struct`` (scalar)
     - object

.. note::

   Integer values encoded as ``uint`` or ``sint`` in TOON are decoded back as
   ``uint64`` or ``int64`` in MATLAB. Use ``double()`` to convert if needed.

Error handling
--------------

All functions throw MATLAB errors with structured identifiers:

.. code-block:: matlab

   try
       v = ctoon_decode('bad input');
   catch e
       e.identifier   % → 'ctoon:decodeError'
       e.message
   end

.. list-table::
   :header-rows: 1
   :widths: 40 50

   * - Identifier
     - Cause
   * - ``ctoon:decodeError``
     - Invalid TOON string
   * - ``ctoon:readError``
     - File not found or parse error
   * - ``ctoon:writeError``
     - Cannot write to file
   * - ``ctoon:badArg``
     - Wrong argument type
