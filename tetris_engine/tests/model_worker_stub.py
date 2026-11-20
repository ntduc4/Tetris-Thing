"""Deterministic stand-in for the Qt inference worker in UI tests."""

import json
import os
import sys
import time

print(json.dumps({"ready": True, "width": 10,
                  "height": int(os.environ.get("TETRIS_MODEL_STUB_HEIGHT", "10"))}),
      flush=True)
for line in sys.stdin:
    request = json.loads(line)
    with open(os.environ["TETRIS_MODEL_STUB_LOG"], "a", encoding="utf-8") as log:
        log.write(json.dumps(request) + "\n")
    time.sleep(float(os.environ.get("TETRIS_MODEL_STUB_DELAY", "0")))
    print(json.dumps({"index": int(os.environ.get("TETRIS_MODEL_STUB_INDEX", "0"))}),
          flush=True)
