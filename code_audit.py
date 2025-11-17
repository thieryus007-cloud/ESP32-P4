import os
import re
import json
from collections import defaultdict, Counter

# Configuration
EXCLUDED_DIRS = {"Exemple"}
EXCLUDED_EXTS = {".md", ".MD", ".Md"}
C_EXTS = {".c", ".h", ".cpp", ".hpp", ".cc", ".cxx"}
PY_EXTS = {".py"}

DECISION_KEYWORDS = re.compile(r"\b(if|for|while|case|switch|catch|elif|else if)\b|\?|&&|\|\|")


def is_excluded(path):
    parts = os.path.normpath(path).split(os.sep)
    if any(part in EXCLUDED_DIRS for part in parts):
        return True
    _, ext = os.path.splitext(path)
    if ext in EXCLUDED_EXTS:
        return True
    return False


def strip_c_comments(text):
    result_lines = []
    in_block = False
    for line in text.splitlines():
        stripped = ""
        i = 0
        while i < len(line):
            if in_block:
                end = line.find("*/", i)
                if end == -1:
                    i = len(line)
                    continue
                i = end + 2
                in_block = False
                continue
            if line.startswith("/*", i):
                in_block = True
                i += 2
                continue
            if line.startswith("//", i):
                break
            stripped += line[i]
            i += 1
        if stripped.strip():
            result_lines.append(stripped)
    return result_lines


def strip_py_comments(text):
    lines = []
    for line in text.splitlines():
        code = line.split("#", 1)[0].strip()
        if code:
            lines.append(code)
    return lines


def count_complexity(lines):
    return sum(len(DECISION_KEYWORDS.findall(line)) for line in lines)


def analyze_file(path):
    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        content = f.read()
    _, ext = os.path.splitext(path)

    if ext in C_EXTS:
        code_lines = strip_c_comments(content)
    elif ext in PY_EXTS:
        code_lines = strip_py_comments(content)
    else:
        code_lines = [ln for ln in (ln.strip() for ln in content.splitlines()) if ln]

    total_lines = len(content.splitlines())
    code_count = len(code_lines)
    complexity = count_complexity(code_lines) + 1 if code_lines else 0

    return {
        "total_lines": total_lines,
        "code_lines": code_count,
        "complexity": complexity,
    }


def main():
    root = os.path.abspath(os.path.dirname(__file__))
    stats_by_ext = defaultdict(lambda: {"files": 0, "total_lines": 0, "code_lines": 0, "complexity": 0})
    file_stats = {}

    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in EXCLUDED_DIRS and not d.startswith('.git')]
        for filename in filenames:
            rel_path = os.path.relpath(os.path.join(dirpath, filename), root)
            if is_excluded(rel_path):
                continue
            path = os.path.join(root, rel_path)
            analysis = analyze_file(path)
            _, ext = os.path.splitext(filename)
            key = ext or "<no_ext>"
            ext_stat = stats_by_ext[key]
            ext_stat["files"] += 1
            ext_stat["total_lines"] += analysis["total_lines"]
            ext_stat["code_lines"] += analysis["code_lines"]
            ext_stat["complexity"] += analysis["complexity"]
            file_stats[rel_path] = analysis

    overall = {
        "files": len(file_stats),
        "total_lines": sum(s["total_lines"] for s in file_stats.values()),
        "code_lines": sum(s["code_lines"] for s in file_stats.values()),
        "complexity": sum(s["complexity"] for s in file_stats.values()),
    }

    top_complexity = Counter({path: data["complexity"] for path, data in file_stats.items()})
    report = {
        "overall": overall,
        "by_extension": stats_by_ext,
        "top_complexity": top_complexity.most_common(10),
    }

    output_path = os.path.join(root, "code_audit_report.json")
    with open(output_path, "w", encoding="utf-8") as f:
        json.dump(report, f, indent=2)

    print(json.dumps(report, indent=2))
    print(f"\nAudit saved to {output_path}")


if __name__ == "__main__":
    main()
