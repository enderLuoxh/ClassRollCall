"""Generate the fixture set used by tests/import_tests.cpp.

Fixture data is deterministic: every roster is 张三 / 李四 / 张三 / 王五 so that
duplicate counting and column detection can be asserted exactly.

Requires openpyxl, python-docx and xlwt.  They are not needed to build the
application, only to regenerate this directory, which is why they are optional:
point CRC_PYLIB at a directory that contains them (or install them normally).
"""
import io
import os
import struct
import sys
import zipfile
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FIXTURES = ROOT / "tests" / "fixtures"
LIB = os.environ.get("CRC_PYLIB", r"D:\project\_tools\pylib")
if Path(LIB).is_dir():
    sys.path.insert(0, LIB)

NAMES = ["张三", "李四", "张三", "王五"]
HEADER = ["学号", "姓名", "性别"]
ROWS = [["001", "张三", "男"], ["002", "李四", "女"], ["003", "张三", "男"], ["004", "王五", "男"]]


def text_files():
    body = "\r\n".join(NAMES) + "\r\n"
    (FIXTURES / "utf-8.txt").write_bytes(body.encode("utf-8"))
    (FIXTURES / "utf-8-sig.txt").write_bytes(body.encode("utf-8-sig"))
    (FIXTURES / "gbk.txt").write_bytes(body.encode("gbk"))
    (FIXTURES / "gb18030.txt").write_bytes(body.encode("gb18030"))
    (FIXTURES / "utf-16.txt").write_bytes(body.encode("utf-16"))
    # Python only prepends a BOM for "utf-16"; a big-endian document carries FE FF.
    (FIXTURES / "utf-16-be.txt").write_bytes(b"\xfe\xff" + body.encode("utf-16-be"))


def csv_files():
    rows = [",".join(HEADER)] + [",".join(row) for row in ROWS]
    csv_text = "\r\n".join(rows) + "\r\n"
    (FIXTURES / "utf-8-sig.csv").write_bytes(csv_text.encode("utf-8-sig"))
    (FIXTURES / "gb18030.csv").write_bytes(csv_text.encode("gb18030"))
    quoted = '学号,姓名\r\n001,"张,三"\r\n002,"李""四"\r\n'
    (FIXTURES / "quoted.csv").write_bytes(quoted.encode("utf-8-sig"))


def xlsx_workbooks():
    from openpyxl import Workbook

    book = Workbook()
    first = book.active
    first.title = "说明"
    first["A1"] = "本次导入请使用第二张工作表"
    first["A2"] = "姓名列位于 B 列"
    second = book.create_sheet("名单")
    second.append(HEADER)
    for row in ROWS:
        second.append(row)
    book.save(FIXTURES / "多列 多工作表.xlsx")


def spreadsheet_ml_sheet(path, rows):
    out = [b'<?xml version="1.0" encoding="UTF-8" standalone="yes"?>',
           b'<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><sheetData>']
    for index, row in enumerate(rows, start=1):
        out.append(('<row r="%d">' % index).encode())
        for column, cell in enumerate(row):
            out.append(cell.format(row=index, column="ABCDEFGH"[column]).encode())
        out.append(b"</row>")
    out.append(b"</sheetData></worksheet>")
    return path, b"".join(out)


def shared_formula_xlsx():
    shared = ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
              '<sst xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" count="2" uniqueCount="2">'
              '<si><t>姓名</t></si><si><t>张三</t></si></sst>')
    workbook = ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
                '<workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"'
                ' xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">'
                '<sheets><sheet name="第一个" sheetId="1" r:id="rId1"/>'
                '<sheet name="缓存公式" sheetId="2" r:id="rId2"/></sheets></workbook>')
    rels = ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
            '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
            '<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/>'
            '<Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet2.xml"/>'
            '<Relationship Id="rId3" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/sharedStrings" Target="sharedStrings.xml"/>'
            "</Relationships>")
    sheet1 = ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
              '<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><sheetData>'
              '<row r="1"><c r="A1" t="inlineStr"><is><t>说明</t></is></c></row>'
              "</sheetData></worksheet>")
    sheet2 = ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
              '<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><sheetData>'
              '<row r="1"><c r="A1" t="s"><v>0</v></c></row>'
              '<row r="2"><c r="A2" t="s"><v>1</v></c></row>'
              '<row r="3"><c r="A3" t="str"><f>1+1</f><v>cached</v></c></row>'
              "</sheetData></worksheet>")
    content_types = ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
                     '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
                     '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
                     '<Default Extension="xml" ContentType="application/xml"/>'
                     '<Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/>'
                     '<Override PartName="/xl/worksheets/sheet1.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>'
                     '<Override PartName="/xl/worksheets/sheet2.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>'
                     '<Override PartName="/xl/sharedStrings.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sharedStrings+xml"/>'
                     "</Types>")
    root_rels = ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
                 '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
                 '<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/>'
                 "</Relationships>")
    with zipfile.ZipFile(FIXTURES / "shared-formula.xlsx", "w", zipfile.ZIP_DEFLATED) as archive:
        archive.writestr("[Content_Types].xml", content_types)
        archive.writestr("_rels/.rels", root_rels)
        archive.writestr("xl/workbook.xml", workbook)
        archive.writestr("xl/_rels/workbook.xml.rels", rels)
        archive.writestr("xl/sharedStrings.xml", shared)
        archive.writestr("xl/worksheets/sheet1.xml", sheet1)
        archive.writestr("xl/worksheets/sheet2.xml", sheet2)


def xls_workbook():
    import xlwt

    book = xlwt.Workbook(encoding="utf-8")
    sheet = book.add_sheet("名单")
    for column, value in enumerate(HEADER):
        sheet.write(0, column, value)
    for index, row in enumerate(ROWS, start=1):
        for column, value in enumerate(row):
            sheet.write(index, column, value)
    book.save(str(FIXTURES / "中文 旧格式.xls"))


def docx_files():
    from docx import Document

    document = Document()
    for name in NAMES:
        document.add_paragraph(name)
    document.save(FIXTURES / "段落.docx")

    table_document = Document()
    table_document.add_paragraph("高二2班学生名单")
    table = table_document.add_table(rows=1, cols=3)
    for column, value in enumerate(HEADER):
        table.rows[0].cells[column].text = value
    for row in ROWS:
        cells = table.add_row().cells
        for column, value in enumerate(row):
            cells[column].text = value
    table_document.save(FIXTURES / "表格.docx")


def unsafe_files():
    (FIXTURES / "broken.xlsx").write_bytes(b"PK\x03\x04this is not a zip archive" + bytes(range(64)))

    with zipfile.ZipFile(FIXTURES / "bomb.docx", "w", zipfile.ZIP_DEFLATED) as archive:
        archive.writestr("word/document.xml", "<document><body><p><t>张三</t></p></body></document>")
        archive.writestr("word/media/blob.bin", b"\0" * (20 * 1024 * 1024))

    with zipfile.ZipFile(FIXTURES / "dtd.docx", "w", zipfile.ZIP_DEFLATED) as archive:
        archive.writestr("word/document.xml",
                         '<?xml version="1.0"?><!DOCTYPE document SYSTEM "http://example.com/evil.dtd">'
                         "<document><body><p><t>张三</t></p></body></document>")


def main():
    FIXTURES.mkdir(parents=True, exist_ok=True)
    text_files()
    csv_files()
    xlsx_workbooks()
    shared_formula_xlsx()
    xls_workbook()
    docx_files()
    unsafe_files()
    for path in sorted(FIXTURES.iterdir()):
        print("%9d  %s" % (path.stat().st_size, path.name))


if __name__ == "__main__":
    main()