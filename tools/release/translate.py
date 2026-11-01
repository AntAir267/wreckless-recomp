#!/usr/bin/env python3
"""translate.py XBE OUT: default.xbe to C in OUT/gen, the steps of regen.sh, from the
download's own copy of the toolkit's tools (BUNDLE/antbox) and this game's config
(BUNDLE/config). Exit 3: the result isn't what the prebuilt runtime was built against
(BUNDLE/GEN_SHA256). --hash prints the hash of OUT/gen and stops."""
import hashlib, os, subprocess, sys
from pathlib import Path

BUNDLE = Path(__file__).resolve().parent


def gen_hash(gen):
    h = hashlib.sha256()
    for f in sorted(p for p in gen.iterdir() if p.is_file()):
        h.update(f.name.encode() + b"\0" + f.read_bytes())
    return h.hexdigest()


def tool(*args, log):
    with open(log, "wb") as out:
        r = subprocess.run([sys.executable, "-E", "-s", "-B", *map(str, args)],
                           cwd=BUNDLE / "antbox", stdout=out, stderr=subprocess.STDOUT)
    if r.returncode:
        sys.stdout.write(Path(log).read_text(errors="replace")[-1500:])
        sys.exit(r.returncode)


def main():
    a = [x for x in sys.argv[1:] if x != "--hash"]
    xbe, out = a[0], Path(a[1]).resolve()
    xr, gen, cfg = out / "xr", out / "gen", BUNDLE / "config"
    xr.mkdir(parents=True, exist_ok=True)
    fns, strs, refs = xr / "disasm/functions.json", xr / "disasm/strings.json", xr / "disasm/xrefs.json"
    tool("-m", "tools.xbe_parser", xbe, "--json", xr / "analysis.json", "--quiet", log=xr / "parse.log")
    tool("-m", "tools.disasm", xbe, "--analysis-json", xr / "analysis.json",
         "--seed-functions", cfg / "seed_functions.json", "--seed-functions", cfg / "seeds_crosscheck.json",
         "-o", xr / "disasm", "-v", log=xr / "disasm.log")
    tool("tools/ghidra_naming/merge_names.py", "--apply", "--names-json", "tools/ghidra_naming/xdk_names.json",
         "--functions-json", fns, "--out", xr / "ghidra_names.json", log=xr / "names.log")
    tool("-m", "tools.func_id", xbe, "--functions", fns, "--strings", strs, "--xrefs", refs,
         "-o", xr / "func_id", log=xr / "func_id.log")
    tool("-m", "tools.abi_analysis", xbe, "--functions", fns, "--identified", xr / "func_id/identified_functions.json",
         "--output-dir", xr / "abi", log=xr / "abi.log")
    tool("-m", "tools.recomp", xbe, "--all", "--split", "250", "--disasm-dir", xr / "disasm",
         "--func-id-dir", xr / "func_id", "--abi-dir", xr / "abi", "--gen-dir", gen,
         "--manual-functions", cfg / "manual_functions.json", "--spin-waits", cfg / "spin_waits.json",
         "--exclude-manual", cfg / "recomp_hooks.c", "-o", xr / "recomp", log=xr / "recomp.log")
    have = gen_hash(gen)
    if "--hash" in sys.argv:
        print(have)
        return
    want = (BUNDLE / "GEN_SHA256").read_text().strip()
    if have != want:
        print(f"the translation is {have}, the runtime was built against {want}")
        sys.exit(3)


main()
