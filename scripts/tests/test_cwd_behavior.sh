#!/bin/bash
# Test: Verify that nehanda-ui passes cwd correctly to the server
# This test verifies the full cwd handling chain

set -e

echo "=== Test: cwd behavior in nehanda-ui ==="

# Test 1: Check if nehanda-ui.mjs includes cwd in the request body
echo "Test 1: Checking if nehanda-ui.mjs includes cwd in request body..."
if grep -q "cwd: process.cwd()" scripts/nehanda-ui.mjs; then
    echo "PASS: cwd is included in the request body"
else
    echo "FAIL: cwd is NOT included in the request body"
    exit 1
fi

# Test 2: Check if openai_chat.c extracts and uses the cwd parameter
echo "Test 2: Checking if openai_chat.c handles cwd parameter..."
if grep -q "run_cmd_set_cwd" upstream/src/server/openai_chat.c; then
    echo "PASS: openai_chat.c uses run_cmd_set_cwd for cwd handling"
else
    echo "FAIL: openai_chat.c does NOT handle cwd parameter"
    echo "      The cwd in the request body is ignored by the OpenAI endpoint!"
    exit 1
fi

echo ""
echo "=== All tests passed ==="