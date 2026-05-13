CToon Python API
================

**CToon** is a high-performance C serialisation library with Python bindings
built via nanobind. It provides a compact, human-readable alternative to JSON
designed to minimise LLM token usage.

.. code-block:: python

   import ctoon

   data = {"name": "Alice", "age": 30, "tags": ["developer", "C++"]}

   toon_str = ctoon.dumps(data)   # encode → TOON
   data      = ctoon.loads(toon_str)  # decode ← TOON

Installation
------------

.. code-block:: bash

   pip install ctoon

.. toctree::
   :maxdepth: 1

   Overview <self>
   toon
   json
   enums
   cli
