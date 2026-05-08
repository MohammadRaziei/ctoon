CToon Python API
================

**CToon** is a high-performance C serialisation library with Python bindings
built via nanobind.

Quick Example
-------------

.. code-block:: python

   import ctoon

   data = {"name": "Alice", "age": 30, "tags": ["developer", "C++"]}

   # TOON
   toon_str = ctoon.dumps(data)
   data      = ctoon.loads(toon_str)

   # JSON interop
   json_str  = ctoon.dumps_json(data, indent=2)
   data      = ctoon.loads_json(json_str)

   # File I/O
   ctoon.dump(data, "out.toon")
   data = ctoon.load("out.toon")

TOON Functions
--------------

.. autofunction:: ctoon.loads
.. autofunction:: ctoon.dumps
.. autofunction:: ctoon.load
.. autofunction:: ctoon.dump

JSON Functions
--------------

.. autofunction:: ctoon.loads_json
.. autofunction:: ctoon.dumps_json
.. autofunction:: ctoon.load_json
.. autofunction:: ctoon.dump_json

Aliases
-------

.. autofunction:: ctoon.encode
.. autofunction:: ctoon.decode

Enums
-----

.. autoclass:: ctoon.ReadFlag
   :members:
   :undoc-members:

.. autoclass:: ctoon.WriteFlag
   :members:
   :undoc-members:

.. autoclass:: ctoon.Delimiter
   :members:
   :undoc-members:

CLI
---

.. code-block:: bash

   ctoon input.json -o output.toon
   python -m ctoon.manager --version