#!/usr/bin/env node
/**
 * nehanda-ui.mjs - Terminal UI for Nehanda CLI
 * 
 * Refactored to support dynamic persona loading:
 * - Reads agent configuration from agents.json
 * - Passes system prompts from agent config in the messages array
 * - Supports multiple agent personas via model selection
 * - Includes SEARCH/REPLACE diff parser for instant file edits
 */

import * as readline from 'readline';
import { spawn } from 'child_process';
import { createRequire } from 'module';
import { readFileSync, writeFileSync, existsSync } from 'fs';
import { homedir } from 'os';
import { join, resolve, isAbsolute } from 'path';

const require = createRequire(import.meta.url);

// Configuration paths
const CONFIG_DIR = join(homedir(), '.config', 'aimee');
const AGENTS_CONFIG_PATH = join(CONFIG_DIR, 'agents.json');
const PERSONAS_DIR = join(CONFIG_DIR, 'personas');

/**
 * Load agent configuration from agents.json
 */
function loadAgentConfig() {
  try {
    if (existsSync(AGENTS_CONFIG_PATH)) {
      const content = readFileSync(AGENTS_CONFIG_PATH, 'utf-8');
      return JSON.parse(content);
    }
  } catch (err) {
    console.error('Warning: Could not load agents.json:', err.message);
  }
  return { default_agent: 'nehanda', agents: [] };
}

/**
 * Load a persona file for a given agent name
 * Looks for {agent_name}.md or {agent_name}.txt in the personas directory
 */
function loadPersona(agentName) {
  const extensions = ['.md', '.txt'];
  for (const ext of extensions) {
    const personaPath = join(PERSONAS_DIR, `${agentName}${ext}`);
    try {
      if (existsSync(personaPath)) {
        return readFileSync(personaPath, 'utf-8');
      }
    } catch (err) {
      // Continue to next extension
    }
  }
  return null;
}

/**
 * Get the system prompt for an agent
 * Priority: 
 * 1. Agent's system_prompt field from config
 * 2. Persona file from personas directory
 * 3. null (let server handle fallback)
 */
function getSystemPrompt(agent) {
  // Check if agent has explicit system_prompt in config
  if (agent && agent.system_prompt && agent.system_prompt.trim()) {
    return agent.system_prompt;
  }
  
  // Try to load from personas directory
  if (agent && agent.name) {
    const persona = loadPersona(agent.name);
    if (persona) {
      return persona;
    }
  }
  
  // Return null - server will use its fallback chain
  return null;
}

/**
 * Find an agent by name from the config
 */
function findAgent(config, agentName) {
  if (!agentName || agentName === 'aimee') {
    // Use default agent
    const defaultName = config.default_agent || 'nehanda';
    return config.agents.find(a => a.name === defaultName) || { name: defaultName };
  }
  return config.agents.find(a => a.name === agentName) || { name: agentName };
}

/**
 * Build the messages array with optional system prompt
 */
function buildMessages(prompt, systemPrompt) {
  const messages = [];
  
  // Add system message if provided
  if (systemPrompt) {
    messages.push({ role: 'system', content: systemPrompt });
  }
  
  // Add user message
  messages.push({ role: 'user', content: prompt });
  
  return messages;
}

/**
 * Parse and apply SEARCH/REPLACE diff blocks from text
 * 
 * Supports Aider-style format:
 * 
 * filepath/to/file.ext
 * <<<<<<< SEARCH
 * [original lines to replace]
 * =======
 * [new replacement lines]
 * >>>>>>> REPLACE
 * 
 * @param {string} text - Text containing diff blocks
 * @returns {Object} - Results with count of applied diffs and any errors
 */
function applyDiffBlocks(text) {
  const results = { applied: 0, failed: 0, errors: [] };
  
  // Regex to match diff blocks
  // Captures: filepath, search content, replace content
  const diffRegex = /^(.+?\.(?:\w+))\s*\n<<<<<<< SEARCH\n([\s\S]*?)\n?=======\n([\s\S]*?)\n?>>>>>>> REPLACE/gm;
  
  let match;
  while ((match = diffRegex.exec(text)) !== null) {
    const [, filepath, searchContent, replaceContent] = match;
    
    try {
      // Resolve filepath (handle relative and absolute paths)
      const targetPath = isAbsolute(filepath) 
        ? filepath 
        : resolve(process.cwd(), filepath);
      
      // Check if file exists
      if (!existsSync(targetPath)) {
        results.failed++;
        results.errors.push(`✗ File not found: ${filepath}`);
        continue;
      }
      
      // Read the file
      const fileContent = readFileSync(targetPath, 'utf-8');
      
      // Check if search content exists in file
      if (!fileContent.includes(searchContent)) {
        results.failed++;
        results.errors.push(`✗ Search block not found in: ${filepath}`);
        continue;
      }
      
      // Perform the replacement
      const newContent = fileContent.replace(searchContent, replaceContent);
      
      // Write back to disk
      writeFileSync(targetPath, newContent, 'utf-8');
      
      results.applied++;
      console.log(`✓ Applied diff to ${filepath}`);
      
    } catch (err) {
      results.failed++;
      results.errors.push(`✗ Error processing ${filepath}: ${err.message}`);
    }
  }
  
  // Log summary if any diffs were found
  if (results.applied > 0 || results.failed > 0) {
    console.log(`\nDiff summary: ${results.applied} applied, ${results.failed} failed`);
    if (results.errors.length > 0) {
      results.errors.forEach(e => console.log(e));
    }
  }
  
  return results;
}

/**
 * Make a chat completion request
 */
async function chatCompletion(messages, model = 'nehanda', stream = true) {
  const body = { model, messages, stream };
  
  return new Promise((resolve, reject) => {
    const url = 'http://localhost:8080/v1/chat/completions';
    
    const curl = spawn('curl', [
      '-s', '-X', 'POST',
      '-H', 'Content-Type: application/json',
      '-d', JSON.stringify(body),
      url
    ]);
    
    let output = '';
    let error = '';
    
    curl.stdout.on('data', (data) => {
      output += data.toString();
    });
    
    curl.stderr.on('data', (data) => {
      error += data.toString();
    });
    
    curl.on('close', (code) => {
      if (code === 0) {
        try {
          resolve(JSON.parse(output));
        } catch (e) {
          resolve({ content: output });
        }
      } else {
        reject(new Error(`curl failed: ${error || output}`));
      }
    });
  });
}

/**
 * Extract content from a chat completion response
 */
function extractContent(response) {
  if (response.choices && response.choices[0] && response.choices[0].message) {
    return response.choices[0].message.content;
  }
  if (response.content) {
    return response.content;
  }
  return JSON.stringify(response);
}

/**
 * Main interactive loop
 */
async function main() {
  const config = loadAgentConfig();
  const defaultAgent = findAgent(config, config.default_agent);
  
  console.log('Nehanda CLI - Terminal UI');
  console.log(`Default agent: ${defaultAgent.name}`);
  console.log('Type /help for commands, Ctrl+C to exit');
  console.log('');
  
  const rl = readline.createInterface({
    input: process.stdin,
    output: process.stdout
  });
  
  let currentAgent = defaultAgent;
  let conversationHistory = [];
  
  const prompt = () => {
    rl.question(`[${currentAgent.name}] > `, async (input) => {
      const trimmed = input.trim();
      
      if (!trimmed) {
        prompt();
        return;
      }
      
      // Handle commands
      if (trimmed.startsWith('/')) {
        const [cmd, ...args] = trimmed.slice(1).split(/\s+/);
        
        switch (cmd.toLowerCase()) {
          case 'help':
            console.log('Commands:');
            console.log('  /help          - Show this help');
            console.log('  /model <name>  - Switch to a different agent');
            console.log('  /models        - List available agents');
            console.log('  /clear         - Clear conversation history');
            console.log('  /system        - Show current system prompt');
            console.log('  /quit          - Exit the program');
            break;
            
          case 'model':
            const newAgentName = args[0];
            if (!newAgentName) {
              console.log('Usage: /model <agent_name>');
            } else {
              const newAgent = findAgent(config, newAgentName);
              if (newAgent) {
                currentAgent = newAgent;
                conversationHistory = [];
                console.log(`Switched to agent: ${currentAgent.name}`);
              } else {
                console.log(`Agent not found: ${newAgentName}`);
              }
            }
            break;
            
          case 'models':
            console.log('Available agents:');
            config.agents.forEach(a => {
              const marker = a.name === currentAgent.name ? ' (current)' : '';
              const isDefault = a.name === config.default_agent ? ' [default]' : '';
              console.log(`  - ${a.name}${marker}${isDefault}`);
            });
            if (!config.agents.some(a => a.name === 'aimee')) {
              console.log('  - aimee (built-in default)');
            }
            break;
            
          case 'clear':
            conversationHistory = [];
            console.log('Conversation history cleared.');
            break;
            
          case 'system':
            const systemPrompt = getSystemPrompt(currentAgent);
            if (systemPrompt) {
              console.log('Current system prompt:');
              console.log('---');
              console.log(systemPrompt.substring(0, 500) + (systemPrompt.length > 500 ? '...' : ''));
              console.log('---');
            } else {
              console.log('No explicit system prompt. Server will use default persona.');
            }
            break;
            
          case 'quit':
          case 'exit':
            console.log('Goodbye!');
            rl.close();
            return;
            
          default:
            console.log(`Unknown command: /${cmd}. Type /help for available commands.`);
        }
        
        prompt();
        return;
      }
      
      // Send message to agent
      try {
        const systemPrompt = getSystemPrompt(currentAgent);
        const messages = buildMessages(trimmed, systemPrompt);
        
        console.log('Thinking...');
        const response = await chatCompletion(messages, currentAgent.name, false);
        const content = extractContent(response);
        
        console.log(`\n${content}\n`);
        
        // Apply any SEARCH/REPLACE diff blocks in the response
        applyDiffBlocks(content);
        
        // Update conversation history
        conversationHistory.push({ role: 'user', content: trimmed });
        conversationHistory.push({ role: 'assistant', content });
        
      } catch (err) {
        console.error('Error:', err.message);
      }
      
      prompt();
    });
  };
  
  prompt();
}

// Run main
main().catch(err => {
  console.error('Fatal error:', err);
  process.exit(1);
});
