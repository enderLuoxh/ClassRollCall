"""Development-only fixtures written by independent Office libraries, never shipped in the app."""
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
import csv
import io
import openpyxl
from docx import Document
import xlwt

root = Path(__file__).resolve().parents[1] / 'tests/fixtures/generated'
root.mkdir(parents=True, exist_ok=True)
names = ['张三', '李四', '张三', '王五']
for encoding in ['utf-8', 'utf-8-sig', 'gbk', 'gb18030', 'utf-16', 'utf-16-be']:
    data = '\n'.join(names) + '\n\n'
    if encoding == 'utf-16-be':
        (root / f'{encoding}.txt').write_bytes(b'\xfe\xff' + data.encode(encoding))
    else:
        (root / f'{encoding}.txt').write_bytes(data.encode(encoding))
rows = [['学号', '姓名', '性别', '班级']] + [[f'{i+1:03}', name, '女', '二班'] for i, name in enumerate(names)]
for encoding in ['utf-8-sig', 'gb18030']:
    stream = io.StringIO(newline='')
    csv.writer(stream).writerows(rows)
    (root / f'{encoding}.csv').write_bytes(stream.getvalue().encode(encoding))
(root / 'quoted.csv').write_text('学号;姓名;备注\n001;"张,三";"含;分号"\n002;"李""四";"多\n行"\n', encoding='utf-8')
book = openpyxl.Workbook()
sheet = book.active
sheet.title = '说明'
sheet.append(['这不是名单'])
sheet = book.create_sheet('高二 2班')
for row in rows:
    sheet.append(row)
sheet.append(['005', None, '男', '二班'])
book.save(root / '多列 多工作表.xlsx')
legacy = xlwt.Workbook(encoding='utf-8')
for title in ['第一班', '第二班']:
    sheet = legacy.add_sheet(title)
    for r, row in enumerate(rows):
        for c, value in enumerate(row):
            sheet.write(r, c, value)
legacy.save(str(root / '中文 旧格式.xls'))
doc = Document()
for name in names:
    doc.add_paragraph(name)
doc.save(root / '段落.docx')
doc = Document()
table = doc.add_table(rows=0, cols=4)
for row in rows:
    for cell, value in zip(table.add_row().cells, row):
        cell.text = value
doc.save(root / '表格.docx')
# Rewrite an independently generated workbook to exercise sharedStrings and cached formula values.
with ZipFile(root / '多列 多工作表.xlsx') as source, ZipFile(root / 'shared-formula.xlsx', 'w', ZIP_DEFLATED) as target:
    for info in source.infolist():
        data = source.read(info.filename)
        if info.filename == 'xl/worksheets/sheet2.xml':
            data = b'''<?xml version="1.0" encoding="UTF-8"?><worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><sheetData><row r="1"><c r="A1" t="s"><v>0</v></c></row><row r="2"><c r="A2" t="s"><v>1</v></c></row><row r="3"><c r="A3" t="str"><f>DO_NOT_EXECUTE()</f><v>cached</v></c></row></sheetData></worksheet>'''
        target.writestr(info.filename, data)
    target.writestr('xl/sharedStrings.xml', '<?xml version="1.0"?><sst xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><si><t>姓名</t></si><si><r><t>张</t></r><r><t>三</t></r></si></sst>')
(root / 'broken.xlsx').write_bytes(b'broken zip file')
with ZipFile(root / 'bomb.docx', 'w', ZIP_DEFLATED) as z:
    z.writestr('word/document.xml', b'A' * (16 * 1024 * 1024 + 1))
with ZipFile(root / 'dtd.docx', 'w', ZIP_DEFLATED) as z:
    z.writestr('word/document.xml', '<!DOCTYPE document [<!ENTITY xxe SYSTEM "file:///C:/Windows/win.ini">]><document><body><p><r><t>&xxe;</t></r></p></body></document>')
print(f'Generated {len(list(root.iterdir()))} real fixture files in {root}')
