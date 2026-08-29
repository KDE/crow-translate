<!-- SPDX-FileCopyrightText: none -->
<!-- SPDX-License-Identifier: CC0-1.0 -->

# Backend templates

One working Crow Translate backend per family, each a complete CMake project
that knows nothing about where Crow's sources are. Copy one, change the id in
its `hello.json` and its class names, and replace the body.

| Directory | Family | What it does |
| --- | --- | --- |
| `translation-hello` | translation | Hands the text back behind a configurable prefix. Carries the commentary; read this one first. |
| `tts-hello` | tts | Writes what it was asked to say into a text file. |
| `ocr-hello` | ocr | Reports the size of the image instead of reading it. |

```sh
cmake -S translation-hello -B build \
      -DCMAKE_PREFIX_PATH=<crow prefix> -DCMAKE_INSTALL_PREFIX=<crow prefix>
cmake --build build && cmake --install build
crow --tp hello -s en -t en "text"      # -> "[hello] text"
```

All three share the id `hello`, which is legal - ids are unique within a
family, not across them - and is the first thing to change in a copy: the id
goes into the user's settings file, so renaming one later deselects the backend
for everyone who had chosen it.

[`../../PLUGINS.md`](../../PLUGINS.md) is the reference. Crow's own test suite
builds and runs all three against an installed prefix; see
`BUILD_OUT_OF_TREE_PLUGIN_TEST`.
