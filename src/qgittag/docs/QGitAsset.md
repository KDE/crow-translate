# QGitAsset

The **QGitAsset** class provides information about tag assets.

## Content

- [Public Functions](#public-functions)
- [Detailed Description](#detailed-description)
- [Member Functions Documentation](#member-functions-documentation)

## Public Functions

| Return type | Data member                                    |
| ----------: | :--------------------------------------------- |
|             | [**QGitAsset**(const QJsonObject &asset)](#c1) |
|     QString | [**name**()](#name)                            |
|     QString | [**contentType**()](#content-type)             |
|        QUrl | [**url**()](#url)                              |
|   QDateTime | [**createdAt**()](#created-at)                 |
|   QDateTime | [**publishedAt**()](#published-at)             |
|         int | [**id**()](#id)                                |
|         int | [**size**()](#size)                            |
|         int | [**downloadCount**()](#download-count)         |

## Detailed Description:

A **QGitAsset** object can be obtained from the [QGitTag](docs/QGitTag.md "Class documentation") object, which contains a list of assets for a particular release.

Example:

```cpp
QGitTag tag("Shatur95", "Crow-Translate");
QTextStream out(stdout);
out << "Asset: " + tag.assets().at(0).name() << endl; // Prints asset filename
out << "Type: " + tag.assets().at(0).type() << endl; // Prints asset type
out << "Download Url: " + tag.assets().at(0).url() << endl; // Prints download link
```

---

## Member Functions Documentation

### <a id='c1'/> QGitAsset::QGitAsset(_const QJsonObject &asset_)

Constructs an object from _asset_. Normally you don't need call this function at all.

---

### <a id='name'/> [QString](https://doc.qt.io/qt-5/qstring.html "Qt Documentation") QGitAsset::name()

Returns the name of the asset file.

---

### <a id='content-type'/> [QString](https://doc.qt.io/qt-5/qstring.html "Qt Documentation") QGitAsset::contentType()

Returns the asset MIME-type.

---

### <a id='url'/> [QString](https://doc.qt.io/qt-5/qstring.html "Qt Documentation") QGitAsset::url()

Returns the asset download link.

---

### <a id='created-at'/> [QDateTime](https://doc.qt.io/qt-5/qdatetime.html "Qt Documentation") QGitAsset::createdAt()

Returns the asset creation time.

---

### <a id='published-at'/> [QDateTime](https://doc.qt.io/qt-5/qdatetime.html "Qt Documentation") QGitAsset::publishedAt()

Returns the asset publication time.

---

### <a id='id'/> int QGitAsset::id()

Returns the asset identifier.

---

### <a id='size'/> int QGitAsset::size()

Returns the asset file size.

---

### <a id='download-count'/> int QGitAsset::downloadCount()

Returns the number of the the asset downloads.
