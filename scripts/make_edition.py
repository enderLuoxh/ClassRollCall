"""Generate a private, built-in roster translation unit from a local JSON file.

Input: {"id":"ExampleClass", "class_name":"Example class", "names":["Alice","Bob"]}
Generated source belongs under build/ and must not be committed with real names.
"""
import argparse
import json
from pathlib import Path
import re
import uuid


def generate(input_path, output_path):
    data = json.loads(Path(input_path).read_text(encoding="utf-8-sig"))
    key = data["id"]
    if not isinstance(key, str) or not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_-]{0,63}", key):
        raise ValueError("id must contain 1-64 ASCII letters, numbers, underscores or hyphens")
    name = data["class_name"]
    names = data["names"]
    if not isinstance(name, str) or not name.strip() or len(name) > 100:
        raise ValueError("class_name must contain 1-100 characters")
    if not isinstance(names, list) or not 1 <= len(names) <= 10000:
        raise ValueError("names must contain 1-10000 records")
    for value in [name, *names]:
        if not isinstance(value, str) or not value.strip() or any(c in value for c in "\0\r\n"):
            raise ValueError("Names must be nonempty strings without line breaks or NULs")
    def wide(value):
        return "L" + json.dumps(value, ensure_ascii=True)
    class_id = str(uuid.uuid5(uuid.NAMESPACE_URL, "ClassRollCall/" + key))
    roster = ",\n        ".join(wide(value) for value in names)
    source = f'''// Generated locally. Contains a private roster; do not publish.
#include "edition.hpp"
namespace crc::edition {{
const wchar_t* dataFolder() {{ return {wide('ClassRollCall-' + key)}; }}
const wchar_t* mutexName() {{ return {wide('Local' + chr(92) + 'ClassRollCall.Native.' + key)}; }}
const wchar_t* floatingClass() {{ return {wide('ClassRollCall.Native.Float.' + key)}; }}
const wchar_t* displayName() {{ return {wide(name + '点名器')}; }}
bool seed(State& state) {{
    if(!state.classes.empty()) return false;
    ClassRoom value{{{wide(class_id)}, {wide(name)}, {{
        {roster}
    }}}};
    state.current=value.id;
    state.classes.push_back(std::move(value));
    state.initialized=true;
    return true;
}}
}}
'''
    target = Path(output_path)
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(source, encoding="utf-8")
    return key, len(names)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input")
    parser.add_argument("output")
    args = parser.parse_args()
    key, count = generate(args.input, args.output)
    print(f"Generated {key}: {count} records")
