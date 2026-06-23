# Agent Instructions

This project contains default instruction files for AI agents and language models to assist with development on the **massconsistent_amr** project.

## Available Instructions

### [CLAUDE.md](CLAUDE.md)
Instructions optimized for Claude (Anthropic). Emphasizes:
- Project context and architecture
- Development guidelines and best practices
- Code style and conventions
- Documentation requirements
- Task-oriented guidance

### [GEMINI.md](GEMINI.md)
Instructions optimized for Gemini (Google). Emphasizes:
- Technical deep-dives and subsystem understanding
- Performance and scalability considerations
- Parallel computing specifics
- Development best practices
- Critical subsystem documentation

### [copilot-instructions.md](.github/copilot-instructions.md)
Instructions optimized for Codex (OpenAI) and Copilot. Emphasizes:
- Code structure and file organization
- Language and framework specifics
- Common development tasks
- Build and testing procedures
- Quality checklists

## Default Development Guidelines

All agents must follow these guidelines when working on code changes:

1. **Documentation Organization**
   - Do not create stray .MD files
   - Always integrate documentation into proper existing sections (`docs/`, inline comments, existing README files)
   - Do not create top-level documentation unless explicitly specified

2. **Regression Tests**
   - Check if regtests exist for the feature/area being modified
   - If regtests don't exist, create them
   - All regtests must pass before submitting PR

3. **Code Comments & Documentation**
   - Include detailed comments explaining changes
   - Add citations where applicable (papers, references, issues)
   - Include date of code addition in comments
   - Reference related issues/PRs in commit messages and comments

4. **Professional Documentation**
   - Keep all documentation and comments professional
   - Avoid informal agent conversations (e.g., "Feature 1", "Case 1", "Phase 1")
   - Use clear, technical language appropriate for code maintenance

5. **Build & Testing Requirements**
   - Code must compile without warnings/errors
   - All regtests must pass on Ubuntu and macOS
   - Verify no regression in existing functionality
   - Run validation before creating PR

6. **No Stray Content**
   - Don't include temporary exploration notes in final code
   - Keep commit messages and documentation focused on the actual change
   - Remove debug code before committing

## How to Use

### Option 1: Manual Reference
When starting a conversation with an AI agent, reference the appropriate instruction file:
- Copy the relevant instruction content and paste it at the start of your conversation
- Or provide the file path/content as context

### Option 2: Automated Integration
Many modern coding agents and assistants will automatically load these instructions based on standard locations:
- **Claude Code** and other Claude assistants will automatically load `CLAUDE.md` from the root directory.
- **GitHub Copilot** will automatically load `.github/copilot-instructions.md` for all repo-wide sessions.
- **Gemini** and general agents will automatically load `GEMINI.md` or `AGENTS.md` from the root directory.

### Option 3: Direct Link
Share the raw GitHub URL of your preferred instruction file with the agent:
```
https://raw.githubusercontent.com/hgopalan/massconsistent_amr/main/CLAUDE.md
```

## Benefits

- **Consistency**: All agents start with the same project context
- **Efficiency**: Reduces time spent explaining the project structure
- **Quality**: Helps agents understand code patterns and best practices
- **Automation**: Can be automatically loaded by tools and IDEs
- **Standards**: Ensures all contributions follow project development standards

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

For more information about the massconsistent_amr project, see the [main README](README.md) or [GETTING_STARTED_TUTORIAL.md](GETTING_STARTED_TUTORIAL.md).
