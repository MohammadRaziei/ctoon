CLI
===

The ``ctoon`` command-line tool is installed alongside the Python package
and is available directly in your shell after ``pip install ctoon``.

Synopsis
--------

.. code-block:: bash

   ctoon [OPTIONS] <input>
   ctoon -e <input>    # force encode  JSON → TOON
   ctoon -d <input>    # force decode  TOON → JSON

Examples
--------

.. code-block:: bash

   # JSON → TOON (auto-detected from extension)
   ctoon input.json
   ctoon input.json -o output.toon

   # TOON → JSON
   ctoon input.toon -o output.json --indent 4

   # Pipe from stdin
   cat data.json | ctoon -e -

Package manager
---------------

The ``ctoon.manager`` sub-module exposes the same functionality
from Python:

.. code-block:: bash

   python -m ctoon.manager --version
   python -m ctoon.manager --help
