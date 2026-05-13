JSON Functions
==============

Functions for interoperating between TOON and JSON formats.

.. code-block:: python

   import ctoon

   # String I/O
   json_str = ctoon.dumps_json(data, indent=2)
   data      = ctoon.loads_json(json_str)

   # File I/O
   ctoon.dump_json(data, "out.json")
   data = ctoon.load_json("out.json")

Reference
---------

.. autofunction:: ctoon.loads_json
.. autofunction:: ctoon.dumps_json
.. autofunction:: ctoon.load_json
.. autofunction:: ctoon.dump_json
