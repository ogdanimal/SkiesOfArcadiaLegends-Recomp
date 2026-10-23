# Keeping guest calls and returns inside generated code (DolRecomp LLVM backend)

Status (2026-10-23): kept for the record; **the premise did not hold**. The exit census (section 3.0, built
as `STATICRECOMP_EXIT_STATS`) found indirect branches behind most dispatches, but a prototype that chained them
(mechanism C) cut dispatches by 85% and made the game thread 14% slower: the runtime round trip was only ~8%
of it. See development notes, *CPU cost against Dolphin's JIT*. The rest of this note is the original design.

Original status: design note, read-only investigation. No code was changed and nothing was built or run.
Everything below is from reading source. Where a number is a guess it is marked **(guess)**; where I
am inferring behaviour from code without having seen it execute it is marked **(inferred)**.

Paths: `DR` = `lib/DolRecomp/src` , `RC` = `lib/ModernGekko/vendor/dolphin/Source/Core/Core/PowerPC/StaticRecomp`.
Note: the build that ships does NOT use `lib/DolRecomp`; it uses the patched copy
`lib/ModernGekko/vendor/dolphin/DolRecomp` (adds `gather_pipe.cpp`, the Win64 sjlj frame fix, a different
`memory.cpp` slow path, the mtmsr-entry patch, extra CPUState fields). The files I cite are identical in
both unless stated, and line numbers below are from `lib/DolRecomp`.

---------------------------------------------------------------------------------------------------

## 0. Summary and two corrections to the premise

1. **The "native ABI" is not where the traffic is.** `build/perf/exact/gen.log` (Skies USA ws34, exact
   semantics) reports `native=10 compat=5451` chunks. Ten of 5,461 chunks qualify. Every other chunk
   already uses the "compat" convention, which *already* does direct host calls between chunks for `bl`
   and tail jumps (`DR/backend/llvm/branch_targets.cpp:35-205`). So "widen the native ABI" would, at best,
   change 0.2% of chunks today, and it cannot be widened to cover game code without becoming something
   else (section 1). I recommend not doing it.

2. **The dispatches come from unwinding, not from missing call edges.** In the compat convention guest
   state lives in `CPUState` (`--state-in-memory`; `DR/backend/llvm/register_state.cpp:13-17`), so a
   host stack frame holds no guest state. Any early exit (budget, `mtmsr` enabling EE, any `bctrl`/`blrl`/
   `bctr`, exception, fallback, interception) stores `pc` and does `ret`; each caller then sees
   `ctx->pc != its continuation` and also `ret`s (`branch_targets.cpp:182-205`) until the wrapper returns
   to `StaticRecompCore::Run`. The host frames for the whole guest call stack are now **gone** although the
   guest stack is intact. From then on, *every guest `blr` that returns into one of the lost frames is a
   dispatch start at a return site*, and the first instruction of each callee entered through the
   dispatcher is a dispatch start at a function entry. That matches your census (hottest starts =
   return sites and function entries) and gives a simple model:

       dispatches  ~=  sum over unwinds ( 1  +  number of lost frames that later return )

   So 93% "not budget exits" is compatible with **a few events that unwind deep stacks** (the 7% budget
   exits, `mtmsr` EE exits, indirect calls, slice ends) each followed by a cascade of return dispatches.
   This is a hypothesis **(inferred)**; step 0 below is a cheap census that confirms or kills it before any
   generator work.

Recommended plan (section 3): (0) census; (1) do not unwind when the host has nothing to do (soft budget
checkpoint + conditional `mtmsr` exit + chain-depth counter), keeping every guest-visible decision point
where it is today; (2) make the remaining dispatches cheap (slim `Run`, flag table instead of the 24 MB
lookup); (3) chain `bctrl`/`blrl` (and `bctr` tails) through a chunk table with the callee's existing
entry checks. Rough size: 3-5 engineer-weeks end to end, step 1 alone about 1-1.5 weeks plus a full PGO
retrain on three platforms.

---------------------------------------------------------------------------------------------------

## 0.1 What exists today (map of the three execution modes)

* **Unit of compilation is a fixed 128-instruction chunk, not a guest function.**
  `DOLLLVM_DEFAULT_CHUNK_INSTRUCTIONS 128` (`DR/app/pipeline.c:55`), env override 128..4096
  (`pipeline.c:111-125`). `ranges[]` are those chunks (`pipeline.c:880-905`); 5,461 of them for Skies. A
  typical guest function spans several chunks, so cross-chunk edges are everywhere (fall-through at the
  end of every chunk, plus `b`/`bl` to other chunks).
* **Entry points** (`pipeline.c:438-470`): program entry, symbols, every `b`/`bc` target. `bctrl`/`bctr`
  targets are *not* entries by themselves; they become region leaders via "instruction after any
  non-fallthrough terminator" (`register_state.cpp:130-155`), the mtmsr patch (`register_state.cpp:141-148`),
  and the dispatch switch in `emitEntry` (`exits.cpp:124-140`). A PC that is not a region leader goes to the
  cold entry, which single-steps in the interpreter (`control_flow.cpp:14-34`, `:36-127`).
* **Per-chunk body** `func_XXXXXXXX_budget(ctx, chain, control)`, `control = entry_pc | return_pc<<32`
  (`exits.cpp:13-30`, `branch_targets.cpp:~95-110`). Per-chunk wrapper `func_XXXXXXXX(ctx)`
  (`native_wrapper.cpp:16-146`) which is what the module dispatcher (`dolrecomp_call`,
  `DR/backend/dispatch.c:~340-375`) calls. The wrapper only uses `setjmp` for native-ABI chunks
  (`native_wrapper.cpp:54`), so for 99.8% of chunks it is a plain call.
* **Compat call/return protocol** (`branch_targets.cpp:169-205`): tail edge = `musttail` call; linked edge
  = call, then compare `ctx->pc` with the constant continuation, `call_resume` if equal, else `ret`
  (`call_mismatch`). Return (`blr`) and every other indirect branch = `INDIRECT` terminator
  (`control_flow.cpp:149-192`): compare against the chunk's own local continuations
  (`continuations_`, `register_state.cpp:99-114`), otherwise store `pc`, settle `downcount`, `ret`. There is
  no `DOLIR_TERM_RETURN` producer in the IR builder (`ir/dolir_builder.c:1385-1406`: `blr`, `bctr`,
  `bctrl`, `blrl` are all `INDIRECT`).
* **Budget** (`exits.cpp:246-270`, called at loop headers `instructions.cpp:29-30` and at every
  cross-chunk direct transfer `branch_targets.cpp:58`): exit when `guard_cycles + cycles >= 256` or (compat
  only) 2048 guard checks in the chain. Exit = `sideExit(pc)`: sync dirty CR/XER, `pc = target`,
  `downcount -= cycles`, `ret` (compat) or `longjmp` to the wrapper (native).
* **Interception** is per chunk, in the callee's own prologue: `host_call == NULL` -> straight on;
  otherwise `ppc_native_region_available(ctx, chunk_start, chunk_end)` (`exits.cpp:69-98`,
  `DR/cpu/cpu.c:366-379`). Skies installs no `host_call` unless a mod is loaded
  (`patches/README.md`, the `dolphin_runtime.cpp` row), so on Skies this is one null test per chunk entry.
* **Runtime loop** (`RC/StaticRecompCore_Run.cpp:126-274`): per dispatch it checks the 24 MB chunk table
  (`fast_dispatchable_at`, `:73-92`), calls `m_module->dispatch` (-> generated dispatcher -> wrapper),
  flushes `-m_guest.downcount` into `ppc.downcount`, advances the guest timebase with a 64-bit division
  (`:182-190`), checks the idle PC (`:193`), `m_guest.exception`, Dolphin synchronous exceptions
  (`SYNC_EXCEPTION_MASK`), and the post-`mtmsr` external-interrupt break (`:209-213`), then loops while
  `downcount > 0`. `SyncIn`/`SyncOut` (`RC/StaticRecompCore_Sync.cpp`) copy the whole register file once
  per **burst** (3.2M), not per dispatch (47.9M).

---------------------------------------------------------------------------------------------------

## 1. Why functions fail `nativeABIFlags`, and what is fundamental

`nativeABIFlags` is `DR/backend/llvm/native_abi.cpp:27-58`. A chunk is native only if **every** block
passes all of:

| # | Rejected when | Where | Verdict | Why |
|---|---|---|---|---|
| a | any block has `cycle_cost == 0` | `:31` | Conservative | Native results carry cycles in registers (`nativeCyclesInResult`, `native_exits.cpp:55-62`). A zero-cost block is embedded data or a cost-less instruction; this could be relaxed to "== 0 is fine". Small effect. |
| b | terminator FALLBACK | `:31-32` | **Fundamental for a register-state ABI** | `ppc_fallback_instruction` runs the *interpreter* on whole CPU state (`RC/..._Hooks.cpp:HookInstructionFallback` does `SyncOut` -> `SingleStepInner` -> `SyncIn`). All guest state a caller might hold in registers would have to be in `CPUState` first, i.e. the ABI input/output set becomes "everything". Not a problem for the compat ABI. |
| c | terminator SYSTEM_CALL / RFI | `:32-33` | Conservative | They only store pc/msr/srr and exit (`control_flow.cpp:211-233`); allowing them as cold exits needs the escape-state propagation that already exists (`native_abi.cpp:287`). Rare in game code, so low value. |
| d | any guest load/store that is not a *constant MEM1 address* | `:17-25`, `:36-41` | Mixed (see below) | The dynamic path (`memory.cpp:96-162`, `:244-295`) has an MEM1 fast path, an MEM2 path, and a slow path calling `external_read/external_write` (MMIO, gather pipe, translation faults). |
| e | any helper other than `MEMORY_FENCE` | `:42-52` | Mostly fundamental | Helpers are the 21 kinds in `ir/dolir.h:121-141`: SPR read/write, FPSCR, program exception, psq load/store, exact float/paired, `lswx`, `stwcx`, `dcbz_l`, `eciwx`, `tlbie`, cache control, fp-available check. They are declared `READ_STATE|WRITE_STATE|MAY_EXIT|MAY_RAISE|BARRIER` without per-slot masks (`dolir_builder.c:1270-1360`), they call into the host or raise exceptions, and several may change MSR/SRR/DAR. `FP_AVAILABLE`, `FPSCR_UPDATED` and `EXACT_FLOAT/PAIRED` are the ones that cover most FP code, which is why almost no real function qualifies. |
| f | callee/caller mismatch at the edge | `branch_targets.cpp:43-46` | Design choice | A native chunk can only call native chunks and vice versa; any mixed edge returns to the dispatcher via `exitDestination`. With 10 native chunks this is practically every edge. |
| g | edge reaches another chunk at all | `prepareModuleABIs` `:88-117` | Neutral | Interprocedural propagation of input/output/escape masks over *chunk* edges (`:216-298`); chunks, not functions. |

**(d) in detail.** A "dynamic" access is rejected because its slow path must `materialize(current_pc_)`
(store all dirty state, `memory.cpp:217`/`:67`), call out, then `reloadUsedState()` (`memory.cpp:83`). For a
native chunk the register-held state of *every caller up the native chain* is only recoverable if the
callee's `escape_state` mask includes it. That machinery exists (`native_abi.cpp:276-298`) so this is
**conservative in principle**: you could admit dynamic accesses and let escape-state grow. What is
fundamental is the cost model: realistic functions have 20-40 live GPR/FPR inputs; the existing statistics
already show spill to the stack for the 9 "stack_functions" out of 10. Past ~8 integer + 8 FP inputs
(AArch64) or 6+8 (SysV x86) the "register ABI" passes arguments in memory, which is what compat does through
`CPUState`. So widening buys little even if it were free. (Also `slotInMemory` is `false` for native
chunks, `register_state.cpp:14`, so state-in-memory's cheap early exits are lost.)

**Conclusion.** Do not invest in widening `nativeABIFlags`. Bypass it: keep the 10 native chunks as they
are, and put the new chaining in the compat convention where the other 5,451 chunks live.

---------------------------------------------------------------------------------------------------

## 2. Invariants a dispatcher round trip guarantees

For each: what Run/dispatcher does today, and what a chained transfer (direct call kept alive, or an
in-module loop) must preserve.

| Invariant | Today | Requirement for chaining |
|---|---|---|
| **Cycle charge to `ppc.downcount`** | Generated code subtracts into `ctx->downcount` (a negative accumulator zeroed by `SyncIn`, `RC/_Sync.cpp:~83`) at every exit (`settleCycles`, `exits.cpp:166-176`). Run converts it per dispatch: `charge = -m_guest.downcount`, minimum 1, `ppc.downcount -= charge` (`_Run.cpp:182-186`). | The chain keeps threading one logical counter through `chain->pending_cycles`/`guard_cycles` (`exits.cpp:179-200`, caller flush `branch_targets.cpp:~104`). A soft checkpoint must do exactly the Run conversion (including the "min 1" rule) at the checkpoint, and must still stop the chain when `ppc.downcount <= 0`. |
| **Timebase advance** | `m_guest.timebase += (rem + GuestTimebaseCycles(charge))/TIMER_RATIO` per dispatch, carry in `m_timebase_cycle_remainder` (`_Run.cpp:187-190`). `mftb` (`ppc_mftb`, `cpu.c:~382`) reads `ctx->timebase`, so within a chain it is stale by at most the unflushed cycles. | Same helper does the same arithmetic (carry preserved, so the *total* is identical). The staleness bound stays "one budget quantum" only if checkpoints keep happening at least every 256 cycles. Guest busy-wait on `mftb` is bounded by that. Optionally have `mftb` add the pending cycles for an exact read (BlueWake chose this). |
| **Budget exhaustion mid-callee** | `emitBudgetGuard` exits with the callee's *target* as pc; the dispatcher re-enters there (`exits.cpp:246-270`). Also bounds host stack depth (implicitly). | Keep a guard at every cross-chunk transfer and loop header; at the guard, either checkpoint-and-continue or unwind. Add an **explicit depth counter**; the implicit bound disappears once budget exits stop unwinding (section 3.6). |
| **External interrupt / exception delivery** | Dolphin raises EXTERNAL_INT/DEC inside `CoreTiming::Advance`, which only runs when `ppc.downcount <= 0` (outer loop `_Run.cpp:126-130`). Delivery (`CheckExceptions`/`CheckExternalExceptions`) happens in `Run` after `SyncOut` (`:218-223`). Native code that takes a guest exception itself sets `ctx->exception` and redirects pc (`cpu.c:317-324`); Run just clears the flag (`:201-208`). Hook-raised Dolphin exceptions (`ppc.Exceptions & SYNC_EXCEPTION_MASK`) break the burst (`:209-210`). | The chain must unwind (state is already in `CPUState`, pc already the vector) when: `ctx->exception != 0` (already checked after each helper), `ppc.downcount <= 0` at a checkpoint, `ppc.Exceptions & SYNC_EXCEPTION_MASK`, or an EE-enabling `mtmsr` finds EXTERNAL_INT pending. Note an existing gap: a Dolphin-side DSI raised inside `HookExternalRead/Write` sets `ppc.Exceptions`, not `ctx->exception`, so generated code keeps running until the dispatch ends. A longer chain lengthens that window; the checkpoint must test `ppc.Exceptions`. |
| **`after_mtmsr`** | An `mtmsr` that raises MSR[EE] 0->1 side-exits to the next instruction (`special_instructions.cpp:13-33`, leader made by `register_state.cpp:141-148`). Run then breaks the burst only if EXTERNAL_INT is pending and the previous instruction is an `mtmsr` (`_Run.cpp:95-117`, `:211-213`, `:221-223`). | The exit exists only to give Run the chance to deliver a pending interrupt exactly at the EE-enable boundary. Replace "always exit" by "exit iff EXTERNAL_INT pending" (read through a pointer to `ppc.Exceptions` placed in `CPUState`). Pending=0 -> identical behaviour; pending=1 -> identical exit. |
| **Host-call interception at callee entry** | Dispatcher: `IsHostCallAddress(pc)` (`_Run.cpp:136-137`, `:215`) and, in chunks, `ppc_native_region_available` (`exits.cpp:69-98`). | Already evaluated in *every* chunk's prologue, including callees reached by existing direct calls, so a new chained edge inherits it for free as long as it enters through the normal body (not a fast re-entry into the middle of a chunk). That is the structural difference from BlueWake's composite loop, which skipped it. A blocked chunk returns `interception_exit` (pc = entry, `ret`), which unwinds. Also keep `IsForcedFallbackAddress` / `ChunkContainsHostCall` semantics (below, flag table). |
| **SMC / ICache invalidation** | `ppc_cache_control` / fallback fast path -> `JitInterface::InvalidateICacheLine` -> `OnICacheInvalidate` marks overlapping chunks `UNVERIFIED` (`RC/_SMC.cpp:333-386`). `DispatchableAt` re-hashes an UNVERIFIED chunk before running it (`_SMC.cpp:~225-232`, `VerifyChunk`). **Direct chunk-to-chunk calls skip all of this** (known: `docs/development-notes.md:1073-1078`, `docs/backlog.md:81`). | Chaining must not make that worse. Provide a per-chunk `u8` state table shared with the runtime and test it at every *new* chained entry (indirect calls) and, cheaply, at existing direct calls (one byte load). The runtime flips entries on invalidate/verify. Optionally a global "code invalidated" byte checked after each call returns (covers returning into a rewritten caller). |
| **Savestates** | Taken between dispatches; state is the Dolphin `ppc` struct only. Host frames are never saved. | Preserved by construction as long as every unwind leaves `CPUState` complete (compat: yes; always `pc` + `downcount`). Never persist chain/depth/flag-table contents. Loading a state must reset them (the runtime already resets on `SyncIn`; add `chain depth = 0`, flags re-derived). |
| **REL modules** | `ResolveNativeAddress`/`TranslateRelAddress` around each dispatch (`_Run.cpp:162-167`), chunk states reset when mapping changes (`_SMC.cpp:96-108`). | Out of scope for Skies (no RELs). Keep the chain disabled when `m_has_rel_modules` (flag table all zero) rather than teaching generated code about address translation. |
| **Lockstep verifier** | Per dispatch: `Prepare`, run, `Verify` replays the interpreter from entry pc to end pc comparing registers, memory journal, cycles (`_Run.cpp:152-173`, `StaticRecompLockstep_Check.cpp:71-229`, env `STATICRECOMP_LOCKSTEP*`). Mftb is pinned to the entry timebase (`StaticRecompLockstep.h` `g_tb_override`). | Still works with longer dispatches (cost grows), but a soft checkpoint must be disabled under lockstep (otherwise `end_pc` is no longer a dispatch end) or the helper must mimic the `Verify` boundary. Simplest: under `STATICRECOMP_LOCKSTEP` force "always unwind" at every checkpoint = current behaviour. Also `m_native_dispatches` indexes the lockstep window and the trace; document that its meaning shrinks. |
| **Idle-PC check** | After each dispatch: `if (pc == m_idle_pc && m_idle_pc) CoreTiming::Idle()` (`_Run.cpp:192-196`). | A soft checkpoint has the pc and must apply the same test (and unwind). Skies leaves the idle PC at 0 as far as I can tell **(guess, not checked)**. |
| **Exit-pc contract for the dispatcher** | Wrapper returns with `ctx->pc` = next instruction to run and nothing else; `lr` carries the return pc (`native_wrapper.cpp:~24-26`). | Unchanged: every unwind path ends in "pc stored, downcount settled, ret". |
| **Frames are fungible (not a requirement, a property to keep)** | Compat frames hold no guest state; returning to a frame at `pc == continuation` is correct whatever the reason the callee exited (state is in `CPUState`), including `pc` being equal by coincidence. | Do not introduce frame-private guest state (no register-resident GPRs across calls, no per-frame spill of guest values) in the new paths. This is what keeps unwinding trivial. |

---------------------------------------------------------------------------------------------------

## 3. Proposed design

### 3.0 Step 0: measure why the stack unwinds (1-2 days, no semantic change)

Before changing the generator, attribute every dispatch to an *exit reason* and a *stack depth at exit*.

* **Free first cut from existing data.** `STATICRECOMP_DISPATCH_SAMPLES` already samples the dispatch pc
  every 4096th dispatch (`_Run.cpp:158-159`). Classify each sampled pc by decoding the guest instruction
  before it and by whether it is a `bl` target:
  * after `bl`: that caller frame was **dead** (a direct call keeps the frame alive, so a dispatch here
    means something unwound while the callee ran) -> cascade return;
  * after `bctrl`/`blrl`: indirect-call return;
  * `bl` target / chunk entry: function entry after a budget/other exit;
  * loop header: budget exit; `mtmsr+4`: EE exit.
  The ratio "after-`bl`" : "after-`bctrl`" : "entries" already says how much is cascade vs indirect calls.
* **Exact version.** Add `u32 exit_reason; u32 exit_depth;` to `CPUState` in a *stats-only* build
  (`DOLRECOMP_LLVM_EXIT_STATS`, not shipped) and store a constant before every `ret`/`longjmp` that leaves
  a body: reasons `BUDGET_CALL`, `BUDGET_LOOP`, `BUDGET_STEPS`, `MTMSR_EE`, `INDIRECT_CALL` (linked),
  `INDIRECT_JUMP` (`bctr`), `RETURN_ROOT` (`blr` with no live caller; needs the depth counter of 3.6),
  `EXCEPTION_*`, `FALLBACK`, `INTERCEPT`, `ENTRY_MISS`, `CALL_MISMATCH`. Run histograms them per dispatch
  and per burst-end cause (`downcount<=0`, `after_mtmsr`, SYNC exception, state change). Report also mean
  guest cycles per dispatch and per burst (3.2M bursts vs 47.9M dispatches = 15/burst; I cannot tell from
  this alone whether bursts end on slice exhaustion or on interrupt breaks).
* Decision rule: if (budget + `mtmsr` EE) exits plus their cascade explain more than half of the dispatches,
  step 1 is the whole story; if indirect calls dominate, do step 3 first; if slice ends dominate, step 2
  (cheaper dispatch) is worth more than anything else.

### 3.1 Principle

In the compat convention **unwinding is always correct and cheap per frame; the problem is only that it
destroys frames that would have returned in a few cycles**. So the design goal is "unwind only when the
host has real work", not "build a new call ABI". Concretely every site that can exit gets one of three
treatments: (A) *continue in place* if the host has nothing to do, (B) *chain through* if the target is
statically/dynamically known and enterable, (C) *unwind* as today.

### 3.2 Mechanism A: soft checkpoint instead of the budget exit

Replace the hard exit in `emitBudgetGuard` (`exits.cpp:246-270`) by:

```
if (guard_cycles + cycles >= 256 || steps >= 2048 || depth >= MAXDEPTH) {
    syncDirtyState(); store pc = target; settleCycles();      // exactly sideExit() today
    r = ctx->chain_checkpoint(ctx);                           // new CPUState hook, C++ in Run
    if (r != CONTINUE) returnFromBody();                      // unwind as today
    reloadUsedState(); reloadCallCounters();                  // CR/XER regs; counters reset by settle
    // fall through into the transfer
}
```

`chain_checkpoint` is `StaticRecompCore::AfterDispatch(bool from_chain)`, the body of `_Run.cpp:175-213`
extracted into a member so Run and the helper share one implementation: charge flush with the "min 1" rule,
timebase advance, idle-PC, `m_guest.exception` reset, `SYNC_EXCEPTION_MASK`, `after_mtmsr`-style
EXTERNAL_INT test, then the loop condition (`ppc.downcount > 0`, `state_ptr == Running`, target chunk
dispatchable, not a host-call address). It returns CONTINUE only if Run would have looped without leaving
the burst. It must not touch guest registers when it returns CONTINUE (only `timebase`, `downcount`,
counters). Under lockstep it always returns UNWIND.

Why this keeps behaviour: the decision points (the guard sites) and the bookkeeping at them are exactly
today's; the only difference is that the host stack survives. The cost of a checkpoint is one indirect
call plus the Run arithmetic, paid once per >=256 cycles instead of once per cascade of returns.

What it does **not** remove: a slice end (`ppc.downcount <= 0`) still has to unwind, because Dolphin must
run `CoreTiming::Advance` (events, interrupts, GPU-on-CPU) and that can redirect pc (interrupt) or pause the
core. I am not proposing to call `Advance` from inside generated frames (re-entrancy with savestate load,
pause, GPU, `PropagateGuestMSR`; unknown, avoid at first). The cascade after each slice end remains. If
the census shows the slice-end cascade dominates, mitigate by *deferring* the unwind until the stack is
shallow (flag "unwind at next return to depth <= 1", hard cap at 2x budget). That delays CoreTiming by up
to a function's run time and is a timing change; treat as an option, not a default **(guess: 1-2 days to
prototype, real risk to audio/DSP timing, needs the DSP-event comparison from section 4)**.

### 3.3 Mechanism B: conditional `mtmsr` EE exit

In `emitStateWrite` (`special_instructions.cpp:13-33`), when the EE bit is being enabled load
`*ctx->ext_exceptions` (a `volatile u32*` to `ppc.Exceptions`, new CPUState field set in `Init`) and exit
only if `EXCEPTION_EXTERNAL_INT` is set. `OSRestoreInterrupts`/`OSEnableInterrupts` then become straight
line code. The region-leader rule for the next instruction (`register_state.cpp:141-148`) stays (the exit
still exists on the pending path). Semantics: unchanged for pending=1; for pending=0 the old path made Run
check and find nothing. One subtle difference: the old exit also forced a flush/timebase update and gave
`DispatchableAt` a chance; none of that is observable except staleness within the 256-cycle bound.

### 3.4 Mechanism C: chain indirect calls (`bctrl`, `blrl`) and indirect tails (`bctr`)

Today these are the worst offenders **(inferred)**: they unwind the entire host stack, then the callee runs
as a root, then its `blr` is a dispatch at the return site (two dispatches per call plus the cascade for
everything above).

For a *linked* `INDIRECT` terminator (`control_flow.cpp:149-192`) in the compat path:

```
target = (CTR|LR) & ~3
if (target is a local continuation) -> existing switch (control_flow.cpp:158-168)
else:
   idx  = chunk_index(target)              // generated: 3 code ranges -> 3 compares + shift
   ok   = idx != NONE && chunk_ok[idx] == VERIFIED && depth < MAXDEPTH
   if (!ok) -> unwind as today
   emitBudgetGuard(target)                 // same guard as direct calls
   fn   = chunk_body[idx]                  // table of func_X_budget, NULL for the 10 native chunks
   call fn(ctx, chain, control = target | (pc+4)<<32)   // same protocol as direct_call
   compare ctx->pc with continuation (local -> call_resume, else mismatch/ret)
```

The callee body runs its normal prologue (`host_call` test, `ppc_native_region_available`, entry switch,
cold entry), so interception, forced-fallback and cold-entry behaviour are inherited. A `bctr` (non-linked)
becomes a `musttail` call to the same table entry, which also stops switch-table jumps from unwinding;
`blr` stays as is (it returns to a live caller when there is one). `bc{l}rl`/`bcctrl` with a false
condition keep their fall-through edge.

Required plumbing: (1) `chunk_body[]` and `chunk_ok[]` arrays emitted next to the generated dispatcher
(`DR/backend/dispatch.c`) and exported to the runtime through the module descriptor
(`StaticRecompModuleDesc`, `module_export.c:92-107`); the runtime already has the address->chunk mapping
(`RC/_SMC.cpp:179-215`); assert at load that its chunk i equals generated range i. (2) `CPUState` additions
(`chunk_ok` pointer, `chain_checkpoint`, `ext_exceptions`, optional `exit_reason`), bump
`GXRUNTIME_CPU_ABI_VERSION`, and mirror in `include/moderngekko/cpu_state.h` (see `patches/README.md`
row for the gather-pipe fields: the size check fails otherwise). (3) Runtime keeps `chunk_ok[i] = 1` iff
`m_chunk_state[i] == CHUNK_VERIFIED && !ChunkContainsHostCall(i) && no forced fallback && !m_has_rel_modules`,
cleared in `OnICacheInvalidate` (`_SMC.cpp:333-386`).

Indirect calls into the 10 native chunks, into non-chunk addresses (RAM code, scratchpad), unverified chunks
and blocked chunks all take the existing unwind path. They are rare.

### 3.5 Direct calls: add the flag test, keep the rest

Existing `bl` direct calls (`branch_targets.cpp:35-205`) already stay in generated code. Add the
`chunk_ok[const_idx]` byte test at `branch_targets.cpp:~58` next to the budget guard. That closes the SMC hole
for direct calls (a behaviour improvement for romhacks, see `docs/development-notes.md:1073`) at the price of
one load/compare per cross-chunk call. It also gives the runtime one switch (zero the table) to disable all
chaining for debugging.

### 3.6 Depth, recursion and host stack

Guest recursion becomes host recursion once budget exits stop unwinding. Today it is bounded implicitly:
256 cycles or 2048 guard checks per chain. After A, the bound disappears. Add `chain->depth` (the chain
struct already has three u64 slots, `native_abi.cpp:~116-124` `chainType`; the BlueWake fork reused
`guard_steps_` as depth with a limit of 64: `ExpansionPak_DolRecomp/src/backend/llvm/branch_targets.cpp`
"call_depth_exit"; the C backend uses 24: `DR/backend/emitter.c:508-520`). Increment before every chained
call (direct or indirect), decrement after the callee returns; `depth >= MAXDEPTH` -> unwind with the callee
target as pc, like a budget exit. Frame size is unknown **(guess 100-400 bytes per compat body frame; I did
not look at the machine code)**. The CPU thread's stack: Linux std::thread 8 MiB, Windows and Bionic
non-main threads about 1 MiB, so MAXDEPTH 64-256 is safe on all three platforms; pick by measuring frame
size on the Android build. Deep legitimate stacks beyond MAXDEPTH just unwind, and the cascade comes back
only for those.

### 3.7 Cycle charges

No new accounting. The chain's `pending_cycles`/`guard_cycles` already thread one counter through nested
frames and `settleCycles` subtracts all of it from `ctx->downcount` exactly once (`exits.cpp:166-176`). A
checkpoint is a `settleCycles` plus the Run arithmetic. Because the helper subtracts only what has been
settled, and `reloadCallCounters` re-reads zeros, there is no double charge. Native-ABI chunks (10) keep
cycles-in-result; they are never on a chain edge that crosses a checkpoint because mixed edges unwind
(`branch_targets.cpp:43-46`), so they need no change.

### 3.8 Early-exit propagation (and does state-in-memory help?)

Yes, decisively, **for the 5,451 compat chunks**: with `--state-in-memory`, `slotInMemory` is true for all
slots except CR/XER (`register_state.cpp:13-17`), so an unwind needs only `syncDirtyState()` for CR/XER,
`pc`, `downcount` and `ret`, with no `longjmp` and no per-frame state marshalling. Frames can be dropped in
any order and re-entered by the dispatcher at any `pc` that is a region leader. The 10 native chunks do need
`longjmp` to the wrapper (`exits.cpp:200-244`) and escape-state propagation; they stay unchanged.

Edge: `call_mismatch` for a *native* caller of a compat callee is not generated (mixed edges exit); fine.

### 3.9 Remaining dispatches: make them cheap (can be done first, independent)

Even after A-C the slice ends and genuine exceptions still unwind and cascade. Per-dispatch host cost then
matters. Candidates, all low risk and runtime-only (no PGO/codegen invalidation):

* replace the 24 MB `m_chunk_lookup_table` (4 bytes per instruction, `_SMC.cpp:179-198`) by chunk-granular
  indexing (chunks are 512 bytes, 5,461 of them -> ~22 KB of `u8` flags). The notes already call this out
  (`development-notes.md:~2942`, "24 MB -> ~350 KB"). The same table is the `chunk_ok` array above.
* skip the redundant `DispatchableAt` + generated dispatcher range search: pass the chunk index to the
  module (`dispatch(ctx, pc)` currently re-derives it through `dolrecomp_call`).
* replace the per-dispatch 64-bit divide in the timebase update by comparing against a precomputed
  threshold, only dividing when a tick boundary is crossed (must reproduce the same remainder logic exactly).
* keep `m_native_dispatches`-driven features (lockstep, trace, samples) behind one predictable branch.

### 3.10 Minimum first step

**Step 1 = the shared checkpoint plus `mtmsr` and budget soft exits (3.2 + 3.3), without any new call
edges.** Reasons: (i) per the unwind model it removes the cause that multiplies dispatches (every
non-essential unwind), (ii) every guest-visible decision stays at the same program point so the risk is
bookkeeping, not control flow, (iii) it adds exactly one reusable piece (the checkpoint helper and
`chain->depth`) that steps 2 and 3 need, (iv) it needs no change to dispatch tables or SMC logic.
Expected effect **(guess, to be replaced by the census)**: 30-60% fewer dispatches; if bctrl/`mtmsr`
dominate, more. It is not the final answer: the slice-end cascade is a floor.

If the census shows that indirect calls are > ~30% of unwinds, do 3.4 *before* 3.2: it needs only the table
plumbing and reuses the existing guard.

### 3.11 Why not the more aggressive alternatives

* **Chain everything until budget (BlueWake composite loop).** Measured -60% CPU for them, but it skipped
  per-address interception, lost DSP events (10,281 -> 0) and title readiness
  (`WWRecomp docs/status/DECISIONS.md` lines 88-92, 152). In our code the same mistake would be skipping the
  chunk prologue or never returning to `CoreTiming::Advance`. The design above keeps both: chained entries go
  through the callee prologue, and every checkpoint tests `ppc.downcount`.
* **Call `CoreTiming::Advance` from inside generated code** to avoid the slice-end cascade. Highest upside
  (removes the floor), highest risk (re-entrancy with savestate load/pause/GPU-on-CPU/exception delivery,
  unknown). Defer until after the census and only with a full determinism run.
* **Widen the native register ABI.** See section 1; the benefit disappears for realistic functions.
* **Frame reconstruction** (a shadow stack of (body, continuation) pairs recorded at unwind and replayed by a
  module-resident trampoline). Equivalent to calling the dispatcher with better caching; superseded by 3.9.

---------------------------------------------------------------------------------------------------

## 4. Risks and test plan

### 4.1 What can silently diverge

| Risk | Mechanism | How we would see it |
|---|---|---|
| **Interrupt latency / delivery point** | Chain length changes only if the checkpoint predicate is looser than Run's. Any difference moves the instruction at which an interrupt or decrementer is taken. | Dispatch-trace comparison (`STATICRECOMP_TRACE_FILE`, `_Run.cpp:35-48`) at burst starts: with the same savestate and inputs, the first N burst-start `(pc, lr, ctr, cr, timebase, ppc_downcount)` tuples must match between old and new module for step 1 *exactly*, since decision points are unchanged. Divergence = bug. For steps 2-3 allow different dispatch counts but compare the sequence of **interrupt-delivery pcs and timebase at delivery**. |
| **Timebase reads** | Fewer flushes -> `mftb` sees older values (bounded by 256 cycles). Busy-wait loops with `mftb` that never hit a guard (self-loop inside one chunk has a loop-header guard, so bounded). | Run a known TB-sensitive scene and compare frame pacing; unit test a loop `mftb; cmp; blt` with chunk boundaries. |
| **Audio/DSP/AI event delivery** | If the chain never yields to `Advance`, DSP/AI events starve (BlueWake: 10,281 -> 0). | Count CoreTiming events per emulated second (AI/DSP/VI), old vs new. Must be equal within noise. Add an assert in the helper: `ppc.downcount` is never allowed to go below `-X` without an unwind. |
| **Savestate compatibility** | Chain-resident data (depth, flags) must not leak into states; old states must still load. | Load existing `.sav` states (4 desktop states, ws states) on a new module; save on new, load on old module. `SavestateLayoutTest` (`Source/UnitTests/Core`) unaffected, but add the CPUState size check. |
| **SMC / romhack code** | The flag table is a new gate; a stale `1` lets modified code run (already true for direct calls). | Romhack discs (hold-B hack, `docs/handoffs/HANDOFF-romhacks.md`), `ICBI` loop test: write code, `icbi`, call through `bctrl`; expect the dispatcher verify path. |
| **Exception inside a callee** | `call_mismatch` relies on `pc != continuation`; exception vector can equal a continuation only by recursion into the same chunk (harmless, frames are fungible). | Existing fixture-style tests (below), plus force a DSI/alignment/fp-unavailable inside a chained callee. |
| **Host stack overflow** | Deep guest recursion + chaining. | Test with a recursive guest function (depth 10,000); check MAXDEPTH unwinds and the cascade count, on Windows and Android. |
| **PGO and build cost** | Checkpoint/indirect-call blocks change the CFG of almost every chunk: every PGO profile silently mismatches (`development-notes.md:~2852`) and the object cache key (digest of emitter sources) invalidates. 8-job CI codegen matrix = 18.5 min full (`skies-ci-codegen-matrix` note), 3 discs x 2-3 targets. | Plan one retrain per platform (Retroid trains on device; ~half a day each) and treat "retrain" as part of the step's definition of done. |
| **Lockstep coverage** | Longer dispatches make the shadow replay long; `STEPCAP` may trip. | Disable soft exits under lockstep (3.2), keep lockstep as the semantic oracle for steps 1-3 by running it with chaining *on* only for the stats-free parts. |
| **Win64 sjlj / ABI quirks** | The existing Win64 sjlj frame fix applies only to native chunks (`native_wrapper.cpp:54-90`, patch `dolrecomp-win64-sjlj-frame.patch`). The new paths use normal returns. | Windows clang module smoke run + the xmm6-15 canary (`development-notes.md:~3241`). |
| **Behaviour change = reproducibility** | Dispatch counts and cycle quantisation change. Anything keyed on `m_native_dispatches` changes. | Document; re-baseline traces. |

### 4.2 Tests that already exist (all under `lib/ModernGekko/vendor/dolphin/DolRecomp/tests`, registered in `CMakeLists.txt:266-495`)

* `llvm_abi_execute_test.c`: executes generated chunks, including a native call chain, return-pc mismatch and
  a host-blocked callee (`block_callee` via `PPC_HOST_CALL_NATIVE_REGION_QUERY`). Extend with: compat chain
  A->B->C, budget exit inside C with and without a checkpoint stub, indirect call through the table, blocked
  callee, invalidated callee.
* `native_interception_test.c`: the host-call/region-query contract. Extend for `chunk_ok` = 0.
* `llvm_ir_test.cpp` (inspects generated IR, e.g. tail-call kinds at `:82-84`): add expectations that
  compat `INDIRECT` linked terminators contain the table load and the guard; that no new `longjmp` appears in
  compat bodies.
* `llvm_smoke_test`, `llvm_semantics_test`, `llvm_pipeline_test`, `llvm_generated_compile`, `llvm_variant_runtime`
  (`CMakeLists.txt:436-495`): must pass unchanged.
* `test_dispatch.c`, `test_module_abi.c` (module ABI struct, version check), `test_pc_reference.c` (host call
  semantics), `test_c_execute.c` (C backend, unaffected).
* Runtime side: `Source/UnitTests/Core/SavestateLayoutTest.cpp`, `CoreTimingTest.cpp`; the repo's
  `tools/check-patches.sh` for patch reproducibility; the perf harness `build/perf/gen.sh`
  (`DOLRECOMP_LLVM_ABI_STATS=1`).
* Lockstep: `STATICRECOMP_LOCKSTEP=1` with `_START/_LIMIT/_FILTER/_NODEDUP/_MAXREPORT/_STEPCAP/_TRACE`
  (`StaticRecompLockstep.cpp:52-85`). Run it from the four desktop savestates before and after each step; the
  divergence count must be zero (modulo the existing whitelist).

### 4.3 New tests/telemetry to add

1. `STATICRECOMP_DISPATCH_STATS=1`: per-reason dispatch histogram, per-depth histogram, mean cycles per
   dispatch/burst, number of checkpoints and fraction that unwound.
2. A deterministic replay check: same savestate + scripted input -> hash of (pc, timebase) at every
   interrupt delivery for N emulated seconds, old vs new (step 1 must be identical; later steps equal in
   multiset of delivery events).
3. CoreTiming event-count parity (AI, DSP, VI, decrementer per emulated second).
4. Android: 5-run device variance rule (glitch counts vary 7-44 per run; never conclude from one run).

---------------------------------------------------------------------------------------------------

## 5. Effort (rough; guesses)

| Step | Work | Size | Notes |
|---|---|---|---|
| 0 Census | `exit_reason/depth` stats build, sample classifier script, one battle capture on WSL/Windows | 1-2 days | Gates everything. Zero semantic risk. |
| 1a Runtime refactor | Extract `AfterDispatch` from `Run`, add `chain_checkpoint`/`ext_exceptions` to CPUState (+ABI version, ModernGekko header copy, Android) | 2-3 days | Also removes duplicate logic; low risk. |
| 1b Generator | Soft guard in `emitBudgetGuard`, depth counter in chain struct, conditional mtmsr exit, tests in `llvm_abi_execute_test` | 3-4 days | Compat and native paths of the guard (native keeps unwinding at first). |
| 1c Validate + retrain | Dispatch-trace parity, lockstep, DSP event parity, Windows + WSL + Retroid speed runs, PGO retrain x3 platforms, CI codegen | 3-5 days | Retrain is the long pole (hours per platform, device time; ask before using the Retroid). |
| 2 Cheap dispatch | `chunk_ok` flags replacing the 24 MB table, timebase division, skip duplicate lookup | 2-3 days | Runtime only; no regen. Can be done before step 1. |
| 3a Chunk table + flags plumbing | Generated `chunk_body[]/chunk_ok[]`, module descriptor, runtime upkeep in `OnICacheInvalidate`/`VerifyChunk` | 3-4 days | |
| 3b Indirect chaining in the generator | linked `INDIRECT` -> table call, `bctr` tail, guard + depth, IR tests | 3-4 days | |
| 3c Validate + retrain | As 1c plus SMC/icbi test, romhack discs, EU/JP discs (3 modules x targets) | 3-5 days | |
| 4 (optional) in-chain `Advance` or deferred unwind | Prototype only | 1-2 weeks, high risk | Only if the census shows the slice-end cascade dominates after 1-3. |

Total for 0-3: roughly 3-5 engineer-weeks including retrains. Expected benefit: **unknown until step 0**;
my honest prior is that step 1 plus step 2 recover a meaningful part of the "runtime" share of the 2.2x
(the generated-code 4x vs Jit64 is a different problem, not touched by this).

---------------------------------------------------------------------------------------------------

## 6. Unknowns and things I did not verify

* I did not run anything. The claim that compat `bl` stays in generated code and that `bctrl`/`blrl`
  unwind the stack is from reading `branch_targets.cpp`, `control_flow.cpp` and `ir/dolir_builder.c:1385-1406`;
  I did not look at generated IR/asm for a real chunk. Step 0 verifies it in minutes.
* I do not know the host stack frame size of a compat body, the average guest call depth at an unwind, how
  many bursts end on slice exhaustion vs an interrupt break, or the cycles per dispatch (47.9M dispatches /
  3.2M bursts = 15 per burst is all I have). The cascade model predicts about depth+1 dispatches per
  unwind; it fits 15/burst only if unwinds are about one per burst, which would point at slice ends, not
  budget exits.
* Whether Skies uses `m_idle_pc` (`MAIN_STATICRECOMP_IDLE_PC`), and whether any HLE `host_call` is ever
  installed in the shipped build. I assumed not, from the patch README.
* `lib/DolRecomp` and the vendored patched copy differ (`memory.cpp`, `register_state.cpp`,
  `native_wrapper.cpp`, `emitter.h`, `cpu.h` ...). The design must be implemented against the vendored copy
  and expressed as a new `patches/dolrecomp-*.patch` like the existing ones.
* Dolphin-side re-entrancy of `CoreTiming::Advance` from a host frame inside generated code: not
  investigated; marked as the high-risk optional step.
* Whether the chunk indexing in `module_tables.inc` (hashed chunk ranges, 5,461) is guaranteed to equal the
  generator's range order; needs an assertion at module load before sharing a flag table.
