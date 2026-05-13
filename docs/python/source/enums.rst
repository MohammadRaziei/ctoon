Enums
=====

CToon exposes three flag/enum types that control encoding and decoding behaviour.

ReadFlag
--------

Flags that control how TOON input is parsed.

.. autoclass:: ctoon.ReadFlag
   :members:
   :undoc-members:

WriteFlag
---------

Flags that control how TOON output is serialised.

.. autoclass:: ctoon.WriteFlag
   :members:
   :undoc-members:

Delimiter
---------

Controls the field delimiter character used when encoding lists.

.. autoclass:: ctoon.Delimiter
   :members:
   :undoc-members:

Usage
-----

.. code-block:: python

   import ctoon
   from ctoon import WriteFlag, Delimiter

   toon = ctoon.dumps(data,
       indent=4,
       delimiter=Delimiter.PIPE,
       flags=WriteFlag.LENGTH_MARKER | WriteFlag.NEWLINE_AT_END,
   )
