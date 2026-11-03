# Mark Reader

面向 macOS 的 Qt 6 Widgets 电子书阅读器。支持 TXT/EPUB 导入、按最近阅读排序、封面卡片书架、章节导航、书签、可自定义中英文字体、滚动/左右翻页、划词笔记侧栏、全文搜索、复古纸页/夜读主题和阅读位置恢复。

## 界面与封面

书架以封面卡片展示藏书：EPUB 优先读取书内声明的封面图；TXT 或没有封面的书会按书名生成一张配色稳定的布面精装封面。旧书库中的书在启动时也会自动补齐封面，同步文件夹会把封面一起复制。

![复古书架](docs/screenshots/shelf-after.png)

阅读页使用羊皮纸底色和宋体排版，工具栏、按钮、下拉框、输入框、滚动条都沿用同一套烫金与暗红配色；右上角“夜读/晨读”可在纸页与暖色夜间主题之间切换。新版本在阅读页顶部加入翻页方式、字体设置和笔记入口。

![复古阅读页](docs/screenshots/reader-with-controls-after.png)

## 阅读设置、翻页与笔记

点阅读页右上角“设置”可以分别选择**中文字体**和**英文字体**，并调整字号。这里的“中英文分别设置”会真正按字符类型渲染：中文、中文标点和日韩文字使用中文字体，英文、数字和西文标点使用英文字体，而不是只换一个无法覆盖中文的字体名。字体候选来自本机已安装字体，换一台 Mac 后缺失的字体会自动回退。

![阅读设置](docs/screenshots/reader-settings-after.png)

把中文字体换成苹方、英文字体换成 Baskerville、字号调到 20 pt 后，正文会同时使用两套字体渲染：

![更换中英文字体后的阅读页](docs/screenshots/reader-fonts-after.png)

阅读页支持两种翻页方式：

- **滚动翻页**：保留滚轮和触控板滚动，适合连续阅读。
- **左右翻页**：每次翻一页，带横向滑动、书页边缘阴影和烫金分隔线组成的翻页动画；支持 `←` / `→`、`↑` / `↓`、`PageUp` / `PageDown`、空格和滚轮，点击正文左右边缘也能翻页。页脚会显示当前页和总页数。

![左右翻页](docs/screenshots/reader-page-mode-after.png)

翻页动画进行中的一帧：旧页向左退去，新页从右侧进入，中间是书页阴影。

![翻页动画中间帧](docs/screenshots/reader-page-turn-mid-after.png)

选中文字后，选区末尾会出现“＋ 笔记”浮标，点它或工具栏的“划词笔记”就会打开右侧笔记侧栏。侧栏上方是当前划线的引用、输入框和记录时间，下方是这本书的全部笔记；每条笔记都带 `yyyy-MM-dd HH:mm` 时间戳和当时的原文引用，点击卡片会跳回对应的划线，方便再次阅读时回忆“这句话是什么时候记下的”。

![划词浮标](docs/screenshots/reader-selection-after.png)

![笔记侧栏](docs/screenshots/reader-notes-after.png)

原来的批注弹窗和批注下拉列表已经移除，改成可以一直打开的侧栏：

| 改造前：批注弹窗 + 下拉列表 | 改造后：划词 + 侧栏 + 时间戳 |
| --- | --- |
| ![改造前的批注体验](docs/screenshots/reader-notes-before.png) | ![改造后的笔记侧栏](docs/screenshots/reader-notes-after.png) |

旧版“划线 / 批注”按钮弹出的输入框：

![旧版批注弹窗](docs/screenshots/reader-notes-popup-before.png)

## 构建与运行

需要 Qt 6（Widgets、Test）、libzip 和 CMake 3.21 或更新版本。Homebrew 环境可安装依赖：

```sh
brew install qt cmake libzip
```

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

在 macOS 上，应用构建为 `build/Mark Reader.app`。CMake 生成的应用包仍使用本机 Qt/libzip 运行库；若要把 Qt 运行库和插件一起复制进包（便于在未安装 Qt 的 Mac 上运行），执行：

```sh
macdeployqt "build/Mark Reader.app"
```

Homebrew 将 Qt Svg 单独安装时，需要显式添加搜索路径；本项目的验证命令为：

```sh
xattr -cr "build/Mark Reader.app"
macdeployqt "build/Mark Reader.app" -libpath="$(brew --prefix qtsvg)/lib" -no-codesign
```

`-no-codesign` 适合本地验证；正式分发还需要代码签名和公证。

书籍副本和 `library.json` 保存在 Qt 的应用数据目录；字号、字体、翻页方式和主题偏好保存在系统应用设置中。笔记和书签随书保存在 `library.json`，笔记的记录时间使用本机时间。“同步文件夹”会把指定文件夹（含子文件夹）里的 TXT/EPUB 导入应用，默认来源是 iCloud Drive 的 `本机迁移/Documents/kindle`；右键按钮可更换文件夹。重复点击只会导入新书，书名和正文高度重复的副本会自动跳过或合并，只保留阅读痕迹最完整的一本。该功能不直接连接云服务，也不提供账号认证或后台自动同步。

| 改造前：同步 = 把书导出到文件夹 | 改造后：同步 = 从文件夹导入新书 |
| --- | --- |
| ![改造前的同步](docs/screenshots/sync-before.png) | ![改造后的同步](docs/screenshots/sync-after.png) |

## 代码导览

- `MainWindow` 管理书架/阅读页切换、按钮交互、设置持久化和笔记侧栏的打开/关闭。
- `ReaderWidget` 显示纯文本，保存滚动位置；负责中英文字体按字符覆盖、滚动/按页两种模式、键盘/滚轮/边缘点击翻页和横向翻页动画。
- `SettingsDialog` 收集中英文字体与字号，展示真实字体预览。
- `AnnotationPanel` 是右侧笔记侧栏：划词引用、笔记输入、带时间戳的历史笔记卡片和跳转。
- `EpubDocument` 读取 EPUB 容器、OPF spine、内嵌封面和 XHTML 章节，抽取纯文本供阅读。
- `CoverArt` 生成布面精装封面；“封面文件缺失”会在加载书库时自动补回。
- `Theme` 集中管理纸色、墨色、烫金、字体候选和全局样式表；`RetroWidgets` 绘制纸张背景与书架封面卡片。
- `LibraryStore` 导入 TXT/EPUB 应用内副本，以原子 JSON 文件保存书架、阅读进度、书签和批注（含记录时间）；`importFolder` 从同步文件夹批量导入，并按书名和正文相似度合并重复书籍。
- `tests/test_librarystore.cpp` 验证导入、副本、书架重载、阅读进度、书签批注、EPUB 章节与封面、主题样式、字体选择与中英文渲染、翻页、设置对话框、笔记侧栏、笔记保存和封面卡片等功能。

## 实现小记

- **中英文字体**：Qt 的一个 `QFont` 在缺字时会走系统回退，不能保证中文一定用选中的中文字体。项目先把全文设成中文字体，再扫描连续的非中文字符片段，用 `QTextCharFormat` 覆盖成英文字体；整段中文只需要一次格式设置，避免逐字处理。
- **翻页单位**：`QPlainTextEdit` 的纵向滚动条以“行”而不是像素计数，`pageStep()` 就是当前视口能容纳的行数。按页翻动时以 `pageStep` 为步长，并用前后两张视口截图做水平过渡动画。
- **笔记时间**：`Annotation` 在保存时由 `LibraryStore` 写入 `createdAt`，侧栏卡片直接读取这个时间，不用重新计算。
