# ClassRollCall 2.0.2 班级点名器

C++17 / Win32 / GDI+ 原生重构版。日常屏幕上只有一个白色圆角悬浮按钮，点一下即可随机点名。
原版作者 enderLuoxh，MIT License。

## 使用

1. 在 Windows 10/11 双击 `ClassRollCall.exe`，无需安装、管理员权限或联网，不需要 .NET、Python、Qt 或另装 VC++ 运行库。
2. 首次运行导入班级名单（可跳过）；开机启动默认关闭。
3. 单击悬浮按钮，姓名快速滚动约 1 秒，结果在屏幕中央停留约 3 秒后淡出。
4. 右键悬浮按钮（触摸屏长按约 0.5 秒）打开菜单：切换班级、管理名单、导入名单、
   抽取模式、开机自动启动、重置位置、隐藏按钮、退出。
5. 拖动按钮可换位置；双击托盘图标显示/隐藏按钮。

按钮平时约 50% 不透明，点击抽取时提高到约 95%，抽取结束后恢复半透明。
结果卡片不接收鼠标和触摸，不会打断 PPT 放映。

## 界面

- 悬浮按钮：白色圆角矩形，112 × 64 DIP（窗口 124 × 76 DIP，内含 6 DIP 透明边），
  灰色「点名」二字，无图标、无阴影、无渐变。
- 结果卡片：460 × 168 DIP 白色圆角卡片，只显示姓名（粗体、自动缩放），居中于按钮所在显示器。
- 右键菜单：自绘圆角弹层，48 DIP 行高，选中勾固定在 14 DIP 方框内；支持鼠标与触摸点按子菜单；
  点击卡片外或按 Esc 关闭。
- 管理窗口：Windows 11 设置式左侧导航（班级名单 / 导入名单 / 使用偏好）。
- 全部尺寸以 DIP 计算，跟随系统 DPI 与浅色 / 深色主题；颜色为中性灰白 + Windows 蓝，
  不使用渐变、图片或实时模糊。

## 名单与导入

支持 `.txt`、`.csv`、`.xls`、`.xlsx`、`.docx` 与剪贴板粘贴。可以一次选择或拖入多个文件，
每个文件默认生成一个以文件名命名的班级。

- 文本：UTF-8 / UTF-8 BOM / UTF-16 LE、BE（含 BOM）/ GBK / GB18030；自动去空白行与首尾空格。
- CSV：逗号、制表符、分号分隔，支持引号与换行转义。
- XLS：libxls 读取 BIFF，支持中文、多列、多工作表。
- XLSX：ZIP + XML 解析，支持共享字符串、内联字符串、多工作表；公式只读缓存值。
- DOCX：ZIP + XML 解析，支持段落与表格。
- 剪贴板：从 Excel / Word / 网页复制的文本直接粘贴预览。

导入前显示预览：文件名、工作表、姓名列、人数、姓名样例、重复与空白数量。
识别到「姓名 / 学生姓名 / 考生姓名 / 名字 / 学生 / Name / Student Name」时自动选列；多工作表时
按「识别出的姓名列中非空单元格最多」自动选择工作表，也可手动切换。无法确定姓名列
时必须手动选择，不会把学号、性别等列混进名单。同名记录默认保留，可勾选去重。

导入安全：不执行宏与公式，不访问网络，XML 禁用 DTD 与外部实体，ZIP 限制单项 16 MiB、
总量 64 MiB、2048 个条目，表格限制 64 个工作表 / 10000 行 / 256 列 / 100000 个单元格。
损坏或超限文件会被拒绝并显示错误；测试覆盖损坏 ZIP、解压大小超限和 DTD 样本。

## 抽取模式

- **均衡抽取**（默认）：Fisher–Yates 洗牌，一轮内每人恰好一次，抽完自动重洗。
- **完全随机**：每次独立均匀抽取，允许重复。

切换班级、修改名单、切换模式或重启都会重置本轮。
结果显示期间选择班级、模式或名单管理，会收起当前结果并执行操作，不再静默忽略。

## 数据与开机启动

数据存放在 `%LOCALAPPDATA%\ClassRollCall\`：

```
settings.ini     当前班级、抽取模式、位置、主题、开机启动
classes.ini      班级 ID 与名称
rosters/<id>.txt UTF-8 名单，一行一个记录
```

写入采用临时文件 + 原子替换。备份时复制整个目录即可。旧版 `names.csv` 不会被读取。
系统目录拒绝写入或设置文件被占用时，会自动尝试 EXE 旁的 `data/<版本数据目录>`，
再尝试用户漫游目录和文档目录；切换目录会保存完整名单与设置，保留原目录。
重启时读取最近成功保存的完整配置。使用软件旁的 `data` 目录时，移动软件请连同它一起复制。
拖动保存失败不会重复弹出阻塞对话框。全部位置都无法保存时，本次运行仍可点名，
托盘仅提示一次设置尚未保存，编辑名单时仍会明确报告保存失败。

开机启动写入当前用户的 `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`，
不需要管理员权限；EXE 移动后下次启动会提示修复路径。程序不安装服务、不联网、无后台线程。

## 兼容性

- Windows 10 / 11：完整支持。
- Windows 8 / 8.1：兼容（WM_POINTER、Per-Monitor V1）。
- Windows 7 SP1：代码保留 System DPI + WM_TOUCH 降级路径，但**本仓库当前的 zig 构建使用
  UCRT 运行库**，干净 Win7 需要 KB2999226；不含该系统更新的机器请使用 MSVC `/MT` 构建
  （见下）。Win7 未在干净虚拟机实测，不作兼容性声明。
- 全部为 x86，可运行于 32 / 64 位系统。较新的 DPI / Pointer API 均通过 `GetProcAddress`
  动态加载，不会静态依赖旧系统不存在的导入项。

## 构建

仓库提供两条构建路径。

### 一、MSVC + CMake（正式 Win7 兼容构建，需 Visual Studio 2022 + Windows SDK）

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build.ps1        # Release x86 -> dist/ClassRollCall.exe
powershell -ExecutionPolicy Bypass -File scripts/build.ps1 -Test  # 额外生成测试夹具并执行测试
```

或手动：

```powershell
cmake -S . -B build -A Win32 -DBUILD_TESTING=OFF
cmake --build build --config Release --parallel 2
```

`/MT` 静态运行库，输出单个 EXE。第三方依赖已存放于 `third_party/vendor/`，构建无需联网。

### 二、zig 交叉编译（无需 Windows SDK，输出 UCRT 版本）

```powershell
$env:ZIG_CACHE="D:\zig-cache"      # 可写目录
python scripts/build_zig.py --clean
python scripts/build_zig.py --tests
```

`scripts/build_zig.py` 用 `zig c++ --target=x86-windows-gnu` 编译，`scripts/makeres.py`
直接生成含 manifest、版本信息与图标的 COFF 资源对象，`scripts/make_icon.py` 绘制应用图标。
产物为 `dist/ClassRollCall.exe`。该构建依赖系统 UCRT（Windows 10/11 自带）。

### 测试

```powershell
python scripts/make_fixtures.py                 # 生成 tests/fixtures（需 openpyxl / python-docx / xlwt）
dist\tests\core_tests.exe
dist\tests\import_tests.exe tests\fixtures
```

结果与待测项见 [测试记录](docs/TEST_REPORT.md)。

本次菜单、资源、触屏与稳定性修复及验证范围见 [2.0.1 修复记录](docs/RELEASE_2_0_1.md)。
普通权限下自动选择可写目录、完整迁移名单和避免反复保存弹窗见 [2.0.2 修复记录](docs/RELEASE_2_0_2.md)。

### 内置名单的本地专用版

把本地名单存入 `private/example.roster.json`，例如：

```json
{"id":"ExampleClass","class_name":"示例班级","names":["张三","李四"]}
```

```powershell
python scripts/build_zig.py --edition private/example.roster.json --output dist/示例班级点名器.exe
```

专用版首次打开自动载入内置名单，跳过首次导入。每个 `id` 使用独立的数据目录、单实例锁、
悬浮窗口类名和开机启动项。名单为空时会恢复该版本内置名单，之后编辑的名单正常保存。
`private/` 和 `*.roster.json` 已忽略，生成的名单源文件只写入 `build/`，请勿把真实名单提交到公共仓库。

其他 UI 回归测试（虚构名单与独立配置）：

```powershell
python tests/functional_ui.py dist/ClassRollCall.exe
python tests/resource_tests.py dist/ClassRollCall.exe
python tests/menu_input_tests.py dist/ClassRollCall.exe
# 以下两项会在当前桌面移动鼠标、注入触摸，仅在本机专门测试时运行：
python tests/menu_input_tests.py dist/ClassRollCall.exe --real --path --touch
```

## 目录

```
src/core       抽取算法、随机数
src/storage    INI 设置与名单存储（原子写）
src/importer   txt/csv/xls/xlsx/docx/剪贴板解析
src/ui         悬浮窗、结果卡、自绘菜单、管理窗口、绘制层
src/platform   DPI、指针/触摸、开机启动、显示器定位
scripts/       构建、资源、图标、测试夹具
third_party/   miniz / pugixml / libxls（见 THIRD_PARTY_NOTICES.md）
```

`src/ClassRollCall.py` 与 `tools/` 是原版 PyQt5 实现，仅作历史参考，不参与构建。
规格见 [docs/PROJECT_SPEC.md](docs/PROJECT_SPEC.md)。
