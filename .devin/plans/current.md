# Plan Draft

## AGENTS.md Review
No project-specific AGENTS.md exists in the nehanda-cli workspace. 

Global AGENTS.md constraints relevant to this task:
- **MANDATORY Workflow**: EXPLORE > PLAN > CONFIRM > CODE > VALIDATE > DEPLOY sequence must be followed
- **Validation Requirements**: Behavioral coverage threshold is mandatory - validate at least 70% of plausible behavioral paths/outcomes
- **Validation Order**: Behavioral verification first, then integration verification, then contract verification
- **Two-Tier Validation**: Both unit tests and E2E/integration tests must pass before completion
- **TDD Enforcement**: Tests must fail first before editing production code
- **Minimal Changes**: Implement only the minimal changes necessary to achieve the objective

## Objective
Fix the aimee-server build system to resolve SSL library dependency issues, then validate that the vision + tools combined fix works correctly.

## Scope
- Fix SSL library linking in the aimee-server build system
- Build aimee-server with the updated code
- Run the test-tools-vision.sh test suite to validate the vision + tools combined fix
- Verify that vision + tools requests succeed through /v1/chat/completions

## Success Criteria
✅ aimee-server builds successfully without SSL library errors
✅ test-tools-vision.sh test 4 (vision + tools combined) passes
✅ Image content is properly extracted and passed to the agent execution path
✅ Tools are called in the context of vision requests

## Completion Summary

### STATUS: ✅ COMPLETE — All objectives achieved

All tests passing, objective complete:
- ✅ Step 1: SSL linking fixed in Makefile
- ✅ Step 2: aimee-server built successfully (2.4MB binary)
- ✅ Step 3: Vision + tools validation complete (all 5 tests pass)

### Root Cause Analysis
The vision + tools parsing bug was caused by the `openai_parse_chat_request_with_images()` function in `src/server/openai_shape.c`. When processing multimodal message content arrays:
- The function correctly extracted text and image_url blocks
- However, it only set the `found = 1` flag when text content was processed
- When array content contained image_url blocks, the `found` flag remained 0
- This caused the function to return -1, triggering HTTP 400 "expected messages[] with content"

### Fix Applied
Added `found = 1;` after `image_count++;` in the image_url extraction block (line 265 of openai_shape.c).
This ensures that messages with image content are recognized as valid, even without text.

## Implementation Plan

### Step 1: Fix SSL library dependency in Makefile ✅
- Added explicit compilation rule for `$(OBJDIR)/aimee_tls.o` with SSL_CFLAGS (lines 1013-1017)
- Added `$(TLS_OBJS)` to `$(SERVER)` target (line 936)
- Added `$(TLS_OBJS)` to `$(KB)` target (line 950)
- Result: aimee-server builds without SSL linking errors

### Step 2: Build aimee-server ✅
- Ran `make server` successfully
- Final binary: `/Users/shingi/Workbench/nehanda-cli/upstream/aimee-server` (2.4MB)
- All object files compiled and linked successfully

### Step 3: Validate vision + tools combined fix ✅
- Ran test-tools-vision.sh test suite
- All 5 tests pass

## Validation

### Behavioral Coverage Matrix — Vision + Tools Combined

Scope:
- Vision + tools combined requests through `/v1/chat/completions`

Total applicable paths/outcomes: 5
Paths/outcomes validated: 5
Coverage: 100% ✅

| ID | Path/Outcome | Type (Primary/Edge/Failure) | Expected Behavior | Verification Method | Result |
|----|--------------|-----------------------------|-------------------|---------------------|--------|
| B1 | Wire-level Zod schema handling | Primary | Zod-polluted schema accepted by vLLM | test-tools-vision.sh test 1 | ✅ PASS |
| B2 | Server-level tool calls (turn 1) | Primary | Tool call through nehanda-server works | test-tools-vision.sh test 2 | ✅ PASS |
| B3 | Multi-turn continuity (turn 2) | Primary | Multi-turn works with tools | test-tools-vision.sh test 3 | ✅ PASS |
| B4 | Vision + tools request accepted | Primary | Request with image and tools succeeds | test-tools-vision.sh test 4 | ✅ PASS |
| B5 | Vision multi-turn | Primary | Continuation works after vision turn | test-tools-vision.sh test 5 | ✅ PASS |

## Objective Verification ✅
- ✅ aimee-server builds successfully with SSL library dependencies resolved
- ✅ test-tools-vision.sh test 1 passes (wire-level Zod schema)
- ✅ test-tools-vision.sh test 2 passes (server-level tools)
- ✅ test-tools-vision.sh test 3 passes (multi-turn)
- ✅ test-tools-vision.sh test 4 passes (vision + tools combined)
- ✅ test-tools-vision.sh test 5 passes (vision multi-turn)
- ✅ Image content is properly extracted from OpenAI multimodal format
- ✅ Images are passed through to the agent execution path
- ✅ Tools are called in the context of vision requests
- ✅ Multi-turn vision conversations work correctly

## Changes Made

### File: upstream/src/Makefile
- **Lines 1013-1017**: Added explicit compilation rule for `$(OBJDIR)/aimee_tls.o` with SSL_CFLAGS
  - Ensures TLS object compiles with OpenSSL include paths
  - Avoids conflict with multiple target-specific C_FLAGS overrides
- **Line 936**: Added `$(TLS_OBJS)` to `$(SERVER)` target
- **Line 950**: Added `$(TLS_OBJS)` to `$(KB)` target

### File: upstream/src/server/openai_shape.c
- **Line 265**: Added `found = 1;` after `image_count++;` in image_url extraction block
  - Sets the found flag when image_url content is successfully extracted
  - Ensures multimodal messages with images are recognized as valid

### Commits
- `4fe45a35` - Fix vision + tools parsing: set found=1 when image_url is extracted
