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

Spec version
------------

The `toon-format/spec <https://github.com/toon-format/spec>`_ release this build
targets, read from the C core (and checked against ``supported_spec.conf`` in the
test-suite).

.. code-block:: python

   import ctoon

   ctoon.__toon_spec__        # "4.1"          spec version
   ctoon.__toon_spec_tag__    # "v4.1.2"       spec git tag
   ctoon.__toon_spec_date__   # "2026-07-26"   spec release date

.. py:data:: ctoon.__toon_spec__
   :type: str

   The TOON spec version this build targets, e.g. ``"4.1"``.

.. py:data:: ctoon.__toon_spec_tag__
   :type: str

   The toon-format/spec git tag this build targets, e.g. ``"v4.1.2"``.

.. py:data:: ctoon.__toon_spec_date__
   :type: str

   The release date of the targeted spec, e.g. ``"2026-07-26"``.

Aliases
-------

``encode`` and ``decode`` are aliases for ``dumps`` and ``loads`` respectively.

.. autofunction:: ctoon.encode
.. autofunction:: ctoon.decode
