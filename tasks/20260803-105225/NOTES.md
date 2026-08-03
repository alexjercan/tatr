# Notes: Make tatr flow --dry-run a real precondition probe

## What changes

Before: `tatr flow <id> --dry-run` returns at `tatr.c:5917`, right after the
edge and the gate name are computed and before the graph is even loaded. It
prints two lines and exits 0 unconditionally. A caller cannot distinguish "this
advance would succeed" from "this advance would be refused for four reasons".

After: `--dry-run` evaluates everything the real call evaluates, in the same
order, through the same collectors, and writes nothing. The exit status is the
contract: 0 only when the real call would complete the whole advance, non-zero
otherwise. The unmet text is the same text the real refusal prints, so the afk
runner can hand it to an agent verbatim.

Output, unchanged edge header plus the refusal halves in the conditional tense:

```console
$ tatr flow 20260802-211009 --dry-run          # would succeed
Task 20260802-211009 would move PLANNING -> WORKING
  gate PLAN would run
$ echo $status
0

$ tatr flow 20260802-211009 --dry-run          # record half unmet
Task 20260802-211009 would move PLANNING -> WORKING
  gate PLAN would run
ERROR: Would refuse to advance 20260802-211009 from PLANNING: 2 precondition(s) not met
  - bad-record-schema: TASK.md has no '## Steps' section
  - bad-record-schema: TASK.md has no '## Definition of Done' section
  Record unchanged.
$ echo $status
1

$ tatr flow 20260802-211009 --dry-run          # world half unmet (half-success)
Task 20260802-211009 would move PLANNING -> WORKING
  gate PLAN would run
ERROR: Would not advance 20260802-211009 to WORKING: 1 precondition(s) not met
  - dependency 20260801-120000 is not CLOSED (STATUS: OPEN)
  Cursor would be held at PLANNING.
$ echo $status
1
```

## Surfaces

| File | Why |
| --- | --- |
| `tatr.c` (`main_flow`, ~5825-6016) | the only change: move the `dry_run` early return down past the graph load and both precondition passes, and tense the two refusal reports |
| `checker.sh` (`test_flow_advances_and_records_gates` ~1473, plus new cases) | the existing dry-run assertion only covers the passing edge; refusal and half-success paths need exact-message + exit-status + file-unchanged assertions |
| `README.md` (~314, ~379, ~485) | the flag's one-line description and the flow transcript both currently promise only "print the edge and the gate" |

## Data and interfaces

No new types, no new functions, no signature changes. The four existing
collectors already take everything they need and write only into a
`Flow_Unmet`, which is why the probe is a control-flow change rather than a
new evaluator:

```c
static void flow_gate_preconditions(const Aids_String_Slice *tasks_dir,
                                    const Task_Graph *graph,
                                    const Aids_String_Slice *huid,
                                    const Task *task,
                                    Aids_String_Slice task_raw,
                                    Task_Gate gate, Flow_Unmet *unmet);
static void flow_close_preconditions(const Aids_String_Slice *tasks_dir,
                                     const Task_Graph *graph,
                                     const Aids_String_Slice *huid,
                                     const Task *task,
                                     Aids_String_Slice task_raw,
                                     unsigned int gates, Flow_Unmet *unmet);
static void flow_world_preconditions(const Aids_String_Slice *tasks_dir,
                                     const Aids_String_Slice *huid,
                                     const Task *task, Flow_Unmet *unmet);
static void flow_unmet_print(const Flow_Unmet *unmet);
```

The one local addition is the `dry_run` flag reaching further down the
function; the two report sites pick their verb from it.

## Sketches

Illustrative, not a patch.

Move the early return down and keep only the header:

```c
-    if (dry_run) {
-        printf("Task " SS_Fmt " would move " SS_Fmt " -> " SS_Fmt "\n", ...);
-        if (has_gate) { printf("  gate " SS_Fmt " would run\n", ...); }
-        else          { printf("  no gate runs on this edge\n"); }
-        return_defer(0);
-    }
-
     task_graph_init(&graph);
     ...
+    if (dry_run) {
+        printf("Task " SS_Fmt " would move " SS_Fmt " -> " SS_Fmt "\n", ...);
+        if (has_gate) { printf("  gate " SS_Fmt " would run\n", ...); }
+        else          { printf("  no gate runs on this edge\n"); }
+        fflush(stdout);   // stderr is unbuffered; the halves must stay ordered
+    }
```

Tense the record-half refusal, exit status unchanged:

```c
     if (unmet.count > 0) {
-        aids_log(AIDS_ERROR, "Refusing to advance " SS_Fmt " from " SS_Fmt ": ...", ...);
+        aids_log(AIDS_ERROR, "%s to advance " SS_Fmt " from " SS_Fmt ": %zu precondition(s) not met",
+                 dry_run ? "Would refuse" : "Refusing", ...);
         flow_unmet_print(&unmet);
         fprintf(stderr, "  Record unchanged.\n");
         return_defer(1);
     }
```

Stop before the write, report the held cursor in the conditional:

```c
     if (!closes && to == Task_Activity_WORKING) {
         flow_world_preconditions(&tasks_dir, &id, &task, &world_unmet);
     }
+    if (dry_run) {
+        if (world_unmet.count == 0) { return_defer(0); }
+        aids_log(AIDS_ERROR, "Would not advance " SS_Fmt " to " SS_Fmt ": ...", ...);
+        flow_unmet_print(&world_unmet);
+        fprintf(stderr, "  Cursor would be held at " SS_Fmt ".\n", SS_Arg(from_label));
+        return_defer(1);
+    }
```

Nothing after that point is reachable in a dry run, so `task.meta` is never
mutated and `task_save` is never called - the "writes nothing" guarantee is
structural, not a promise each branch has to keep.

## Shape

```
  main_flow
    |
    parse, resolve, load raw TASK.md
    refuse if RESOLUTION set
    compute edge (from -> to, closes?) and exit gate
    |
    load task graph                      <-- dry run now reaches here
    |
    [dry] print "would move" + gate line
    |
    record half: gate_preconditions + close_preconditions -> unmet
    |
    unmet? --yes--> print "Refusing" / "Would refuse" ------------> exit 1
    |  no                                   (nothing written either way)
    |
    world half (edge entering WORKING only) -> world_unmet
    |
    [dry] world_unmet? --no--> exit 0
    |            \--yes--> "Would not advance" + cursor held -----> exit 1
    |
    [real] record gate, advance cursor if world clear, task_save
           world_unmet? --yes--> "Not advancing" + cursor held ---> exit 1
           --no--> print moved line, exit 0
```

## Consequences and open questions

- **The half-success decision.** The task asks how a dry run reports the case
  where the record half passes and only the world half fails, since the real
  command half-succeeds there: it records the gate and holds the cursor. Taken
  assumption: report the halves as two distinct messages (the same two the real
  command prints, tensed), and collapse the exit status to non-zero for both,
  because "the advance would not complete" is the question the consumer asks.
  The header line plus the message text is what distinguishes them.
- **One non-zero code, not two.** `1` for both refusal shapes, matching the
  real command. A distinct code for half-success (say `2`) was considered and
  rejected under YAGNI: the stated consumer branches on refuse-or-not and reads
  the text. Cheap to add later; impossible to remove once a caller depends on
  it.
- **Cost: the probe is no longer free.** It now loads the whole task graph and
  reads sibling records, so a dry run costs what a real refusal costs. That is
  the point, but a caller that only wanted the edge name pays for it too. No
  such caller exists in-tree.
- **The record half short-circuits the world half**, in the dry run exactly as
  in the real call. A task whose plan is malformed *and* whose dependency is
  open reports only the plan problems. This keeps the probe's output identical
  to the refusal it predicts; the alternative (evaluate both halves always)
  would print text the real command never prints.
- **What this forecloses:** `--dry-run` can no longer be used on an unreadable
  or unresolvable task graph to ask only "what edge is next". That question has
  no other answer in the CLI; `tatr show` prints ACTIVITY, not the successor.
- **Still open, non-blocking:** whether `tatr close --resolution DONE`
  (`tatr.c:6343`, which runs `flow_close_preconditions` today) should grow the
  same probe. Out of scope here; the consumer only drives `flow`.
