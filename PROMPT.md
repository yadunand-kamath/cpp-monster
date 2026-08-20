# C++ INTERMEDIATE → ADVANCED MASTERY WORKBOOK

You are building a long-term **C++ crash-course + problem-solving + project workbook** for me.

The workbook is NOT intended to be a traditional textbook.

Its purpose is:

> Give me extremely concise explanations of intermediate and advanced C++ concepts, followed by enough problems and projects to force genuine understanding and practical ability.

My target is:

> Become a junior-level professional C++ developer with unusually deep C++ knowledge and engineering reasoning approaching the conceptual knowledge of a senior/principal C++ engineer.

I understand that real-world engineering experience cannot be replaced by study. The workbook should therefore maximize conceptual depth, implementation practice, debugging ability, and engineering judgment.

Target language:
- C++20
- C++23 where useful
- Modern C++ practices

---

# CRITICAL DESIGN PRINCIPLE

The workbook should be approximately:

20% crash-course/reference
80% active problem solving and projects

Do NOT turn it into a long textbook.

For almost every concept:

1. Give a one-line definition.
2. Optionally give one tiny example.
3. Immediately move to problems.

The explanations should be short enough that I can scan the entire chapter quickly before attempting the problems.

The workbook should teach through doing.

---

# WORKBOOK PHILOSOPHY

The progression should be:

CONCEPT
↓
MENTAL MODEL
↓
PREDICTION
↓
SMALL PROBLEM
↓
IMPLEMENTATION
↓
DEBUGGING
↓
COMBINED PROBLEM
↓
REALISTIC PROJECT

Do not make every problem an isolated syntax exercise.

As difficulty increases, combine previously learned concepts.

---

# DIFFICULTY LEVELS

Every chapter must progress through levels.

LEVEL 1 — Recognition

Identify and explain concepts.

Examples:
- What does this code mean?
- What is the object's lifetime?
- Which overload is selected?

LEVEL 2 — Prediction

Predict:
- output
- types
- overload resolution
- lifetime
- iterator validity
- exceptions
- concurrency behavior

LEVEL 3 — Implementation

Write small pieces of C++.

LEVEL 4 — Debugging

Given broken C++, identify:
- root cause
- undefined behavior
- lifetime errors
- logic bugs
- exception-safety problems
- concurrency issues

LEVEL 5 — Integration

Combine several concepts in one problem.

LEVEL 6 — Production

Solve realistic engineering tasks with:
- requirements
- edge cases
- tests
- error handling
- maintainability
- performance considerations

LEVEL 7 — Principal Reasoning

Incomplete requirements.

I must:
- identify missing requirements
- ask useful questions
- make trade-offs
- justify architecture
- reason about failure
- reason about scalability
- reason about concurrency
- reason about performance
- consider future changes

Do not artificially make every problem difficult.
Difficulty should increase naturally.

---

# CHAPTER FORMAT

Every concept/chapter should use approximately this structure:

# Topic

## Crash Course

For every concept:

**Concept:** one-line definition.

**Why it exists:** one concise sentence.

**Key rule:** one concise sentence.

**Tiny example:** only when it clarifies the concept.

Avoid long explanations.

---

## Common Misconceptions

List the 3–8 most important misconceptions.

Focus particularly on misconceptions that produce incorrect mental models.

---

## Quick Checks

Very short prediction/reasoning questions.

Do NOT provide answers immediately.

---

## Problems

Problems should increase in difficulty.

For each problem include:

### Problem N
**Difficulty:** 1–7

**Skills tested:**
- ...

**Problem statement:**
...

**Constraints:**
...

**Expected outcome:**
...

Do NOT include the solution.

For selected difficult problems, include:
**What this is secretly testing:**
but only when revealing that does not give away the solution.

---

## Debugging Problems

At least some problems should give me deliberately broken code.

Require me to determine:
- what is wrong
- why it is wrong
- whether it is undefined behavior
- how to reproduce it
- how to fix it

Do not immediately show the fix.

---

## Implementation Problems

Require actual code.

Prefer tasks that produce reusable components.

Examples:
- implement a small RAII wrapper
- implement a vector-like container
- implement a thread-safe queue
- implement an allocator
- implement type erasure
- implement a generic algorithm
- implement an LRU cache

---

## Integration Challenge

End major chapters with a problem combining multiple concepts from the chapter and previous chapters.

---

# CONCEPT COVERAGE

The workbook should comprehensively cover intermediate and advanced modern C++.

At minimum include:

## Language & Core Semantics

- initialization
- `const`
- `constexpr`
- `consteval`
- `constinit`
- scope
- lifetime
- storage duration
- references
- pointers
- `auto`
- `decltype`
- `decltype(auto)`
- overload resolution
- implicit conversions
- explicit conversions
- operator overloading
- lambdas
- function objects
- `std::function`
- `std::invoke`

## Object Lifetime & Resource Management

- constructors
- destructors
- initialization order
- RAII
- rule of 0/3/5
- ownership
- `unique_ptr`
- `shared_ptr`
- `weak_ptr`
- custom deleters
- custom resources
- exception safety

## Value Categories

- lvalue
- xvalue
- prvalue
- glvalue
- rvalue
- temporary materialization
- move semantics
- copy elision
- NRVO
- forwarding references
- reference collapsing
- `std::move`
- `std::forward`

## STL

- containers
- iterator categories
- iterator invalidation
- algorithms
- allocators
- `vector`
- `deque`
- `list`
- associative containers
- unordered containers
- `string`
- smart pointers
- `optional`
- `variant`
- `any`
- `function`
- ranges

## Generic Programming

- function templates
- class templates
- specialization
- partial specialization
- variadic templates
- parameter packs
- fold expressions
- type traits
- SFINAE
- concepts
- constraints
- compile-time programming
- CRTP
- policy-based design
- type erasure

## Error Handling

- exceptions
- stack unwinding
- `noexcept`
- exception guarantees
- error codes
- `optional`
- `variant`
- `expected`
- error propagation
- API error design

## Object Model

- object representation
- alignment
- padding
- triviality
- standard layout
- inheritance
- virtual functions
- vtables/vptrs
- RTTI
- multiple inheritance
- virtual inheritance
- object lifetime rules
- aliasing
- placement construction
- low-level object manipulation

## Compilation & Linking

- preprocessing
- translation units
- declarations
- definitions
- ODR
- inline
- templates and instantiation
- symbol resolution
- name mangling
- static libraries
- shared libraries
- dynamic linking
- ABI
- binary compatibility

## Build Systems

- CMake
- targets
- `PRIVATE`
- `PUBLIC`
- `INTERFACE`
- generator expressions
- toolchains
- dependency management
- testing
- installation
- packaging
- CI

## Linux & Systems

- processes
- threads
- virtual memory
- system calls
- file descriptors
- files
- sockets
- TCP
- memory mapping
- shared libraries
- signals
- debugging tools

## Concurrency

- threads
- `jthread`
- mutexes
- locks
- condition variables
- futures
- promises
- async
- atomics
- data races
- happens-before
- synchronization
- memory ordering
- lock-free programming
- thread pools
- work queues
- producer/consumer systems
- deadlocks
- starvation
- livelock
- false sharing

## Performance

- algorithmic complexity
- allocations
- memory locality
- cache behavior
- branch prediction
- data layout
- false sharing
- profiling
- benchmarking
- compiler optimization
- inlining
- vectorization
- move/copy costs
- allocators
- object pools
- arenas

## Modern C++

- ranges
- views
- concepts
- coroutines
- `co_await`
- `co_yield`
- coroutine frames
- modules
- modern error handling

## Engineering

- API design
- ownership design
- dependency management
- coupling
- cohesion
- abstraction
- testing
- fuzzing
- sanitizers
- observability
- backwards compatibility
- maintainability
- architectural trade-offs

---

# PROBLEM DESIGN RULES

Avoid repetitive exercises.

Do NOT generate 20 problems that differ only in variable names.

Every problem should teach or test something distinct.

Use a mixture of:

- predict the output
- predict the type
- predict the lifetime
- explain compiler behavior
- find UB
- debug code
- implement a component
- refactor code
- compare two designs
- optimize code
- benchmark code
- design an API
- analyze architecture
- investigate a concurrency bug
- inspect compiler output
- reason about ABI
- write tests
- diagnose a performance problem

---

# PROJECTS ARE EXTREMELY IMPORTANT

Include many substantial projects.

Projects must NOT be generic textbook exercises.

Avoid projects such as:
- calculator
- basic todo list
- student management system
- simple guessing game
- tic-tac-toe
- trivial CRUD applications

Prefer projects that resemble real engineering work.

---

# PROJECT CATEGORIES

Include projects across these categories:

## Developer Tools

Examples:
- fast log parser
- code statistics analyzer
- duplicate-file detector
- command-line search/indexing tool
- configuration loader
- binary file inspector

## Systems

Examples:
- memory allocator
- arena allocator
- thread pool
- job system
- concurrent queue
- event loop
- resource manager
- process supervisor

## Networking

Examples:
- TCP server
- HTTP client
- HTTP server
- connection pool
- rate limiter
- asynchronous network service

## Storage

Examples:
- LRU cache
- persistent key-value store
- WAL/logging system
- simple database
- page cache
- B-tree/B+ tree

## Libraries

Examples:
- serialization library
- command-line parser
- type-erasure utility
- task system
- configuration library
- small testing framework

## Performance

Examples:
- benchmark suite
- allocator comparison
- cache-locality experiment
- serialization benchmark
- concurrent queue benchmark

---

# PROJECT QUALITY REQUIREMENT

Every substantial project should have:

- explicit requirements
- non-functional requirements where relevant
- edge cases
- tests
- failure scenarios
- measurable performance targets where appropriate
- extensibility considerations
- suggested milestones
- optional advanced features

For larger projects, provide:

PHASE 1
minimum viable implementation

PHASE 2
robust implementation

PHASE 3
performance improvements

PHASE 4
advanced features

PHASE 5
production review

Do not provide the implementation unless specifically requested.

---

# REALISM REQUIREMENT

Projects should contain real constraints.

Examples:

"Must support 100k operations/sec."

"Must remain bounded in memory."

"Must be safe under concurrent access."

"Must recover cleanly after failure."

"Must support millions of records."

"Must avoid unnecessary allocations."

"API must remain stable."

Do not invent arbitrary constraints merely to make projects harder.

---

# CROSS-CONCEPT PROJECTS

As the workbook progresses, projects must deliberately combine concepts.

For example:

A thread pool might eventually require:

- RAII
- move semantics
- templates
- synchronization
- condition variables
- exception safety
- atomics
- testing
- CMake
- performance analysis

The project should not tell me which concepts to use once I reach advanced difficulty.

I should determine that myself.

---

# ADVANCED CHALLENGES

At the end of major sections, include "Blind Challenges."

A Blind Challenge:

- gives requirements
- does not name the concepts being tested
- does not suggest a design
- may have incomplete requirements
- requires me to identify the relevant engineering issues

This tests transfer of knowledge rather than chapter-based recall.

---

# PRINCIPAL-LEVEL CHALLENGES

Include dedicated design problems where there is no single correct solution.

For example:

"Design a high-throughput task execution service."

Require me to consider:

- API
- ownership
- concurrency
- failure
- backpressure
- memory
- observability
- testing
- scalability
- performance

Evaluate trade-offs rather than one predetermined implementation.

---

# REFERENCES AND ACCURACY

Do not invent C++ rules.

For subtle language/library behavior, verify against authoritative sources when appropriate.

Prefer:
- C++ standard documentation
- cppreference
- C++ Core Guidelines
- compiler documentation
- established technical references

Distinguish:
- standard guarantees
- implementation behavior
- ABI behavior
- observed behavior

---

# WORKBOOK ARCHITECTURE

Before generating detailed content:

1. Design the entire curriculum.
2. Identify prerequisites.
3. Order topics by dependency.
4. Determine chapter difficulty.
5. Map projects to concepts.
6. Identify repeated concepts that need spaced review.
7. Identify prerequisite gaps.
8. Create a master table of contents.

Do NOT start by blindly generating Chapter 1.

First produce the complete architecture.

---

# FILE ORGANIZATION

Create the workbook as Markdown files.

Recommended structure:

cpp-workbook/
├── README.md
├── CURRICULUM.md
├── PROGRESS.md
├── CONCEPT_INDEX.md
│
├── 01-core-semantics/
├── 02-lifetime-raii/
├── 03-value-categories/
├── 04-stl/
├── 05-generic-programming/
├── 06-error-handling/
├── 07-object-model/
├── 08-compilation-abi/
├── 09-linux-build/
├── 10-concurrency/
├── 11-performance/
├── 12-modern-cpp/
├── 13-architecture/
│
├── projects/
│   ├── level-1/
│   ├── level-2/
│   ├── level-3/
│   ├── level-4/
│   └── capstones/
│
└── assessments/

Keep chapters modular.

Do not place the entire workbook in one file.

---

# CONCEPT INDEX

Create a CONCEPT_INDEX.md mapping every major concept to:

- chapter
- prerequisite concepts
- difficulty
- related problems
- related projects
- review checkpoints

This allows later targeted study.

---

# PROGRESS TRACKING

Create PROGRESS.md.

Track:

- completed concepts
- completed problems
- completed projects
- weak areas
- recurring mistakes
- topics requiring review

Do not automatically mark something complete merely because I read it.

Completion should be based on solving the associated problems.

---

# FINAL QUALITY BAR

Before considering a chapter complete, check:

- Is every concept explained in one or two concise sentences?
- Are there enough problems?
- Do problems increase in difficulty?
- Are there prediction questions?
- Are there debugging tasks?
- Are there implementation tasks?
- Are there integration tasks?
- Does the chapter contain realistic engineering scenarios?
- Does it avoid repetitive textbook exercises?
- Does it connect to previous material?
- Is the wording concise?
- Is the technical content accurate?

Before considering the entire workbook complete, check:

- Does difficulty increase globally?
- Are prerequisites sensible?
- Are concepts revisited?
- Are projects progressively more realistic?
- Are there genuinely useful projects?
- Are there blind/principal-level challenges?
- Does the workbook test transfer rather than memorization?

---

# IMPORTANT GENERATION RULE

Do not attempt to generate the entire workbook in one response.

First generate:

1. CURRICULUM.md
2. CONCEPT_INDEX.md
3. PROJECT_ROADMAP.md
4. PROGRESS.md

Then generate chapters one at a time.

After each chapter, perform a quality review before continuing.

If the workbook becomes too long, prioritize:
- breadth of concepts
- concise crash-course definitions
- high-quality problems
- high-quality projects

over long theoretical explanations.

The workbook should feel like an extremely good **technical training manual + problem book**, not a textbook.

---

# FINAL PRINCIPLE

The workbook should make me think:

"I can read this chapter in 15 minutes, then spend 2–5 hours actually doing things."

Not:

"I spent two hours reading this chapter."

Build for active learning.