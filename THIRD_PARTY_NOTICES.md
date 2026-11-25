# Third-party notices

The executable statically links the following readers. No Office, Python, Qt,
browser engine, VC redistributable or network connection is required at runtime.

| Component | Version / source | License |
|---|---|---|
| miniz | [3.0.2](https://github.com/richgel999/miniz/tree/3.0.2) | MIT |
| pugixml | [1.15](https://github.com/zeux/pugixml/tree/v1.15) | MIT |
| libxls | [1.6.3, c199d132494833da696b58aa4acf3fc5a36d930b](https://github.com/libxls/libxls/tree/c199d132494833da696b58aa4acf3fc5a36d930b) | BSD 2-Clause |

CMake copies each upstream license verbatim into `build/licenses/`. The release
archive includes these notices alongside the executable. Keep the licenses when
redistributing. `src/importer/xls_locale.c` is this project's Windows UTF-8 bridge
for libxls's locale interface; it avoids reliance on a machine's active codepage.

Original ClassRollCall 1.5 source remains in `src/ClassRollCall.py` and `tools/`
as an unbuilt historical reference. Copyright (c) 2026 enderLuoxh, MIT License.
The C++ target neither loads nor launches the Python source.

Fixture generation uses openpyxl 3.1.5 (MIT), python-docx 1.1.2 (MIT), and xlwt
1.3.0 (BSD). These are development-only tools and are not linked or bundled.
