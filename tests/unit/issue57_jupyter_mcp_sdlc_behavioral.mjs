/**
 * Issue #57 Behavioral Specification:
 * Integrate Datalayer Jupyter MCP into 11-step SDLC with Jev-Mem tracking.
 */
import test from 'node:test'
import assert from 'node:assert/strict'
import fs from 'node:fs'
import path from 'node:path'
import { fixture } from '../fixtures/sdlcAcceptance.mjs'
import { planningGateDeniesTool, getPhase, setPhase, getOriginalRedEvidence } from '../../lib/workflow.mjs'
import { executeBuiltinTool, EXPLORE_ONLY_TOOLS } from '../../lib/tools.mjs'
import { classifyUserIntent, heuristicAnswers, INTENT_QUESTIONS } from '../../lib/laya.mjs'

test('AC-1: Given diagnose, create_issue, or planning phase, when an MCP mutation or execution tool is checked, then planningGateDeniesTool denies it', () => {
  const f = fixture('ac1-mcp-gate', 'diagnose')
  try {
    const mutatingMcpTools = [
      'mcp__jupyter__insert_cell',
      'mcp__jupyter__edit_cell_source',
      'mcp__jupyter__overwrite_cell_source',
      'mcp__jupyter__move_cell',
      'mcp__jupyter__delete_cell',
      'mcp__jupyter__execute_cell',
      'mcp__jupyter__execute_code',
    ]

    for (const phase of ['diagnose', 'create_issue', 'planning', 'confirm_fix']) {
      setPhase(f.db, f.cid, phase)
      for (const tool of mutatingMcpTools) {
        assert.equal(
          planningGateDeniesTool(f.db, f.cid, tool),
          true,
          `Phase ${phase} must deny mutating/executing MCP tool ${tool}`
        )
      }
    }
  } finally {
    f.close()
  }
})

test('AC-2: Given explore phase, when tool admission is checked, then read-only MCP tools are permitted and mutating/executing MCP tools are denied', () => {
  const f = fixture('ac2-explore-mcp', 'explore')
  try {
    const readOnlyMcp = [
      'mcp__jupyter__read_notebook',
      'mcp__jupyter__read_cell',
      'mcp__jupyter__list_notebooks',
      'mcp__jupyter__list_kernels',
    ]
    const mutatingMcp = [
      'mcp__jupyter__insert_cell',
      'mcp__jupyter__edit_cell_source',
      'mcp__jupyter__execute_cell',
    ]

    for (const tool of readOnlyMcp) {
      assert.equal(
        planningGateDeniesTool(f.db, f.cid, tool),
        false,
        `explore phase must allow read-only MCP tool ${tool}`
      )
    }

    for (const tool of mutatingMcp) {
      assert.equal(
        planningGateDeniesTool(f.db, f.cid, tool),
        true,
        `explore phase must deny mutating MCP tool ${tool}`
      )
    }
  } finally {
    f.close()
  }
})

test('AC-3: Given explore/diagnose phase, when an MCP tool succeeds, then its observation is ingested into Jev-Mem', async () => {
  const f = fixture('ac3-mcp-jev-mem', 'explore')
  try {
    const ctx = {
      conversationId: f.cid,
      sessionId: f.sid,
      cwd: f.cwd,
      settings: {
        mcp_servers: {
          jupyter: {
            request: async (method, params) => {
              if (method === 'tools/call') {
                return {
                  result: {
                    content: [{ type: 'text', text: 'Notebook metadata and 3 cells' }],
                    isError: false,
                  }
                }
              }
              return { result: {} }
            }
          }
        }
      }
    }

    // Mock MCP server invoke response via mock dispatch
    const res = await executeBuiltinTool(f.db, ctx, 'mcp__jupyter__read_notebook', { path: 'analysis.ipynb' })
    // In current unpatched code, read-only MCP is rejected by EXPLORE_ONLY_TOOLS in explore
    assert.equal(res.is_error, false, 'Read-only MCP tool in explore should not fail with phase denial')

    // After success, a jev_observation memory node must exist for this tool
    const memories = f.db.prepare("SELECT * FROM memories WHERE conversation_id = ? AND provenance = 'explore_mcp'").all(f.cid)
    assert.ok(memories.length > 0, 'Jev-Mem must contain an observation with provenance explore_mcp')
  } finally {
    f.close()
  }
})

test('AC-4: Given a user prompt about Jupyter notebooks, when intent is classified, then notebook domain context is detected', async () => {
  const prompt = 'Please inspect the model training curves in notebooks/experiment_01.ipynb using the jupyter kernel'
  const answers = heuristicAnswers(INTENT_QUESTIONS, prompt)
  assert.equal(answers.task_domain?.choice, 'notebook_research', 'Laya heuristic should classify notebook task as notebook_research')
})

test('AC-5: Given create_tests phase, when mcp__jupyter__execute_cell executes a failing test cell, then RED test evidence is recorded in test_executions', async () => {
  const f = fixture('ac5-mcp-test-evidence', 'create_tests')
  try {
    // Setup test notebook file in tests/
    const testsDir = path.join(f.cwd, 'tests')
    fs.mkdirSync(testsDir, { recursive: true })
    const nbPath = path.join(testsDir, 'test_analysis.ipynb')
    fs.writeFileSync(nbPath, JSON.stringify({ cells: [] }), 'utf8')

    const ctx = {
      conversationId: f.cid,
      sessionId: f.sid,
      cwd: f.cwd,
      settings: {
        mcp_servers: {
          jupyter: {
            request: async (method, params) => {
              if (method === 'tools/call') {
                return {
                  result: {
                    content: [{ type: 'text', text: 'Traceback (most recent call last):\n  AssertionError: test failed' }],
                    isError: true,
                  }
                }
              }
              return { result: {} }
            }
          }
        }
      }
    }

    // Attempting to execute test cell in create_tests
    const res = await executeBuiltinTool(f.db, ctx, 'mcp__jupyter__execute_cell', {
      notebook_path: 'tests/test_analysis.ipynb',
      cell_index: 0,
    })

    const red = getOriginalRedEvidence(f.db, f.cid)
    assert.ok(red, 'RED test evidence must be recorded for failing notebook test execution')
  } finally {
    f.close()
  }
})
