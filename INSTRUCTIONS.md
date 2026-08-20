# SOLUTIONS AND PROJECT SPECIFICATION

## PROBLEM SOLUTIONS

Every chapter MUST end with a complete **Solutions** section.

Do NOT place solutions directly after individual problems.

The intended chapter structure is:

1. Crash Course
2. Quick Checks
3. Problems — increasing difficulty
4. Debugging Problems
5. Integration Challenge
6. Chapter Project(s), where applicable
7. **Solutions**

This allows me to attempt the entire chapter without accidentally seeing the answer.

---

## SOLUTION QUALITY

For every problem, provide a solution at the end of the chapter.

The solution should contain:

### Approach
A concise explanation of the intended reasoning.

### Reference Solution
Correct, compilable C++ code where applicable.

### Explanation
Explain the important parts of the solution.

### Why It Works
Explain the relevant C++ rules or engineering principles.

### Common Wrong Approaches
Where useful, show common incorrect approaches and explain why they fail.

### Complexity
For algorithmic problems, include:
- time complexity
- space complexity

### C++ Considerations
For relevant problems, explicitly mention:
- lifetime
- ownership
- exception safety
- undefined behavior
- iterator invalidation
- move/copy behavior
- concurrency
- performance

Do not over-explain trivial solutions.

---

# SOLUTION LEVELS

Solutions should match the difficulty of the problem.

For simple questions:
- concise explanation
- short solution

For subtle C++ questions:
- explain the relevant language rule precisely
- distinguish standard guarantees from implementation behavior
- explain why plausible alternatives are wrong

For implementation problems:
- provide complete compilable code
- use modern C++20/23
- include appropriate error handling

For design problems:
- provide a reference design
- explain trade-offs
- acknowledge that multiple valid solutions may exist

Never imply that the reference solution is the only valid design when the problem genuinely allows multiple approaches.

---

# PROJECT PROBLEM STATEMENTS

Every substantial project MUST have a self-contained and sufficiently detailed problem statement.

The project statement must NOT merely say:

"Build an HTTP server."

It must define the actual engineering problem.

At minimum include:

## Objective

What the system is supposed to accomplish.

## Functional Requirements

Exactly what the program/system must do.

## Input

Specify:
- input format
- data types
- accepted values
- examples
- command-line arguments, files, or network protocol details where applicable

## Output

Specify:
- output format
- expected values
- error output where relevant
- examples

For interactive/networked projects, describe the protocol or request/response format instead of forcing traditional stdin/stdout when inappropriate.

## Constraints

Include relevant constraints such as:
- C++ standard
- platform
- memory limits
- file size limits
- concurrency requirements
- throughput targets
- latency targets
- maximum number of objects/connections
- acceptable complexity
- library restrictions

Do not invent arbitrary constraints solely to make the project difficult.

Constraints should make the project resemble a realistic engineering task.

## Edge Cases

Explicitly describe important boundary conditions that must be handled.

Do not reveal every hidden test case.

Some cases should remain undisclosed so that I must reason about robustness.

## Error Handling

Specify expected behavior for:
- invalid input
- resource exhaustion
- I/O failure
- malformed data
- concurrent shutdown/failure
- other relevant failures

## Acceptance Criteria

Define objective criteria for considering the project complete.

Examples:
- functional correctness
- tests passing
- sanitizer-clean execution
- no data races
- API requirements
- performance requirements
- resource limits

## Testing Requirements

State what I am expected to test.

Where useful, require:
- unit tests
- integration tests
- stress tests
- fuzz tests
- sanitizer runs
- benchmarks

## Hints

Every substantial project must include hints.

Use progressive hints rather than giving away the architecture immediately.

### Hint 1 — Direction
A broad hint pointing me toward the relevant concept.

### Hint 2 — Technique
A more specific suggestion about an appropriate technique or abstraction.

### Hint 3 — Implementation
A concrete suggestion about how to approach a difficult part.

### Hint 4 — Debugging/Design
A stronger hint that addresses a likely blocker.

Hints should be optional and should not directly provide the complete solution.

---

# PROJECT SOLUTIONS

Every substantial project MUST have a solution/reference section.

However, the project solution should be separated from the problem statement so that it is difficult to accidentally see.

The solution should contain:

## Reference Architecture

Describe a reasonable architecture.

## Design Rationale

Explain important decisions and alternatives.

## Reference Implementation

Provide representative or complete implementation where practical.

For very large projects, provide:
- milestone-by-milestone implementation guidance
- key interfaces
- critical implementation sections
- tests
- reference approaches

Do not necessarily dump thousands of lines of code into a single file.

## Testing Strategy

Explain how the reference solution should be tested.

## Performance Analysis

Where relevant:
- expected complexity
- allocation behavior
- concurrency behavior
- bottlenecks
- benchmark considerations

## Failure Modes

Explain important ways the system can fail and how the reference design handles them.

## Extensions

Provide optional advanced improvements.

---

# PROJECT DIFFICULTY

Projects should increase progressively.

### Level 1 — Focused component

One major concept.

### Level 2 — Multi-concept component

Several related concepts.

### Level 3 — Realistic utility

Useful standalone program/library.

### Level 4 — Systems component

Concurrency, resource management, performance, or OS interaction.

### Level 5 — Production-style project

Multiple components, tests, error handling, performance, maintainability.

### Level 6 — Capstone

Large system requiring architecture and significant engineering judgment.

---

# HIDDEN TESTING

For appropriate implementation projects, design some tests that are not obvious from the problem statement.

The hidden tests should target:
- edge cases
- resource lifetime
- invalid input
- exception safety
- concurrency
- performance
- API correctness

Do not make hidden tests deceptive.

Their purpose is to determine whether the implementation is genuinely robust.

---

# DO NOT LEAK THE SOLUTION

When presenting a problem, never include details that effectively reveal its solution.

For example, do not say:

"Use a std::unordered_map to solve this."

unless choosing that data structure is explicitly part of the exercise.

Prefer:

"You need efficient average-case lookup by key."

The learner should decide the implementation.

---

# PROJECT HINT POLICY

Hints should guide reasoning rather than prescribe the implementation.

BAD:
"Create a std::mutex called m and lock it in this function."

GOOD:
"Consider what synchronization is required to protect shared state."

BAD:
"Use an LRU cache implemented with a list and unordered_map."

GOOD:
"You need O(1)-average lookup and efficient removal of the least-recently-used entry. What data structures provide both properties?"

---

# CHAPTER COMPLETION REQUIREMENT

A chapter is not complete until it contains:

- concise crash-course definitions
- quick checks
- progressively harder problems
- debugging exercises
- implementation exercises
- at least one integration challenge where appropriate
- project(s) where appropriate
- complete solutions for all problems
- project hints
- project reference solutions/designs

The learner should be able to:

READ → ATTEMPT → CHECK → UNDERSTAND → APPLY

without needing an external answer key.