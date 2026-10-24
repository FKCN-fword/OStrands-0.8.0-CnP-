#!/usr/bin/env python3
"""Lightweight static checks for the OStrands 0.8.0 P+ creature hybrid source.
This does not compile the DLL or prove in-game behavior.
"""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]

def read(rel: str) -> str:
    path = ROOT / rel
    if not path.is_file():
        raise AssertionError(f"missing required file: {rel}")
    return path.read_text(encoding="utf-8-sig", errors="strict")

checks: list[tuple[str, bool]] = []
cmake = read("CMakeLists.txt")
bridge = read("src/SexLabBridge.cpp")
fluid = read("src/FluidManager.cpp")
anatomy = read("src/Anatomy.cpp")
menu = read("src/SettingsMenu.cpp")
main = read("src/main.cpp")
build = read(".github/workflows/windows-build.yml")

checks.append(("project remains version 0.8.0", bool(re.search(r"project\(OStrands VERSION 0\.8\.0", cmake))))
checks.append(("10-second actor repeat guard remains", "constexpr float            kRepeat = 10.0f;" in bridge))
checks.append(("standalone SLSO.esp is not a runtime gate", 'LookupModByName("SLSO.esp")' not in bridge and "slsoInstalled" not in bridge))
checks.append(("SexLabBridge does not directly hook SKSE internal ModEvent.Send", "write_branch" not in bridge and "ModEvent.Send" not in bridge))
checks.append(("thread-controller counter is checked dynamically", "HasFunction(script->GetTypeInfo(), kCountFunction)" in bridge))
checks.append(("P+ alias counter call uses no arguments", "DispatchMethodCall(script, kCountFunction, RE::MakeFunctionArguments(), cb)" in bridge))
checks.append(("counter increase delta is calculated", "const auto delta = answer.count - entry->second;" in bridge))
checks.append(("counter reset updates baseline without effect", "entry->second = answer.count;" in bridge and "resynchronized without spawning an effect" in bridge))
checks.append(("unexpected large counter jump is guarded", "delta > kMaxCounterDelta" in bridge and "kMaxCounterDelta = 16" in bridge))
checks.append(("multi-count observations are coalesced", "multiple increments are coalesced" in bridge))
checks.append(("callback failure uses retry backoff", "answer.aliasRound != 0" in bridge and "scene->aliasRetryAfter = std::max" in bridge))
checks.append(("late alias poll callbacks are rejected", "scene->aliasRound != answer.aliasRound" in bridge))
checks.append(("creature genital-node fallback remains", "CreatureTipNode" in anatomy and "NPC Penis Tip" in anatomy and "names.shaft[i]" in anatomy))
start = fluid.find("void FluidManager::StartClimax")
end = fluid.find("void FluidManager::Plan", start)
climax_section = fluid[start:end] if start >= 0 and end > start else ""
checks.append(("fluid generation does not require a human actor", bool(climax_section) and "IsPerson(" not in climax_section))
checks.append(("host lists are only made when domains need creation", fluid.count("Build the host list only if a new fluid domain is required") == 2))
checks.append(("Korean alignment settings remain", "위치 보정" in menu and "BuildAlignment" in menu))
checks.append(("hybrid build marker is logged", "OStrands hybrid source" in main))
checks.append(("GitHub workflow builds and uploads one MO2-ready DLL artifact", "run: call build.cmd" in build and "build\\release\\0_OStrands.dll" in build and "SKSE\\Plugins" in build and "name: OStrands-0.8.0-Creature-PPlus-Hybrid-MO2" in build and "path: artifact/**" in build and "actions/upload-artifact@v4" in build))

failed = [name for name, passed in checks if not passed]
for name, passed in checks:
    print(f"{'PASS' if passed else 'FAIL'}: {name}")
print(f"\n{len(checks) - len(failed)}/{len(checks)} static checks passed")
if failed:
    sys.exit(1)
