# AGENTS.md

## Project goal
This repository is a seL4 + Microkit-based automotive lighting control demo.
The near-term goal is to evolve it from a tutorial-style demo into an engineering-grade embedded project.

## Current structure
Core files currently include:
- commandin.c
- scheduler.c
- lightctl.c
- gpio.c
- faultmg.c
- light.system
- Makefile

## Build environment
- Microkit SDK version: 2.0.1
- Board: qemu_virt_aarch64
- Default config: debug

## Commands
- Build current full image: `make part5`
- Run in QEMU: `make run`

## Rules for changes
- Prefer small, reviewable patches.
- Do not introduce new features until build structure and docs are cleaned up.
- Keep seL4/Microkit communication behavior unchanged unless task explicitly asks for refactor.
- When refactoring, centralize duplicated constants into shared headers.
- Always update README when build commands or repo structure change.
- If a task changes behavior, also add a test plan section to the commit summary.

## Validation
For each task:
1. explain what changed
2. list commands run
3. show build/test results
4. list any remaining risk