#!/usr/bin/env python3
"""Generate the domain and SDK catalog schema from the host's canonical registry."""
import json
import pathlib
import re
import subprocess
import sys
root = pathlib.Path(__file__).resolve().parent.parent
host = sys.argv[1] if len(sys.argv) > 1 else str(root / "build/lmspace")
lines = subprocess.check_output([host, "--lmcode-signatures"], text=True).splitlines()
types, functions, context = {}, {}, {}
pattern = re.compile(r"^(?:fn (\w+)|method (\w+)\.(\w+))\((.*?)\) -> (\w+)$")
for line in lines:
    if line.startswith("context "):
        name, typ = line[8:].split(": ")
        context[name] = typ
        continue
    match = pattern.fullmatch(line)
    if not match:
        raise SystemExit("unparsable registry signature: " + line)
    fn, typ, method, params, returns = match.groups()
    entry = {"params": params.split(", ") if params else [], "returns": returns}
    if fn:
        functions.setdefault(fn, []).append(entry)
    else:
        types.setdefault(typ, {"methods": {}})["methods"].setdefault(method, []).append(entry)
schema = {"dsl.schema": 2, "name": "lmspace", "types": types, "functions": functions, "context": context}
raw = (json.dumps(schema, indent=2) + "\n").encode("utf-8")
merged = subprocess.run([sys.executable, str(root / "scripts/merge-std.py"), "--host-only", "-", "-"], input=raw, capture_output=True, check=True)
with open(root / "schema/lmspace.host.json", "wb") as output:
    output.write(merged.stdout)
