#!/usr/bin/env python3
import os
import sys
import subprocess
import json
import urllib.request
import urllib.error

GREEN = "\033[0;32m"
RED = "\033[0;31m"
BOLD = "\033[1m"
RESET = "\033[0m"

def log_pass(msg):
    print(f"{GREEN}✓ PASS:{RESET} {msg}")

def log_fail(msg):
    print(f"{RED}✗ FAIL:{RESET} {msg}", file=sys.stderr)

def run_cmd(args, stdin_data=None):
    try:
        p = subprocess.Popen(
            args,
            stdin=subprocess.PIPE if stdin_data else None,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        stdout, stderr = p.communicate(input=stdin_data)
        return p.returncode, stdout.strip(), stderr.strip()
    except Exception as e:
        return -1, "", str(e)

def test_args_mode():
    print(f"\n{BOLD}── Test 1: One-shot Arguments Mode ──{RESET}")
    cmd = ["nehanda", "chat", "Reply with exactly: ARGS_OK"]
    print(f"Running: {' '.join(cmd)}")
    code, stdout, stderr = run_cmd(cmd)
    
    if code != 0:
        log_fail(f"Command exited with code {code}. Error: {stderr}")
        return False
        
    print(f"Model raw output: {repr(stdout)}")
    if "ARGS_OK" in stdout:
        log_pass("One-shot arguments response is correct and coherent.")
        return True
    else:
        log_fail(f"Response did not contain ARGS_OK. Output: {stdout}")
        return False

def test_pipe_mode():
    print(f"\n{BOLD}── Test 2: One-shot Piped Mode ──{RESET}")
    cmd = ["nehanda"]
    input_str = "Reply with exactly: PIPE_OK"
    print(f"Running: echo '{input_str}' | {' '.join(cmd)}")
    code, stdout, stderr = run_cmd(cmd, stdin_data=input_str)
    
    if code != 0:
        log_fail(f"Command exited with code {code}. Error: {stderr}")
        return False
        
    print(f"Model raw output: {repr(stdout)}")
    if "PIPE_OK" in stdout:
        log_pass("One-shot piped response is correct and coherent.")
        return True
    else:
        log_fail(f"Response did not contain PIPE_OK. Output: {stdout}")
        return False

def test_http_api():
    print(f"\n{BOLD}── Test 3: aichat TUI Backend Loopback (OpenAI HTTP API) ──{RESET}")
    
    # 1. Locate aichat config.yaml
    home = os.path.expanduser("~")
    mac_path = os.path.join(home, "Library/Application Support/aichat/config.yaml")
    linux_path = os.path.join(home, ".config/aichat/config.yaml")
    
    config_path = mac_path if os.path.exists(mac_path) else linux_path
    if not os.path.exists(config_path):
        log_fail(f"aichat config.yaml not found at {mac_path} or {linux_path}")
        return False
        
    print(f"Reading configuration from: {config_path}")
    
    # 2. Extract port and key manually
    port = 8740  # default
    api_key = None
    
    try:
        with open(config_path, "r") as f:
            for line in f:
                line = line.strip()
                if "api_base:" in line:
                    # e.g. api_base: http://127.0.0.1:8740/v1
                    if "127.0.0.1:" in line:
                        port = int(line.split("127.0.0.1:")[1].split("/")[0])
                    elif "localhost:" in line:
                        port = int(line.split("localhost:")[1].split("/")[0])
                elif "api_key:" in line:
                    # e.g. api_key: "token..." or api_key: token...
                    api_key = line.split("api_key:")[1].strip().strip('"').strip("'")
    except Exception as e:
        log_fail(f"Error parsing config.yaml: {e}")
        return False
        
    if not api_key:
        log_fail("Could not extract api_key (bearer token) from config.yaml")
        return False
        
    url = f"http://127.0.0.1:{port}/v1/chat/completions"
    print(f"Found loopback endpoint: {url}")
    
    # 3. Build OpenAI Completions payload
    payload = {
        "model": "nehanda",
        "messages": [
            {"role": "user", "content": "Reply with exactly: HTTP_OK"}
        ]
    }
    
    req_body = json.dumps(payload).encode("utf-8")
    
    req = urllib.request.Request(
        url,
        data=req_body,
        headers={
            "Content-Type": "application/json",
            "Authorization": f"Bearer {api_key}"
        },
        method="POST"
    )
    
    try:
        with urllib.request.urlopen(req) as resp:
            resp_body = resp.read().decode("utf-8")
            data = json.loads(resp_body)
            print(f"Loopback raw response: {resp_body}")
            
            # 4. Assert response formatting and coherence
            content = data["choices"][0]["message"]["content"].strip()
            print(f"Model raw output: {repr(content)}")
            
            if "HTTP_OK" in content:
                log_pass("TUI loopback OpenAI HTTP API behaves correctly and returns coherent text.")
                return True
            else:
                log_fail(f"Response did not contain HTTP_OK. Content: {content}")
                return False
    except urllib.error.URLError as e:
        log_fail(f"HTTP request to {url} failed: {e}")
        return False
    except Exception as e:
        log_fail(f"Unexpected error validating HTTP loopback: {e}")
        return False

def main():
    print(f"{BOLD}================================================{RESET}")
    print(f"{BOLD}      NEHANDA-CLI USER EXPERIENCE TEST SUITE      {RESET}")
    print(f"{BOLD}================================================{RESET}")
    
    success = True
    success &= test_args_mode()
    success &= test_pipe_mode()
    success &= test_http_api()
    
    print(f"\n{BOLD}================================================{RESET}")
    if success:
        print(f"{GREEN}{BOLD}            ALL USER WORKFLOW TESTS PASSED        {RESET}")
        sys.exit(0)
    else:
        print(f"{RED}{BOLD}            SOME TESTS FAILED                     {RESET}")
        sys.exit(1)

if __name__ == "__main__":
    main()
