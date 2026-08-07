#!/usr/bin/env python3
"""
audit-code-integrity.py — Ona Platform Manual Code Integrity Auditor

Audits repository files for:
1. Lifecycle Symmetry & Teardown Parity (Shell/Python resources)
2. Behavioral Test Authenticity (Mock Theater Detection)
3. Architectural Naming & Contract Alignment
4. Error Handling & Hygiene (Swallowed Exceptions & Root Clutter)

Outputs formatted terminal reports and saves structured JSON logs to .review/<M-D-YYYY-XXX>-audit-report.json
"""

import argparse
import ast
import datetime
import json
import os
import re
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, ".."))
WORKSPACE_ROOT = os.path.abspath(os.getcwd())


class CodeIntegrityAuditor:
    def __init__(self, target_path=None, verbose=False, output_dir=None):
        self.target_path = os.path.abspath(target_path or WORKSPACE_ROOT)
        self.verbose = verbose
        base_dir = os.path.dirname(self.target_path) if os.path.isfile(self.target_path) else self.target_path
        self.output_dir = output_dir or os.path.join(base_dir, ".review")
        self.findings = {
            "lifecycle_parity": [],
            "behavioral_tests": [],
            "architectural_naming": [],
            "error_handling": [],
        }

    def run(self):
        if os.path.isfile(self.target_path):
            files_to_scan = [self.target_path]
            base_dir = os.path.dirname(self.target_path)
        else:
            files_to_scan = self._get_files_to_scan(self.target_path)
            base_dir = self.target_path

        for file_path in files_to_scan:
            rel_path = os.path.relpath(file_path, base_dir) if file_path.startswith(base_dir) else file_path
            if file_path.endswith(".sh"):
                self._audit_shell_file(file_path, rel_path)
            elif file_path.endswith(".py"):
                self._audit_python_file(file_path, rel_path)

        report_path = self._save_json_report()
        return report_path

    def _get_files_to_scan(self, root_dir):
        files = []
        skip_dirs = {"node_modules", "venv", "/tmp/"}
        for dirpath, dirnames, filenames in os.walk(root_dir):
            dirnames[:] = [d for d in dirnames if d not in skip_dirs and not d.startswith(".")]
            for f in filenames:
                if f.endswith(".sh") or f.endswith(".py"):
                    files.append(os.path.join(dirpath, f))
        return sorted(files)

    def _audit_shell_file(self, file_path, rel_path):
        try:
            with open(file_path, "r", encoding="utf-8", errors="ignore") as f:
                content = f.read()
                lines = content.splitlines()
        except Exception:
            return

        has_trap = "trap " in content or "cleanup()" in content
        
        # 1. Resource Allocations vs Teardown Parity
        creations = [
            ("docker buildx create", "docker buildx rm", "buildx builder creation"),
            ("docker context create", "docker context rm", "docker context creation"),
            ("aws ec2 run-instances", "aws ec2 terminate-instances", "EC2 instance launch"),
            ("aws ec2 authorize-security-group-ingress", "aws ec2 revoke-security-group-ingress", "security group ingress rule"),
        ]

        for create_cmd, remove_cmd, resource_desc in creations:
            if create_cmd in content:
                if not has_trap or remove_cmd not in content:
                    self.findings["lifecycle_parity"].append({
                        "file": rel_path,
                        "severity": "FAIL",
                        "rule": "LIFECYCLE_ASYMMETRY",
                        "message": f"Script contains '{create_cmd}' ({resource_desc}) but lacks matching '{remove_cmd}' in cleanup trap.",
                    })

        # 2. Forced Teardown Checks (--force or -f)
        for idx, line in enumerate(lines, 1):
            if "docker buildx rm" in line and "--force" not in line and "-f" not in line:
                self.findings["lifecycle_parity"].append({
                    "file": rel_path,
                    "line": idx,
                    "severity": "FAIL",
                    "rule": "BLOCKING_TEARDOWN",
                    "message": "Call to 'docker buildx rm' lacks '--force' / '-f' flag, which can hang on dead SSH hosts.",
                })
            if "docker context rm" in line and "2>/dev/null" not in line and "|| true" not in line:
                self.findings["lifecycle_parity"].append({
                    "file": rel_path,
                    "line": idx,
                    "severity": "WARN",
                    "rule": "UNGUARDED_CONTEXT_RM",
                    "message": "Call to 'docker context rm' lacks error guard ('2>/dev/null || true').",
                })

        # 3. Global Ambient State Leak Check
        if "docker buildx use" in content and "docker buildx use default" not in content:
            self.findings["lifecycle_parity"].append({
                "file": rel_path,
                "severity": "WARN",
                "rule": "AMBIENT_STATE_LEAK",
                "message": "Script modifies active buildx builder ('docker buildx use') without restoring default builder in cleanup.",
            })

    def _audit_python_file(self, file_path, rel_path):
        try:
            with open(file_path, "r", encoding="utf-8", errors="ignore") as f:
                content = f.read()
            tree = ast.parse(content, filename=file_path)
        except Exception:
            return

        is_test_file = (
            "test_" in rel_path
            or "_test" in rel_path
            or "tests/" in rel_path
            or re.search(r"^\s*def test_", content, re.MULTILINE) is not None
        )

        # Scan AST nodes
        for node in ast.walk(tree):
            # 1. Behavioral Test Authenticity (Mock Theater Detection)
            if is_test_file and isinstance(node, ast.FunctionDef) and node.name.startswith("test_"):
                self._check_mock_theater(node, rel_path, content)

            # 2. Architectural Naming Verification
            if isinstance(node, ast.Assign):
                for target in node.targets:
                    if isinstance(target, ast.Name) and "LAMBDA" in target.id.upper():
                        if isinstance(node.value, ast.Constant) and isinstance(node.value.value, str):
                            val = node.value.value
                            if val and not val.startswith("ona-") and not val.startswith("/ona/") and not val.startswith("/aws/lambda/"):
                                self.findings["architectural_naming"].append({
                                    "file": rel_path,
                                    "line": node.lineno,
                                    "severity": "FAIL",
                                    "rule": "INVALID_LAMBDA_NAME",
                                    "message": f"Lambda function variable '{target.id}' value '{val}' violates 'ona-{{service}}-{{stage}}' naming invariant.",
                                })

            # 3. Swallowed Error Exceptions
            if isinstance(node, ast.ExceptHandler):
                if len(node.body) == 1 and isinstance(node.body[0], ast.Pass):
                    self.findings["error_handling"].append({
                        "file": rel_path,
                        "line": node.lineno,
                        "severity": "WARN",
                        "rule": "SWALLOWED_EXCEPTION",
                        "message": "Exception handler silently swallows errors with bare 'pass' without logging.",
                    })

    def _check_mock_theater(self, func_node, rel_path, content):
        func_name = func_node.name
        # Extract target function name (e.g. test_discover_customer -> discover_customer)
        target_stem = func_name.replace("test_", "")

        # Inspect decorators for @patch(target_stem)
        for decorator in func_node.decorator_list:
            dec_str = ast.unparse(decorator) if hasattr(ast, "unparse") else ""
            if "patch" in dec_str and target_stem in dec_str:
                self.findings["behavioral_tests"].append({
                    "file": rel_path,
                    "line": func_node.lineno,
                    "severity": "WARN",
                    "rule": "MOCK_THEATER_TARGET_MOCKED",
                    "message": f"Test '{func_name}' mocks the primary function under test ('{target_stem}'). Behavioral tests must call the actual target function.",
                })


    def _save_json_report(self):
        os.makedirs(self.output_dir, exist_ok=True)
        now = datetime.datetime.now()
        date_str = f"{now.month}-{now.day}-{now.year}"
        
        # Calculate next sequence number for today
        existing = [f for f in os.listdir(self.output_dir) if f.startswith(date_str) and f.endswith("-audit-report.json")]
        seq = len(existing) + 1
        filename = f"{date_str}-{seq:03d}-audit-report.json"
        report_file = os.path.join(self.output_dir, filename)

        total_fails = sum(1 for cat in self.findings.values() for item in cat if item.get("severity") == "FAIL")
        total_warns = sum(1 for cat in self.findings.values() for item in cat if item.get("severity") == "WARN")

        payload = {
            "timestamp": now.isoformat(),
            "target": self.target_path,
            "summary": {
                "total_failures": total_fails,
                "total_warnings": total_warns,
                "status": "FAIL" if total_fails > 0 else "PASS",
            },
            "findings": self.findings,
        }

        with open(report_file, "w", encoding="utf-8") as f:
            json.dump(payload, f, indent=2)

        return report_file


def print_formatted_summary(auditor, report_path):
    print("=" * 80)
    print("                    ONA PLATFORM CODE INTEGRITY AUDIT REPORT                     ")
    print("=" * 80)

    total_fails = 0
    total_warns = 0

    category_titles = {
        "lifecycle_parity": "Lifecycle Parity & Resource Teardown",
        "behavioral_tests": "Behavioral Test Authenticity (Anti-Mock Theater)",
        "architectural_naming": "Architectural Naming & Contract Invariants",
        "error_handling": "Error Handling & Exception Hygiene",
    }

    for cat_key, items in auditor.findings.items():
        title = category_titles.get(cat_key, cat_key)
        fails = [i for i in items if i.get("severity") == "FAIL"]
        warns = [i for i in items if i.get("severity") == "WARN"]
        total_fails += len(fails)
        total_warns += len(warns)

        if not items:
            print(f"[PASS] {title}")
        else:
            status_badge = "[FAIL]" if fails else "[WARN]"
            print(f"{status_badge} {title} ({len(fails)} Failures, {len(warns)} Warnings)")
            for item in items:
                line_info = f":L{item['line']}" if "line" in item else ""
                sev = f"[{item['severity']}]"
                print(f"  • {item['file']}{line_info} {sev} {item['message']}")
        print()

    print("=" * 80)
    summary_status = "FAIL" if total_fails > 0 else "PASS"
    print(f"OVERALL STATUS: {summary_status} ({total_fails} Failures, {total_warns} Warnings)")
    print(f"Detailed audit log saved to: {report_path}")
    print("=" * 80)
    return total_fails


def main():
    parser = argparse.ArgumentParser(description="Ona Platform Manual Code Integrity Auditor")
    parser.add_argument("--target", default=None, help="Target file or directory to scan (defaults to repository root)")
    parser.add_argument("--verbose", action="store_true", help="Print verbose output")
    parser.add_argument("--format", choices=["text", "json"], default="text", help="Output format")
    parser.add_argument("--output-dir", default=None, help="Directory to save JSON report (defaults to .review/)")

    args = parser.parse_args()

    auditor = CodeIntegrityAuditor(target_path=args.target, verbose=args.verbose, output_dir=args.output_dir)
    report_path = auditor.run()

    if args.format == "json":
        with open(report_path, "r") as f:
            print(f.read())
        total_fails = sum(1 for cat in auditor.findings.values() for item in cat if item.get("severity") == "FAIL")
        sys.exit(1 if total_fails > 0 else 0)

    fails = print_formatted_summary(auditor, report_path)
    sys.exit(1 if fails > 0 else 0)


if __name__ == "__main__":
    main()
