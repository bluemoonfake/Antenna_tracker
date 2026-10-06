# Global Coding Rules

## Priority
When rules conflict, prioritize:
1. Correctness and safety
2. Readability
3. Simplicity / absence of unnecessary code
4. Minimum code size

Do not sacrifice correctness or clarity merely to reduce line count.

## Simplicity
- Implement only behavior explicitly required by the task.
- Follow YAGNI. Do not add speculative features or extensibility.
- Prefer the smallest correct change over architectural expansion.
- Do not introduce an abstraction for a single concrete use case.
- Do not create interfaces, factories, wrappers, managers, or base classes
  unless there are at least two real uses or the existing architecture requires them.
- Prefer modifying existing code over creating new files/classes when reasonable.

## Reuse
Before implementing new functionality, check:
1. existing repository code;
2. the language standard library;
3. libraries already used by the repository.

Do not reimplement functionality already provided by them.

## Code size
For newly written code, target:
- function body <= 40 lines;
- <= 4 function parameters;
- nesting depth <= 3;
- C++ source file preferably <= 400 lines.

If exceeding these limits is clearly simpler or safer, keep the clearer design
and briefly explain why.

## Boilerplate
Do not:
- create getters/setters by default;
- add defensive checks for invariants guaranteed by an established boundary;
- add comments that repeat the code;
- add helper classes/files used only once without a readability benefit;
- add configuration options that were not requested;
- add TODO infrastructure for hypothetical future requirements.

## Editing
When modifying existing code:
- preserve the repository's current architecture and naming style;
- make the smallest coherent diff;
- do not refactor unrelated code;
- do not rewrite an entire file when a local edit is enough.

## Final simplification pass
Before finishing code changes:
- remove unused variables, functions, comments and abstractions;
- remove duplicated logic where consolidation improves clarity;
- replace custom implementations with existing library/repository utilities;
- verify no speculative functionality was added.

## Engineering decision policy

Before changing code:

1. Understand the existing behavior.
2. Inspect nearby implementations and call sites.
3. Identify the smallest change that satisfies the requirement.
4. Prefer the repository's existing pattern over introducing a new pattern.
5. Prefer evidence from the codebase over assumptions.

When multiple implementations are possible, prefer in this order:

1. Existing repository pattern.
2. Existing library/framework capability.
3. Simple local implementation.
4. New abstraction only when clearly justified.

Do not redesign architecture merely because another architecture appears cleaner.

If a design decision cannot be determined from the repository or requirement,
state the assumption instead of silently inventing one.
## Repository exploration

Before implementing non-trivial changes:

- Find the relevant call sites.
- Inspect at least one analogous implementation if available.
- Check existing utilities before creating new helpers.
- Trace data flow far enough to understand units, coordinate frames,
  ownership and lifecycle.

Do not begin implementation based only on a filename or function name.

Do this silently before presenting the result.