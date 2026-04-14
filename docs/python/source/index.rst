CToon Python API
================

**CToon** is a high-performance C++ serialization library with Python bindings
built via nanobind.

Quick Example
-------------

.. code-block:: python

   import ctoon

   data = {"name": "Ali", "age": 30, "tags": ["developer", "C++"]}

   json_str = ctoon.dumps_json(data, indent=2)
   toon_str = ctoon.encode(data)

Functions
---------

.. autofunction:: ctoon.loads_json
.. autofunction:: ctoon.dumps_json
.. autofunction:: ctoon.load_json
.. autofunction:: ctoon.dump_json
.. autofunction:: ctoon.encode
.. autofunction:: ctoon.decode
.. autofunction:: ctoon.encode_to_file
.. autofunction:: ctoon.decode_from_file
.. autofunction:: ctoon.loads_toon
.. autofunction:: ctoon.dumps_toon
.. autofunction:: ctoon.loads
.. autofunction:: ctoon.dumps

Classes
-------

.. autoclass:: ctoon.EncodeOptions
   :members:
   :undoc-members:

.. autoclass:: ctoon.DecodeOptions
   :members:
   :undoc-members:

Enums
-----

.. autoclass:: ctoon.Delimiter
   :members:
   :undoc-members:

.. autoclass:: ctoon.Type
   :members:
   :undoc-members:

CLI
---

.. code-block:: bash

   ctoon input.json -o output.toon
   python -m ctoon.manager --version
