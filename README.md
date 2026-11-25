# ClassRollCall 2.0 · 班级点名器

C++17 / Win32 / GDI+ 原生重构。日常只显示一个半透明的 **Call** 悬浮按钮。
原版作者 enderLuoxh，MIT License。

## 使用

1. 解压 Release 包，双击 `ClassRollCall.exe`。无需安装、管理员权限或联网。
2. 首次设置中导入班级名单；开机启动默认关闭，也可以跳过导入。
3. 单击悬浮按钮，姓名滚动约 1 秒，最终结果停留约 3 秒后淡出。
4. 右键或触摸长按约 500 ms 打开菜单，切换班级、导入、管理名单或退出。
5. 拖动悬浮按钮调整位置；双击系统托盘图标显示或隐藏按钮。

默认按钮约 50% 不透明，抽取期间约 95%；抽取期间忽略重复点击。
中央结果卡片默认穿透鼠标和触摸，不抢占 PPT 焦点。菜单项目高 48 DIP。

## 名单与导入

支持 `.txt`、`.csv`、`.xls`、`.xlsx`、`.docx`。可以一次选择多个文件，或将文件
拖到悬浮按钮上。每个文件默认作为一个班级，以文件名命名。

剪贴板：右键 → 管理班级名单 → 粘贴名单。从 Excel 复制多列数据时可以选取姓名列。
管理窗口也可新增空班级、编辑逐行姓名、重命名及删除班级。

导入预览可选择工作表 / Word 表格、姓名列、跳过的表头行数，并显示人数、空白和
重复记录数量。识别到“姓名 / 学生姓名 / 名字 / 学生 / Name / Student Name”时
自动选择；没有明确表头的多列表格必须手动选列。同名学生作为独立记录保留，
仅在勾选去重时删除重复。多工作表文件一次确认所选工作表；需要其他表时再次导入。

同名班级必须选择重命名、替换或取消。导入不会运行宏、公式或外部资源；公式仅读取
已缓存的结果。旧版 `.doc`、PDF、图片 / OCR 不支持。

支持 UTF-8、UTF-8 BOM、UTF-16 LE/BE BOM、GBK / GB18030；无 BOM 的非 UTF-8
文本按 GB18030 解码。CSV 支持逗号、分号、制表符，以及引号和换行转义。

为控制内存，导入限制：文件 16 MiB、Office 单项解压 16 MiB、总解压 64 MiB、
2048 个压缩条目、64 个工作表、10000 行、256 列、100000 个单元格。
超过限制会提示，建议先导出姓名列。导入发生在 UI 线程，大文件解析期间界面会短暂等待。

## 抽取模式

- **均衡抽取**：Fisher–Yates 洗牌，一轮内每条记录恰好一次，结束后重新洗牌。
- **完全随机**：每次独立均匀抽取，允许连续抽中同一人。

切换班级、修改名单或切换模式后重置本轮；重新启动也重新洗牌。

## 数据和开机启动

数据位于 `%LOCALAPPDATA%\ClassRollCall\`，含 `settings.ini`、`classes.ini` 和
`rosters/*.txt`。名单采用 UTF-8，一行一条记录。文件先写入临时文件，刷新后原子替换。
修改班级时使用新名单 ID，再切换班级索引；旧文件不会被当作当前名单。
如需备份，复制整个数据目录。旧版 `settings.json`、`position.json` 和 `names.csv`
不会被覆盖，也不会自动导入；可以从旧版 CSV 手动导入。

自启动使用当前用户的注册表 Run 项。移动 EXE 后，下次运行检测到路径变化时会提示修复。
程序不安装服务，不提供自动更新，不发送网络请求。

## 系统兼容性与 DPI

优先支持 Windows 10 / 11，x86 Release 可运行在 32 位和 64 位系统上。
Win7 SP1 / Win8 / 8.1 保留代码级降级路径；**未经过干净系统实测前，不宣称兼容验收通过**。
最新 DPI / Pointer API 均通过动态查找使用。Win7 使用系统 DPI 和 WM_TOUCH；
Win8+ 使用 WM_POINTER；Win10 1703+ 优先 Per-Monitor V2。

界面以 DIP 布局，处理 DPI 改变。位置保存显示器名称和相对工作区的 DIP 坐标；
移除显示器后回到主显示器。测试覆盖范围见 [测试记录](docs/TEST_REPORT.md)。

## 构建

开发机需要 Visual Studio 2022 的 C++ 桌面开发工具和 Windows SDK、CMake 3.24+。
默认静态链接 MSVC 运行库和解析依赖，发布文件为 `dist/ClassRollCall.exe`。
首次配置下载固定版本源码；依赖缓存完成后可离线重新构建。

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build.ps1
# 同时生成实际 Office 测试文件并执行测试（测试开发机才需要 Python）：
powershell -ExecutionPolicy Bypass -File scripts/build.ps1 -Test
```

也可以直接执行：

```powershell
cmake -S . -B build -A Win32 -DBUILD_TESTING=OFF
cmake --build build --config Release --parallel 2
```

完整自动验证由 `.github/workflows/native.yml` 执行，包含 x86 Release 构建、
算法 / 持久化测试、真实导入文件测试、GUI 冒烟及导入表检查。
发布包同时保留 `LICENSE`、第三方许可和测试结果；运行时只需主要 EXE。

`src/ClassRollCall.py` 和 `tools/` 是原版历史参考，不参与原生目标构建。
第三方来源见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)，需求见
[PROJECT_SPEC.md](docs/PROJECT_SPEC.md)，旧版核查见 [原版实现核查](docs/LEGACY_REVIEW.md)。
