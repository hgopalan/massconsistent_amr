# Agent Instructions

This directory contains default instruction files for AI agents and language models to assist with development on the **massconsistent_amr** project.

## Available Instructions

### [claude-instructions.md](./claude-instructions.md)
Instructions optimized for Claude (Anthropic). Emphasizes:
- Project context and architecture
- Development guidelines and best practices
- Code style and conventions
- Documentation requirements
- Task-oriented guidance

### [gemini-instructions.md](./gemini-instructions.md)
Instructions optimized for Gemini (Google). Emphasizes:
- Technical deep-dives and subsystem understanding
- Performance and scalability considerations
- Parallel computing specifics
- Development best practices
- Critical subsystem documentation

### [codex-instructions.md](./codex-instructions.md)
Instructions optimized for Codex (OpenAI). Emphasizes:
- Code structure and file organization
- Language and framework specifics
- Common development tasks
- Build and testing procedures
- Quality checklists

## How to Use

### Option 1: Manual Reference
When starting a conversation with an AI agent, reference the appropriate instruction file:
- Copy the relevant instruction content and paste it at the start of your conversation
- Or provide the file path/content as context

### Option 2: Automated Integration
If your IDE or tool supports it, you can:
- Configure your AI assistant to load instructions from `.github/agents/` automatically
- Set up pre-conversation scripts to inject the relevant instructions
- Create Git hooks to reference these files in commit messages or PR templates

### Option 3: Direct Link
Share the raw GitHub URL with the agent:
```
https://raw.githubusercontent.com/hgopalan/massconsistent_amr/main/.github/agents/claude-instructions.md
```

## Benefits

- **Consistency**: All agents start with the same project context
- **Efficiency**: Reduces time spent explaining the project structure
- **Quality**: Helps agents understand code patterns and best practices
- **Automation**: Can be automatically loaded by tools and IDEs

## Customization

Feel free to:
- Update these files as the project evolves
- Add model-specific optimizations
- Include additional context or guidelines
- Create new instruction files for other agents

## Version Control

These files are tracked in Git and should be:
- Reviewed as part of documentation updates
- Updated when major architectural changes occur
- Kept in sync with project structure changes in `docs/` and `README.md`

---

For more information about the massconsistent_amr project, see the [main README](../../README.md) or [GETTING_STARTED_TUTORIAL.md](../../GETTING_STARTED_TUTORIAL.md).
