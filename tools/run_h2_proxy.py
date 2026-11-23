#!/usr/bin/env python3
"""Build and run the isolated HTTP/2 WebSocket reverse-proxy integration test."""
import os
from pathlib import Path
import subprocess
import sys
import uuid

ROOT = Path(__file__).resolve().parents[1]


def main():
    output = ROOT / "build/h2-proxy"
    runs = output / "runs"
    runs.mkdir(parents=True, exist_ok=True)
    image_id = output / ("image-" + uuid.uuid4().hex)
    try:
        print("Building current IXWebSocket source and H2 proxy test image...", flush=True)
        with (output / "build.log").open("w") as log:
            subprocess.run(["docker", "build", "--file", str(ROOT / "docker/Dockerfile.h2-proxy"),
                            "--iidfile", str(image_id), str(ROOT)], check=True,
                           stdout=log, stderr=subprocess.STDOUT)
        command = ["docker", "run", "--rm", "--init", "--network", "none",
                   "--user", "{}:{}".format(os.getuid(), os.getgid()),
                   "--env", "PYTHONUNBUFFERED=1", "--volume",
                   str(runs) + ":/app/build/h2-proxy/runs",
                   image_id.read_text().strip()] + sys.argv[1:]
        return subprocess.run(command).returncode
    except (OSError, subprocess.CalledProcessError) as error:
        print("H2 proxy test failed: {}. Build log: {}".format(error, output / "build.log"),
              file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        return 130
    finally:
        image_id.unlink(missing_ok=True)
        print("Reports and service logs: " + str(runs), flush=True)


if __name__ == "__main__":
    raise SystemExit(main())
