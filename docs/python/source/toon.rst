TOON Functions
==============

Functions for encoding and decoding the TOON serialisation format.

.. code-block:: python

   import ctoon

   # String I/O
   toon = ctoon.dumps({"name": "Alice", "age": 30})
   data = ctoon.loads(toon)

   # File I/O
   ctoon.dump(data, "out.toon")
   data = ctoon.load("out.toon")

Reference
---------

.. autofunction:: ctoon.loads
.. autofunction:: ctoon.dumps
.. autofunction:: ctoon.load
.. autofunction:: ctoon.dump

Aliases
-------

``encode`` and ``decode`` are aliases for ``dumps`` and ``loads`` respectively.

.. autofunction:: ctoon.encode
.. autofunction:: ctoon.decode
