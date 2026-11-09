import json
import shlex
import subprocess


with open("build/compile_commands.json") as source:
    commands = [entry["command"] for entry in json.load(source)
                if "CMakeFiles/ixwebsocket.dir/" in entry["command"]]
assert commands, "No library compilation commands found"
for command in commands:
    standards = [arg for arg in shlex.split(command) if arg.startswith("-std=")]
    assert standards == ["-std=c++11"], command

version = subprocess.check_output(["./build/ws/ws", "--version"], text=True)
print(version, end="")
assert "ssl/Network.framework" in version, version
dependencies = subprocess.check_output(["otool", "-L", "build/ws/ws"], text=True)
assert "libssl" not in dependencies and "libcrypto" not in dependencies, dependencies
