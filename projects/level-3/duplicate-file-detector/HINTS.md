# P-3.4 — Progressive Hints

Use these in order. Each tier gives away more than the last — if you reach Hint 4 and are still stuck, that's a signal to reread Ch04's `string_view`/streaming-I/O material before continuing, not to open `SOLUTION.md`.

## Hint 1 — Direction

Think of this project as a series of increasingly expensive sieves, each one only ever operating on the survivors of the previous one: size is nearly free to obtain (a `stat` call, no file content read at all), a partial hash costs a small bounded read, and a full hash costs reading the entire file. The vast majority of files in a real corpus will be eliminated as "definitely not duplicates" by the cheapest sieve alone, since most files simply don't share their exact size with anything else.

## Hint 2 — Technique

At each stage, group survivors by their current cheap signal into a hash map (signal → list of candidate paths), and only promote groups with 2 or more members to the next, more expensive stage — anything alone in its group after any stage can be dropped immediately, since it has nothing left to be compared against. This naturally produces the cascade shape without needing to think about it as anything more exotic than "group, filter singletons, repeat with a more expensive key."

## Hint 3 — Implementation

For resumable progress, think about what "has this file already been fully processed" needs to durably record to be trustworthy on the next run: the file's identity (path plus something that detects if it changed, like size+mtime together), and which cascade stage it reached. Writing each file's record as a single atomic append (rather than an in-place update that could be caught half-written by a process kill) sidesteps having to detect and repair partial writes — a killed process simply loses its last, not-yet-fully-appended record, and everything before that point remains trustworthy.

## Hint 4 — Debugging/Design

If your peak memory usage still scales with total file count more than expected, check whether you're holding every file's full path *and* every stage's data (size groups, partial-hash groups, full-hash groups) all in memory simultaneously for the entire run, rather than allowing earlier stages' now-singleton groups to be dropped and their memory reclaimed once a file has been eliminated from further consideration — the bound this project asks for comes from processing being stage-by-stage over surviving candidates only, not from holding the full original candidate set alive throughout.
