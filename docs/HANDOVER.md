# 交接文档

本文件说明 ClassRollCall 2.0 当前的实现状态、本轮改动、目录职责、构建与测试方法，
以及后续接手时需要注意的坑。规格原文见 [PROJECT_SPEC.md](PROJECT_SPEC.md)。

---

## 1. 一句话现状

原生 C++17 / Win32 / GDI+ 的点名工具已经能编译、能运行、测试全绿：
日常只有一个白色圆角「点名」悬浮按钮，点一下在屏幕中央显示抽中的姓名。
交付物是 `dist/ClassRollCall.exe`（1,511,424 字节，x86，单文件，免安装）。

**未完成的是实机验收**：Win7、实体触摸屏、托盘、混合 DPI 多显示器都还没有真机证据，
详见 [TEST_REPORT.md](TEST_REPORT.md) 的「尚需实机验收」。

---

## 2. 本轮（接手后）做了什么

接手时的状态：Codex 已经写过一版完整实现（`codex/native-v2` 分支），但存在几个硬伤，
且界面被用户评价为「不够现代化」「太丑」。本轮工作分三块：

### 2.1 让它在我这台机器上能跑起来

| 问题 | 处理 |
|---|---|
| 没有 MSVC / Windows SDK / CMake 可用 | 用 zig 0.13 交叉编译，自写 `scripts/build_zig.py`，不需要任何 Windows SDK |
| `zig rc` 在本环境无法运行 | 自写 `scripts/makeres.py`，直接生成 i386 COFF 资源对象（manifest / 版本信息 / 图标） |
| 资源重定位用 `IMAGE_REL_I386_DIR32` 导致启动报「不是有效的 Win32 应用程序」 | 改用 `IMAGE_REL_I386_DIR32NB`（RVA 不加 imagebase） |
| `-mwindows` 被 zig 忽略，产出控制台程序 | 链接参数改为 `-Wl,--subsystem,windows` |
| 第三方库原本用 `FetchContent` 联网下载（本机 TLS 坏了） | 下载后 vendored 进 `third_party/vendor/`，构建完全离线 |
| 没有 shell（无 explorer.exe）时 `Shell_NotifyIcon` 失败 → 程序弹「无法启动」后卡死 | 托盘失败降级为非致命（`trayAdded_` 标志），程序照常运行 |
| `_WIN32_WINNT=0x0601` 下 Win8 指针 API 不可见，编译失败 | 新增 `src/platform/compat.hpp` 补声明，运行时仍走 `GetProcAddress` |

### 2.2 按用户反馈重做视觉（多轮迭代）

用户的原始反馈链（按时间）：
「所有的UI都不够现代化」→「还是原来的简洁风，要给老师用也不要出问题」→
「按钮就白色，其他颜色太丑了」→「点名按钮还是长方形好看，骰子图标去掉」→
「进度条去掉，小字都去掉，就名字就可以」→「这还有一圈阴影，给弄好了啊所有样式」→
「完全离线这些所有提示全部删掉，关于也删掉，美化一下右键打开的菜单」→「软件图标你随便绘制一下」。

最终形态（细节见 [UI_UPDATE.md](UI_UPDATE.md)）：

- **悬浮按钮**：白色圆角矩形 112×64 DIP，只有「点名」二字，无图标/无阴影/无渐变/无进度条；
  平时 50% 不透明，按下与抽取时 95%。
- **结果卡片**：460×168 DIP，只显示姓名（粗体、超长自动缩字），滚动 1 秒 → 定格 3 秒 → 淡出；
  不接收鼠标与触摸。
- **右键菜单**：全新自绘圆角弹层（`src/ui/menu.cpp`），取代原生 `TrackPopupMenuEx`。
- **图标**：`scripts/make_icon.py` 画的蓝色圆角方块 + 白色对勾，9 个尺寸，用于 EXE / 标题栏 / 托盘。
- 删掉了「关于」页和所有「完全离线」文案。

### 2.3 修的真实 bug

- 导入带封面页的 Excel 时默认选中第一张「说明」表，会把说明文字当名单导入。
  现在 `dialogs.cpp` 的 `bestSheet()` 会挑「识别出的姓名列里非空单元格最多」的工作表。
- 抽取动画期间悬浮按钮的进度环不刷新（只重绘了结果窗）。
- `theme` 设置字段原来只写不读，`theme=dark` 实际无效。

### 2.4 补齐测试

- `scripts/make_fixtures.py`：生成 17 个真实夹具（GBK/GB18030/UTF-16 带 BOM、多工作表 xlsx、
  共享字符串 + 公式缓存 xlsx、BIFF8 xls、docx 段落与表格、损坏 ZIP、ZIP bomb、DTD 注入）。
- `build_zig.py --tests` 构建 `core_tests` / `import_tests`，两者均通过。
- GUI 冒烟 24 项检查全部通过（见 TEST_REPORT）。

---

## 3. 目录与文件职责

```
ClassRollCall/
├── CMakeLists.txt          MSVC + CMake 构建入口（Win7 正式产物用这条路径）
├── README.md               使用说明 + 两条构建路径
├── LICENSE                 MIT（原项目 enderLuoxh）
├── THIRD_PARTY_NOTICES.md  第三方组件、版本、许可证
├── dist/                   交付目录：ClassRollCall.exe + LICENSE + README + 声明
├── docs/
│   ├── PROJECT_SPEC.md     需求规格（用户给的原文）
│   ├── TEST_REPORT.md      实测结果 + 待实测清单
│   ├── UI_UPDATE.md        最终视觉规格与尺寸常量
│   ├── HANDOVER.md         本文件
│   ├── LEGACY_README.md    原版 PyQt 项目 README 留存
│   ├── LEGACY_REVIEW.md    原版实现核查记录
│   ├── validation/         机器可读的测试结果 JSON
│   └── screenshots/        本轮实际截图（按钮/结果卡、菜单、管理窗口）
├── src/
│   ├── main.cpp            入口：单实例 mutex、GDI+ 初始化、--preview 离屏渲染分支
│   ├── core/random.*       抽取算法：均衡模式 Fisher–Yates、完全随机、mt19937 + 系统种子
│   ├── storage/store.*     settings.ini / classes.ini / rosters 读写，临时文件 + 原子替换
│   ├── importer/
│   │   ├── importer.hpp    统一接口：Document / Sheet / detectNames / selectNames
│   │   ├── importer.cpp    按扩展名分发
│   │   ├── text_csv.cpp    编码探测（UTF-8/BOM/UTF-16/GBK/GB18030）+ CSV 引号转义
│   │   ├── xls.cpp         libxls 读取 BIFF
│   │   ├── xlsx.cpp        ZIP + XML（miniz + pugixml），共享字符串 / 内联字符串 / 缓存公式
│   │   ├── docx.cpp        ZIP + XML，段落与表格
│   │   ├── office.cpp      ZIP 安全限制（大小/条目数/解压比）
│   │   └── xls_locale.c    libxls 的 Windows UTF-8 locale 桥接
│   ├── ui/
│   │   ├── application.*   悬浮窗与结果窗的创建、拖动、点击、动画定时器、命令分发
│   │   ├── paint.*         悬浮按钮与结果卡片的绘制（含 metrics 尺寸常量）
│   │   ├── skin.*          调色板、圆角/描边/文字绘制、分层窗口提交（UpdateLayeredWindow）
│   │   ├── menu.*          自绘右键菜单（鼠标捕获方案，见下方「坑」）
│   │   ├── dialogs.*       管理窗口、导入预览、首次运行、通知框
│   │   ├── dialog_window.* 自绘对话框基类（原生子控件 + 自绘背景）
│   │   ├── theme.*         旧调色板（部分仍被 dialog_window 使用）与标题栏样式
│   │   └── preview.*       离屏渲染 PNG（--preview），用于快速检查视觉
│   ├── platform/
│   │   ├── platform.*      DPI 感知、显示器工作区、坐标夹取、开机启动注册表
│   │   └── compat.hpp      Win7 目标下补齐 Win8 指针 API 声明
│   └── resources/          manifest 与 .rc（供 MSVC 构建使用）
├── scripts/
│   ├── build_zig.py        zig 交叉编译（本机使用；--tests / --clean / CRC_NO_RES）
│   ├── makeres.py          生成 COFF 资源对象（manifest + 版本 + 图标）
│   ├── make_icon.py        用 Pillow 画多尺寸 ICO
│   ├── make_fixtures.py    生成 tests/fixtures 的 17 个测试文件
│   ├── generate_fixtures.py 旧版夹具脚本（保留）
│   ├── build.ps1           MSVC + CMake 构建（Win7 产物）
│   └── smoke.ps1           CI 用冒烟脚本
├── tests/
│   ├── core_tests.cpp      算法 / DPI 算术 / 持久化
│   ├── import_tests.cpp    六类导入 + 安全用例
│   └── fixtures/           17 个真实测试文件
├── third_party/
│   ├── CMakeLists.txt      连接 vendor 源码的 CMake 入口
│   ├── config.h.in         libxls 配置模板
│   └── vendor/{miniz,pugixml,libxls}  固定 commit 的源码（离线）
├── src/ClassRollCall.py    原版 PyQt5 实现（历史参考，不参与构建）
└── tools/                  原版打包脚本（历史参考）
```

### 运行时的用户数据（不在仓库里）

```
%LOCALAPPDATA%\ClassRollCall\
├── settings.ini   当前班级 / 抽取模式 / 位置 / 主题 / 开机启动
├── classes.ini    GUID=班级名，一行一个
└── rosters\<GUID>.txt   UTF-8 名单，一行一个
```

测试时用环境变量 `LOCALAPPDATA` 重定向到 `D:\project\_run`，不会碰到真实数据。

---

## 4. 构建

### 4.1 zig（本机可用，产物依赖 UCRT）

```powershell
$env:ZIG_CACHE = "D:\zig-cache"          # 必须可写；默认缓存目录在沙箱里会被拒
python scripts/build_zig.py --clean       # 产出 dist/ClassRollCall.exe
python scripts/build_zig.py --tests       # 额外产出 tests 到 build/zig/tests
```

- 目标三元组是 `x86-windows-gnu`（不是 `i686-windows-gnu`）。
- 只用 `zig c++` 编 `.cpp`，`zig cc` 编 `.c`。
- 环境变量 `CRC_NO_RES=1` 可跳过资源链接，便于二分定位启动问题。

### 4.2 MSVC + CMake（Win7 正式产物）

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build.ps1
cmake -S . -B build -A Win32 -DBUILD_TESTING=OFF
cmake --build build --config Release --parallel 2
```

`/MT` 静态运行库，不产生 `api-ms-win-crt-*` 依赖，这是**唯一能在干净 Win7 SP1 上跑的路径**。
本机没有 MSVC，因此这条路径本轮未实跑。

---

## 5. 测试

```powershell
python scripts/build_zig.py --tests
build\zig\tests\core_tests.exe
build\zig\tests\import_tests.exe tests\fixtures
```

预期输出：`core tests passed`、`import tests passed`。

GUI 冒烟与截图脚本是我在本机临时写的，放在 `D:\project\_tmp\`（不属仓库）：
`ui.py`（ctypes 封装：启停、枚举窗口、PrintWindow 截图、消息注入）、`smoke.py`（24 项检查）、
`menu_test.py`（菜单）、`clip2.py`（剪贴板导入）、`drop.py` / `multi.py`（拖放）、
`shot5.py`（截图序列）、`memprobe.py`（内存）、`preview.py`（拼图）。
这些脚本依赖「同一进程内启动 + 操作 + 截图」，换机器可以直接复用。

---

## 6. 已知限制

1. **Win7 未实测**。zig 产物依赖 UCRT；干净 Win7 需要 KB2999226 或改用 MSVC `/MT` 构建。
2. **托盘未实测**。测试会话没有 explorer.exe，`Shell_NotifyIcon` 必然失败；代码已降级为非致命。
3. **触摸未实测**。WM_POINTER / WM_TOUCH 两条路径只做了代码级实现，没有实体触摸屏。
4. **混合 DPI 多显示器未实测**。单显示器环境，跨屏换 DPI 的行为没有证据。
5. **开机启动未实测**。沙箱禁止写 `HKCU\...\Run`；实际表现是弹「操作未完成」提示且进程存活。

---

## 7. 接手注意（踩过的坑）

- **自绘菜单为什么用鼠标捕获而不是激活窗口**：悬浮窗是 `WS_EX_NOACTIVATE`，
  若菜单窗口靠 `SetForegroundWindow` 激活，激活失败时点菜单外面关不掉——对老师是致命问题。
  现在的实现用 `SetCapture()` 接管鼠标，点卡片外、Esc、选中都会关闭。
- **不要在 zig 输出上接 `Select-Object`/`Select-String`**：管道提前关闭会杀掉 zig，
  表现为假的 `error: AccessDenied`。重定向到文件再看。
- **GUI 子系统 EXE 从 PowerShell 启动必须重定向 stdout**，否则工具会一直等到超时。
- **`PrintWindow` 抓分层窗口拿到的透明区域是黑色**，这是截图伪影，不是界面问题；
  透明 alpha 不会体现在抓图里（要判断真实透明度得看运行效果或渲染离屏预览）。
- **PowerShell 的 `Get-Content` 读无 BOM 的 UTF-8 中文会乱码**，判断源码编码用 `read` 工具或 Python。
- **改 `skin.cpp` 的调色板初始化列表必须数个数**：早前一次替换漏掉一个值，后面字段整体错位，
  圆点渲染成紫色。
- **`src/ui/theme.cpp` 与 `src/ui/skin.cpp` 并存**：新的界面元素用 skin，旧对话框仍用 theme，
  不要以为 theme 是死代码。

---

## 8. 建议的下一步

1. 在带 explorer 的正常桌面上跑一遍：托盘双击/右键、按钮显示隐藏、拖到副屏。
2. 用 MSVC 构建一次，在干净 Win7 SP1 虚拟机验证启动与导入。
3. 补 100% / 125% / 175% / 200% 与 1080P / 2K / 4K 的截图，填进 TEST_REPORT。
4. 鸿合触摸屏实测：点按、拖动、500 ms 长按、连续点按、触摸与鼠标不重复触发。
5. 若还需要视觉调整，先跑 `--preview` 或 `shot5.py` 出图确认，再改代码。