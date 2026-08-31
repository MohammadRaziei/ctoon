# CToon C API {#mainpage}

**CToon** is a high-performance, zero-dependency C99 library for reading and
writing the **TOON** serialization format, with built-in **JSON** interop.
This reference documents the plain C API declared in `ctoon.h`.

## Installation

```cmake
# CMakeLists.txt
include(FetchContent)
FetchContent_Declare(
  ctoon
  GIT_REPOSITORY https://github.com/MohammadRaziei/ctoon.git
  GIT_TAG        main
)
FetchContent_MakeAvailable(ctoon)

# Link the C target
target_link_libraries(my_app PRIVATE ctoon::ctoon)
```

Requires CMake 3.19+ and a C99-compatible compiler.

## Quick example

```c
#include "ctoon.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    const char *src = "name: Alice\nage: 30";
    ctoon_doc *doc  = ctoon_read(src, strlen(src), 0);
    ctoon_val *root = ctoon_doc_get_root(doc);

    /* Access fields */
    ctoon_val *name = ctoon_obj_get(root, "name");
    printf("%s\n", ctoon_get_str(name));   /* Alice */

    /* Serialise back to TOON */
    size_t len;
    char *toon = ctoon_write(doc, &len);
    free(toon);

    /* Export as JSON (CTOON_ENABLE_JSON=1, default) */
    char *json = ctoon_doc_to_json(doc, 2,
                     CTOON_WRITE_NOFLAG, NULL, &len, NULL);
    printf("%s\n", json);
    free(json);

    ctoon_doc_free(doc);
    return 0;
}
```

See @ref ctoon.h for the full function reference — document I/O
(`ctoon_read`, `ctoon_write`), the object/array accessors
(`ctoon_obj_get`, `ctoon_arr_get`, ...), and JSON interop
(`ctoon_doc_to_json`).
