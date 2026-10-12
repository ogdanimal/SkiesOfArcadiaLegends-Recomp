# DRAFT: DolRecomp issue (not filed)

Target: https://github.com/ExpansionPak/DolRecomp/issues
Checked against DolRecomp `origin/HEAD` (`c876e2b`) and ModernGekko master's pin (`71ce7f9`) on 2026-10-07:
both still store `llvm.frameaddress` in `native_wrapper.cpp` and set no unwind tables.

---

**Title:** LLVM backend, Windows x64: native entry wrapper stores the wrong frame pointer for `eh.sjlj`, corrupting the caller's xmm6–xmm15

**Summary**

On `x86_64-pc-windows-msvc` targets, every native entry that leaves through the `eh.sjlj` escape path
returns to its caller with xmm6–xmm15 restored from the wrong stack slots. Those registers are
callee-saved on Win64, so the host's own variables get corrupted. In our case this shows up as an
`abort()` (`0xc0000409`) at exit. Linux and other SysV targets are unaffected.

**Cause**

`FunctionEmitter::emitWrapper` (`src/backend/llvm/native_wrapper.cpp`) writes
`llvm.frameaddress(0)` into `buffer[0]` of the jump buffer. `llvm.eh.sjlj.longjmp` reloads the frame
register from that slot, and the resume block then addresses the frame through it, including in the
epilogue's XMM restores. On SysV x86-64 the frame address is `rbp`, so this works. On Win64 the frame
register is placed at an offset into the fixed frame (here `rbp = rsp + 0x80`), while `frameaddress`
returns the frame base. After a longjmp, `rbp` sits 0x80 too low, so the epilogue reloads xmm6–xmm15 from
the wrong slots. The GPRs are popped through `rsp` and survive, which is why this stays mostly invisible.

**How we hit it**

The Skies of Arcadia Legends (GEAE8P) module runs under ModernGekko on Windows 11. The module is
cross-generated from Linux with `DOLRECOMP_LLVM_TARGET=x86_64-pc-windows-msvc` (LLVM 20) and linked
with clang-cl. MSVC keeps the Play Time Tracker `std::thread` in xmm6 across the CPU loop
(`CPUManager::Run`). After any escaping native entry, xmm6 holds garbage (we saw a heap pointer and a
return address). `timing.join()` fails, throws `std::system_error` out of the thread function, and
`std::terminate` follows. Every exit in our two savestates that hit escapes crashed (12 of 12). A canary
trampoline around `dispatch` presets xmm6–15 and checks them afterwards; it showed the corruption on every
escaping entry.

**Fix we're using** (patch attached / below): on Windows x86-64 targets, store `rbp` itself
(`llvm.read_register("rbp")`, with `"frame-pointer"="all"` on the wrapper) instead of `frameaddress`.
After the fix there were 40 of 40 clean exits. The Linux x86-64 objects are byte-identical (all 5,452),
and DolRecomp's tests pass.

```diff
-      Value *frame =
-          builder_.CreateCall(Intrinsic::getDeclaration(
-                                  &module_, Intrinsic::frameaddress, {pointer}),
-                              {builder_.getInt32(0)});
+      // eh.sjlj.longjmp reloads the frame register from buffer[0] and the
+      // resume path addresses the frame through it. A Win64 frame register
+      // sits at an offset into the frame, so frameaddress (the frame's base)
+      // would land rbp below its slots and the epilogue would restore the
+      // callee-saved xmm6-15 from the wrong places. Store rbp itself there.
+      const Triple triple(module_.getTargetTriple());
+      const bool win64 = triple.isOSWindows() && triple.getArch() == Triple::x86_64;
+      Value *frame = nullptr;
+      if (win64) {
+        wrapper->addFnAttr("frame-pointer", "all");
+        MDNode *rbp = MDNode::get(context_, {MDString::get(context_, "rbp")});
+        frame = builder_.CreateIntToPtr(
+            builder_.CreateCall(
+                Intrinsic::getDeclaration(&module_, Intrinsic::read_register,
+                                          {builder_.getInt64Ty()}),
+                {MetadataAsValue::get(context_, rbp)}),
+            pointer);
+      } else {
+        frame = builder_.CreateCall(
+            Intrinsic::getDeclaration(&module_, Intrinsic::frameaddress,
+                                      {pointer}),
+            {builder_.getInt32(0)});
+      }
```

(This needs `<llvm/IR/Metadata.h>` and `<llvm/TargetParser/Triple.h>`. The arch check keeps Windows on
ARM64 on the existing path. Our local copy only checks `isOSWindows()`, because we only target x64.)

**Related suggestion: unwind tables on Windows targets.** Generated functions carry `nounwind` and no
`uwtable`, so the COFF objects have no `.pdata`/`.xdata`. Windows then treats module functions as leaves,
so debuggers, crash dumps and SEH dispatch can't walk through module frames. Setting
`module.setUwtable(UWTableKind::Async)` and `function.setUWTableKind(UWTableKind::Async)` in
`applyTargetAttributes` for Windows triples fixed that for us. It isn't needed for the fix above, but
it's what made this bug debuggable. We can open this as a separate issue or PR if you prefer.
