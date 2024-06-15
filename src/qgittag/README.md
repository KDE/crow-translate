# QLabTag

**QLabTag** is a [**fork** of library](https://github.com/crow-translate/QGitTag) for **Qt5** that uses the GitLab API to provide information about releases that can be used to check for updates. [Detailed documentation](docs/QGitTag.md "Class documentation").

Example:

```cpp
QCoreApplication app(argc, argv);
QCoreApplication::setApplicationVersion("0.9.0");
QTextStream out(stdout);

QGitTag tag(nullptr, "private-key");
tag.get("12345678")
if (tag.tagName() >= QCoreApplication::applicationVersion())
    out << "Update available: " + tag.url().toString() << endl;
```

## Installation

To include the library files I would recommend that you add it as a git submodule to your project and include it's contents with a `.pri` file. For example, if you want to clone the library in `src/third-party/` in your project, use this command to clone with **ssh**:

`git submodule add git@gitlab.com:VolkMilit/QLabTag.git src/third-party/qlabtag`

or this to clone with **https**:

`git submodule add https://gitlab.com/VolkMilit/QLabTag.git src/third-party/qlabtag`

or if you don't want to add the library as a submodule, you can download the archive from the [releases](https://gitlab.com/VolkMilit/QLabTag/-/tags) page and unpack it to the desired folder **manually**.

Then include the `qgittag.pri` file in your `.pro` project file:

`include(src/third-party/qgittag/qgittag.pri)`

**Header:**

```cpp
#include "qgittag.h"
```

**or:**

```cpp
#include <QGitTag>
```
