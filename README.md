# Mark Reader

面向 macOS 的 Qt 6 Widgets TXT 阅读器 MVP。支持导入 UTF-8 TXT、在书架中打开、调整字号，并保存/恢复阅读位置。

## 构建与运行

需要 Qt 6（Widgets、Test）和 CMake 3.21 或更新版本：

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/mark-reader
```

书籍副本和 `library.json` 保存在 Qt 的应用数据目录；字号偏好保存在系统应用设置中。

## 代码导览

- `MainWindow` 管理书架/阅读页切换和按钮交互。
- `ReaderWidget` 显示纯文本、提供滚动位置并响应字号设置。
- `LibraryStore` 导入应用内副本，以原子 JSON 文件保存书架和阅读进度。
- `tests/test_librarystore.cpp` 验证副本、书架重载、进度恢复和错误输入。
