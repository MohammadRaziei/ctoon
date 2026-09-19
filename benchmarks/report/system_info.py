"""
system_info.py — a factual record of the machine these benchmarks ran
on: OS, CPU model, core count, total RAM, Python version. Linux-first
(reads /proc/cpuinfo and /proc/meminfo, which is where this suite is
actually run -- CI and most dev machines); falls back to whatever the
`platform` module can tell us elsewhere rather than guessing.

No interpretation, no comparison, no "this machine is fast/slow" --
just what a reader needs to judge whether these numbers transfer to
their own hardware.
"""
import argparse
import json
import os
import platform
import sys


def _linux_cpu_model():
    try:
        with open("/proc/cpuinfo", "r", encoding="utf-8") as f:
            for line in f:
                if line.lower().startswith("model name"):
                    return line.split(":", 1)[1].strip()
    except OSError:
        pass
    return None


def _linux_ram_gib():
    try:
        with open("/proc/meminfo", "r", encoding="utf-8") as f:
            for line in f:
                if line.startswith("MemTotal:"):
                    kib = int(line.split()[1])
                    return round(kib / (1024 * 1024), 1)
    except (OSError, ValueError, IndexError):
        pass
    return None


def collect():
    cpu_model = _linux_cpu_model() if sys.platform.startswith("linux") else None
    ram_gib = _linux_ram_gib() if sys.platform.startswith("linux") else None

    return {
        "os": f"{platform.system()} {platform.release()}",
        "platform": platform.platform(),
        "architecture": platform.machine(),
        "cpu_model": cpu_model or platform.processor() or "unknown",
        "logical_cores": os.cpu_count(),
        "ram_gib": ram_gib,
        "python_implementation": platform.python_implementation(),
        "python_version": platform.python_version(),
    }


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("output", help="path to write results JSON")
    args = p.parse_args()

    info = collect()

    with open(args.output, "w", encoding="utf-8") as f:
        json.dump(info, f, indent=2)

    print(
        f"system_info: {info['cpu_model']} ({info['logical_cores']} logical cores), "
        f"{info['ram_gib']}GiB RAM, {info['os']} -> {args.output}",
        file=sys.stderr,
    )
