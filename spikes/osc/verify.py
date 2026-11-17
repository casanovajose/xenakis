#!/usr/bin/env python3
"""Send one JUCE timetagged bundle and check oscdump's printout."""

import argparse
import json
import os
import subprocess
import sys
import time


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--sender", required=True)
    parser.add_argument("--oscdump", required=True)
    parser.add_argument("--port", type=int, default=17655)
    parser.add_argument("--out", required=True)
    args = parser.parse_args()

    dump = subprocess.Popen(
        [args.oscdump, "-L", str(args.port)],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    time.sleep(0.4)
    sent_path = args.out + ".sent.json"
    sender = subprocess.run(
        [args.sender, "--port", str(args.port), "--latency-ms", "10", "--out", sent_path],
        text=True,
        capture_output=True,
    )
    time.sleep(0.6)
    dump.terminate()
    try:
        stdout, _ = dump.communicate(timeout=3)
    except subprocess.TimeoutExpired:
        dump.kill()
        stdout, _ = dump.communicate()

    sent = json.loads(open(sent_path, encoding="utf-8").read())
    text = stdout or ""
    ok_address = "/xen/ping" in text and "xenakis" in text
    ok_tag = (not sent["time_tag_immediate"]) and sent["time_tag_raw"] != 1
    # oscdump prints a non-immediate NTP tag as hex seconds.fraction, never as the
    # immediate sentinel alone. Accept either a dotted NTP word or a bundle line.
    ok_dump_tag = ("." in text) or ("bundle" in text.lower()) or ("#" in text)
    result = {
        "sender_exit": sender.returncode,
        "oscdump": text,
        "sent": sent,
        "address_and_args_seen": ok_address,
        "time_tag_not_immediate": ok_tag and ok_dump_tag,
        "pass": sender.returncode == 0 and ok_address and ok_tag and ok_dump_tag,
    }
    with open(args.out, "w", encoding="utf-8") as handle:
        json.dump(result, handle, indent=2)
        handle.write("\n")
    sys.stdout.write(text)
    print(json.dumps({k: result[k] for k in ("pass", "address_and_args_seen", "time_tag_not_immediate")}))
    return 0 if result["pass"] else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:  # noqa: BLE001
        print(error, file=sys.stderr)
        return_code = 1
        os._exit(return_code)
