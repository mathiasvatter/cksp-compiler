#!/usr/bin/env python3
import argparse
import os
import platform
import signal
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path
from typing import Optional, TextIO

def default_kontakt_path() -> str:
	"""Return a sensible default Kontakt executable path for the current OS."""
	if platform.system() == "Darwin":
		return "/Applications/Native Instruments/Kontakt 7/Kontakt 7.app/Contents/macOS/Kontakt 7"
	elif platform.system() == "Windows":
		return r"C:\Program Files\Native Instruments\Kontakt 7\Kontakt 7.exe"
	else:
		return ""

def make_lua(code_file: str) -> str:
	"""Create a minimal Lua script that loads 'code_file' into instrument 0, slot 0 (no os.exit)."""
	code_file = os.path.abspath(code_file)
	lua_path_literal = code_file.replace("\\", "\\\\").replace('"', '\\"')

	lua_source = f'''\
-- Minimal Kontakt Lua runner (generated)
local kt = Kontakt
local fs = Filesystem

-- Ensure instrument 0 exists
kt.add_instrument(0)

-- Injected by Python
local script_path = "{lua_path_literal}"

-- Read entire file
local file = io.open(script_path, "rb")
if not file then
	print("[error] could not open file: " .. script_path)
	return
end
local content = file:read("*a")
file:close()

-- Put KSP into instrument 0, slot 0
kt.set_instrument_script_source(0, 0, content)

print("[kontakt] script applied to instrument 0, slot 0")
-- Do NOT exit Kontakt here; Python manages lifetime.
'''
	tmp = tempfile.NamedTemporaryFile(delete=False, suffix=".lua", mode="w", encoding="utf-8")
	tmp.write(lua_source)
	tmp.flush()
	tmp.close()
	return tmp.name

def _stdout_pump(proc: subprocess.Popen, last_activity_ptr: list, error_flag: list, fout: Optional[TextIO]):
	"""Continuously read and forward stdout; update last_activity_ptr[0] when new data arrives; tee to fout if given."""
	try:
		for line in iter(proc.stdout.readline, ''):
			if not line:
				break
			# Console
			# print(line, end='')
			# File (tee)
			if fout is not None:
				try:
					fout.write(line)
					fout.flush()
				except Exception:
					pass
			last_activity_ptr[0] = time.time()
			if "[error]" in line.lower():
				error_flag[0] = True
	except Exception:
		pass

def run_kontakt_idle(kontakt_exec: str,
					 lua_file: str,
					 idle_timeout: float,
					 startup_grace: float,
					 hard_timeout: float,
					 grace_sec: float,
					 fout: Optional[TextIO]) -> int:
	"""Launch Kontakt, stop it when no new stdout has appeared for idle_timeout seconds."""
	if not os.path.exists(kontakt_exec):
		print(f"Error: Kontakt executable not found at '{kontakt_exec}'", file=sys.stderr)
		return 127

	# Separate process group to signal the whole app
	popen_kwargs = {}
	if platform.system() in ("Darwin", "Linux"):
		popen_kwargs["preexec_fn"] = os.setsid
	elif platform.system() == "Windows":
		popen_kwargs["creationflags"] = subprocess.CREATE_NEW_PROCESS_GROUP

	cmd = [kontakt_exec, lua_file]
	proc = subprocess.Popen(
		cmd,
		stdout=subprocess.PIPE,
		stderr=subprocess.STDOUT,
		text=True,
		bufsize=1,
		**popen_kwargs,
	)

	start_time = time.time()
	last_activity = [start_time]  # mutable cell for cross-thread update
	error_flag = [False]

	t = threading.Thread(target=_stdout_pump, args=(proc, last_activity, error_flag, fout), daemon=True)
	t.start()

	# Allow Kontakt to start before applying idle logic
	while proc.poll() is None and (time.time() - start_time) < startup_grace:
		time.sleep(0.05)

	# Main watchdog loop
	while proc.poll() is None:
		now = time.time()
		if now - start_time > hard_timeout:  # hard cap
			break
		if now - last_activity[0] > idle_timeout:  # idle
			break
		time.sleep(0.05)

	# Try graceful termination first (SIGTERM / TerminateProcess)
	if proc.poll() is None:
		try:
			if platform.system() in ("Darwin", "Linux"):
				os.killpg(proc.pid, signal.SIGTERM)
			else:
				proc.terminate()
		except Exception:
			pass

		try:
			proc.wait(timeout=grace_sec)
		except subprocess.TimeoutExpired:
			# Force kill
			try:
				if platform.system() in ("Darwin", "Linux"):
					os.killpg(proc.pid, signal.SIGKILL)
				else:
					proc.kill()
			except Exception:
				pass
			proc.wait()

	# Cleanup
	try:
		if proc.stdout:
			proc.stdout.close()
	except Exception:
		pass
	t.join(timeout=1.0)

	if error_flag[0]: # and proc.returncode == 0:
		return 1
	return proc.returncode

def main():
	parser = argparse.ArgumentParser(
		description="Run Kontakt with Lua, then auto-close when no new output appears for a given idle timeout."
	)
	parser.add_argument("--kontakt", type=str, default=default_kontakt_path(),
						help="Path to Kontakt executable (platform-dependent default).")
	parser.add_argument("--idle", type=float, default=0.5,
						help="Seconds of no new stdout before stopping Kontakt (default: 0.5).")
	parser.add_argument("--startup-grace", type=float, default=3.0,
						help="Seconds to wait before applying idle detection (default: 3.0).")
	parser.add_argument("--hard-timeout", type=float, default=30.0,
						help="Absolute cap in seconds; Kontakt is stopped regardless (default: 30).")
	parser.add_argument("--grace", type=float, default=2.0,
						help="Grace seconds after terminate before force-kill (default: 2.0).")
	parser.add_argument("--output", type=str, default=None,
						help="Optional file to write all Kontakt stdout to (overwrites existing file).")
	parser.add_argument("script_path", type=str,
						help="Path to the KSP text file (e.g. /path/to/code_output.txt).")
	args = parser.parse_args()

	if not args.kontakt:
		print("Error: No Kontakt path resolved. Provide --kontakt explicitly.", file=sys.stderr)
		sys.exit(2)
	if not os.path.exists(args.script_path):
		print(f"Error: File '{args.script_path}' does not exist.", file=sys.stderr)
		sys.exit(1)

	# Prepare optional output file
	fout: Optional[TextIO] = None
	try:
		if args.output:
			out_path = Path(args.output)
			out_path.parent.mkdir(parents=True, exist_ok=True)
			fout = open(out_path, "w", encoding="utf-8")

		lua_file = make_lua(args.script_path)
		try:
			rc = run_kontakt_idle(args.kontakt, lua_file,
								  idle_timeout=args.idle,
								  startup_grace=args.startup_grace,
								  hard_timeout=args.hard_timeout,
								  grace_sec=args.grace,
								  fout=fout)
		finally:
			try:
				Path(lua_file).unlink(missing_ok=True)
			except Exception:
				pass
	finally:
		if fout is not None:
			try:
				fout.close()
			except Exception:
				pass

	sys.exit(rc)

if __name__ == "__main__":
	main()
