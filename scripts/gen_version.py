#
# gen_version.py — PlatformIO pre-build script
#
# Genererer firmware/main/core/version.h fra version.json FØR hver kompilering,
# så den indbyggede GATEWAY_VERSION/GATEWAY_BUILD ALTID matcher version.json —
# uanset rækkefølgen af versionsbump og build. Forhindrer fejletiketterede
# OTA-binærer (jf. BUGS.md F4).
#
Import("env")  # noqa: F821  (leveres af PlatformIO)
import json
import os

proj = env["PROJECT_DIR"]
version_json = os.path.join(proj, "version.json")
version_h    = os.path.join(proj, "firmware", "main", "core", "version.h")

try:
    with open(version_json, "r", encoding="utf-8") as f:
        data = json.load(f)
except Exception as e:
    print("gen_version: KUNNE IKKE læse version.json:", e)
    Return()  # noqa: F821

version = str(data.get("version", "0.0.0"))
build   = str(data.get("build", "0000"))

content = (
    "#pragma once\n"
    "\n"
    "// AUTO-GENERERET fra version.json af scripts/gen_version.py ved hvert build.\n"
    "// Rediger IKKE manuelt — ret version.json i stedet.\n"
    '#define GATEWAY_VERSION "%s"\n'
    '#define GATEWAY_BUILD   "%s"\n'
) % (version, build)

old = None
if os.path.exists(version_h):
    with open(version_h, "r", encoding="utf-8") as f:
        old = f.read()

# Skriv kun ved ændring — undgår unødig recompilering.
if old != content:
    with open(version_h, "w", encoding="utf-8") as f:
        f.write(content)
    print("gen_version: version.h opdateret -> %s b%s" % (version, build))
else:
    print("gen_version: version.h allerede %s b%s" % (version, build))
