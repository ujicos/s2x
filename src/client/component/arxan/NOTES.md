# Notes on Arxan

> These are personal research notes written for educational and preservation-related documentation around S2x.

Before reading this, I recommend checking out the resources below. They already explain a large part of the Arxan-related behaviour that I reference throughout these notes.

I created this document because, even after reading those resources, some parts were still not completely clear to me. I wanted to dig deeper, verify certain behaviour myself, and write down what helped me understand it better while working on S2x.

The focus is mostly on the parts I personally investigated: anti-debug behaviour, integrity checks, and code healing. Some details may be incomplete or imperfect, but the goal is to document the process and reasoning in case it helps others.

- [Reverse Engineering Integrity Checks in Black Ops 3 - Momo5502](https://momo5502.com/posts/2022-11-17-reverse-engineering-integrity-checks-in-black-ops-3/)
- [Arxan info from reversing CW - Mallgrab](https://github.com/mallgrab/CWHook/blob/master/NOTES.md)

## Anti-debug

Mallgrab already documented a lot of the anti-debug behaviour used in Cold War, and much of it also seems to apply to some older Call of Duty titles. Because of that, I already had a decent idea of what kind of Windows APIs and patterns I should be looking for. However, I still wanted to verify the behaviour myself instead of assuming the same tricks were present everywhere.

For clarity, I will refer to Call of Duty: WWII as S2 and Call of Duty: Black Ops 3 as T7 throughout these notes, since those are the internal names commonly used when discussing these games.

One annoying part is that many of these Windows API calls are resolved dynamically at runtime. Because of that, they do not appear in the Import Address Table (IAT). While looking through T7 statically, I noticed that the game resolved API addresses in multiple places and then called them indirectly, often through registers.

There was a recognizable pattern, but manually following every indirect call site would have been repetitive and time-consuming. Hooking every suspicious API just to log what was happening also felt messy. I wanted a cleaner way to observe the runtime behaviour and confirm which anti-debug checks were actually being used.

Since I had been following Momo's work for a while and had heard about [Sogen](https://github.com/momo5502/sogen), it seemed like a good tool to try here.

For readers who are not familiar with it, Sogen is a Windows user-space emulator that can emulate and log a program's runtime behaviour. Instead of only looking at the static code, it allows you to observe what the program actually does during execution by looking at the output it produces.

My first idea was to use Sogen directly on S2, but it did not run out of the box and crashed immediately. Because of that, I went back to T7, where I had already done some earlier research and where Sogen was able to produce useful output.

T7 is obviously not the same game as S2, so I did not treat the results as direct proof for S2. However, the observations still helped me understand the kind of anti-debug behaviour I was dealing with, and several of those patterns appeared to carry over to S2, which I will show later.

<img width="1115" height="799" alt="Sogen-bo3-output" src="https://github.com/user-attachments/assets/b3346dec-aaff-4182-b452-666c9f3aa0ae" />

Running T7 in [Sogen](https://github.com/momo5502/sogen) eventually seemed to get stuck, or at least needed more time to continue, but the output it produced before that point was already valuable.

The following output seemed interesting:

```txt
Executing function: GetSystemDirectoryW (kernel32.dll) (0x103c6f790) via 0x14212cb9e (blackops3.exe)
Executing function: CreateFileW (kernel32.dll) (0x103c870f0) via 0x14212d1b7 (blackops3.exe)
--> Opening file: \??\c:\windows\SYSTEM32\ntdll.dll
Executing function: NtCreateSection (ntdll.dll) (0x1801624f0) via 0x14212df09 (blackops3.exe)
--> Section for file: \??\c:\windows\SYSTEM32\ntdll.dll
Executing function: NtMapViewOfSection (ntdll.dll) (0x1801620b0) via 0x14212e0c3 (blackops3.exe)
Executing function: VirtualAlloc (kernel32.dll) (0x103c63c90) via 0x142131dff (blackops3.exe)
--> Allocating 0x1075d0000 - 0x10773b000 (rwx)
```

This shows the game opening `ntdll.dll` from the system directory, creating a section for it, and mapping it into memory. That is interesting because it suggests that the game wants access to a clean copy of `ntdll`.

The next part was especially interesting because these calls did not come from the normally loaded `ntdll.dll`. They came from the memory region that was just allocated after mapping the clean `ntdll` section.

```txt
Executing inline syscall: NtQueryInformationThread (0x25) at 0x107731062 (<N/A>)
Executing inline syscall: NtSetInformationThread (0xD) at 0x107730d62 (<N/A>)
Suspicious: Hiding thread from debugger at 0x107730d62 via 0x107730d60 (<N/A>)
```

This points directly to thread-related anti-debug behaviour. `NtSetInformationThread` can be used with `ThreadHideFromDebugger`, which hides a thread from the debugger, as its name suggests.

When this technique is used, debugging can become unstable or misleading. Exceptions and debug events from the hidden thread may no longer be delivered to the debugger in the way you expect. In practice, this can cause the program to crash when a debugger is attached, make breakpoints appear unreliable, or cause exceptions to be handled by the program's own VEH/SEH logic instead of being caught cleanly by the debugger.

This gave me a much better starting point. Instead of blindly searching for every possible anti-debug trick, I could focus on behaviour that was actually observed during execution.

I later discovered that Arxan does not create only one copy of this data. It actually creates two similar regions. These regions are not full copies of the entire `ntdll.dll` image. They appear to correspond to the executable part of the image, starting after the PE headers, around `ntdll + 0x1000`. The size also seems to vary slightly from system to system, which makes sense if it is based on the mapped image or section layout rather than a hardcoded fixed size.

### Moving back to S2

With this information from T7, the first thing I did when moving back to S2 was run the game and inspect its memory layout in [System Informer](https://systeminformer.sourceforge.io/), previously known as Process Hacker.

Based on the T7 behaviour, I knew what kind of pattern to look for. If S2 was doing something similar, I expected to find one or more private executable memory regions that looked like copies of part of `ntdll.dll`, most likely starting around the executable section rather than the PE headers.

When inspecting the Memory tab in System Informer, I could indeed spot two candidates. Both regions had the same size, were marked as `RWX`, and were close in size to the executable `ntdll` image region.

<img width="916" height="621" alt="system-informer-memory-tab" src="https://github.com/user-attachments/assets/0708f194-d329-46bc-a6fa-94b8b1e8168e" />

Opening those regions and comparing the bytes with the loaded `ntdll.dll` code region showed that they matched. The important detail here is that the selected `ntdll` region in System Informer is not the PE header at the image base. It is the executable region, which starts after the headers, around `ntdll + 0x1000`.

That lines up with the earlier observation from T7: Arxan does not appear to copy the full PE image including the headers, but rather a section/range of `ntdll` that contains executable syscall-related code.

<img width="1315" height="1005" alt="system-informer-memory-inspection" src="https://github.com/user-attachments/assets/430f6ebb-91f6-462e-8f70-a07eccd250a7" />

At that point, the behaviour seen in T7 became useful for S2 as well. It strongly suggested that S2 was doing something very similar: creating private executable regions based on the executable part of `ntdll`. By doing this, the game can avoid relying on the normally loaded `ntdll.dll` code region, making simple API hooks on functions like `NtQueryInformationThread` and `NtSetInformationThread` ineffective.

To work around this, the most straightforward approach for me was to hook `VirtualAlloc` and use a heuristic to detect when the game was allocating memory for these copied `ntdll` sections. When such an allocation was detected, I returned a pointer to the original loaded `ntdll` image at `ntdll + 0x1000` instead.

At first, this caused the game to crash when it reached a copy loop. The reason was that the game still tried to copy data from the clean `ntdll` mapping into the pointer returned by `VirtualAlloc`. Since my returned pointer pointed into the original loaded `ntdll`, that memory had `RX` protection, so the attempted write caused an access violation.

The fix was to skip the copy loop. After doing that, the game no longer used its private copied `ntdll` region for these syscalls. Instead, the calls went through the original loaded `ntdll`, which meant my hooks on functions such as `NtQueryInformationThread` and `NtSetInformationThread` could actually take effect.

The simplified/pseudo implementation looks roughly like this:

```cpp
LPVOID WINAPI virtual_alloc_stub(LPVOID address, SIZE_T size, DWORD allocation_type, DWORD protect)
{
    if (is_ntdll_copy_allocation(size, allocation_type, protect))
    {
        // One of the copy loops is only unpacked while these regions are being set up,
        // so the patches are applied here once the allocation is observed.
        jump(..., ...); // Skip first ntdll copy loop
        jump(..., ...); // Skip second ntdll copy loop

        const auto ntdll = get_loaded_module("ntdll.dll");

        // The copied region starts after the PE headers, so redirect to the
        // equivalent location in the already-loaded ntdll image.
        return ntdll.base() + 0x1000;
    }

    return original_virtual_alloc(address, size, allocation_type, protect);
}
```

As mentioned earlier, the game uses `NtSetInformationThread` to hide threads from the debugger and `NtQueryInformationThread` to check whether a thread is hidden. At first, it seems like the solution would be simple: hook `NtSetInformationThread`, pretend the hide request succeeded, and then make `NtQueryInformationThread` report that the thread is hidden when the game asks.

However, that alone was not enough. The game also probes these functions with invalid or unusual parameters to verify that they behave like the real Windows implementations. Because of that, the hooks need to be careful. They cannot simply return success for every call. They need to validate the input and return the expected error codes in the same cases where the real functions would fail. Otherwise, the game can detect that the behaviour does not match the real API and crash or react accordingly.

The simplified/pseudo implementation looked roughly like this:

```cpp
NTSTATUS NTAPI nt_set_information_thread_stub(...)
{
    if (information_class == ThreadHideFromDebugger)
    {
        // Preserve native-like behaviour for the probes the game performs.
        if (!is_valid_thread_handle(thread_handle))
            return STATUS_INVALID_HANDLE;

        if (thread_information_length != 0)
            return STATUS_INFO_LENGTH_MISMATCH;

        // Pretend the request succeeded, but do not actually hide the thread.
        return STATUS_SUCCESS;
    }

    return original_nt_set_information_thread(...);
}

NTSTATUS NTAPI nt_query_information_thread_stub(...)
{
    if (information_class == ThreadHideFromDebugger)
    {
        // Preserve native-like behaviour for invalid query probes.
        if (thread_information_length != 1)
            return STATUS_INFO_LENGTH_MISMATCH;

        // Report the state the game expects.
        if (thread_information)
            *static_cast<std::uint8_t*>(thread_information) = 1; // thread is hidden

        if (return_length)
            *return_length = 1;

        return STATUS_SUCCESS;
    }

    return original_nt_query_information_thread(...);
}
```

After applying these patches, the threads were no longer hidden from the debugger, and the game no longer detected these hooks.

Another small helper that made the S2 side easier was an API logger I wrote while investigating the anti-debug checks. Compared to T7, S2 was easier to instrument here because many dynamically resolved Windows API calls go through a shared thunk that ends in `jmp rax`. By hooking that thunk, I could log the target address in `RAX`, the return address, and the first few register arguments. This made it much easier to confirm which Windows APIs the game was calling and what arguments were being passed during the anti-debug checks.

The game also uses `VEH` handlers together with `int 2d` instructions as another anti-debugging trick. Normally, the `int 2d` instruction raises a breakpoint-style exception that should be handled by the game's registered `VEH` handler. If a debugger intercepts the exception first, or changes how the exception is delivered, the game can detect that something is attached.

For this part, I reused an existing patch created by the [Auroramod](https://github.com/auroramod/) team.

The remaining anti-debug checks were mostly similar to patterns already seen in older titles, so I will not go into all of them in detail here. One thing still worth mentioning is that S2 also uses `EnumWindows` to iterate over open windows and look for suspicious program names, such as debuggers or memory editing tools.

This is not a new trick, and similar behaviour was also patched in the BOIII client for T7. In S2, it appears that this check can be avoided by passing a harmless callback when the game calls `EnumWindows`. The game does not seem to verify whether the original callback was actually executed, so returning early was enough in this case.

The simplified/pseudo implementation looked roughly like this:

```cpp
BOOL CALLBACK harmless_enum_windows_callback(HWND, LPARAM)
{
    // Continue enumeration, but do not inspect anything.
    return TRUE;
}

BOOL WINAPI enum_windows_stub(WNDENUMPROC callback, LPARAM lparam)
{
    if (call_originates_from_game())
    {
        // Let EnumWindows run, but replace the game's callback with a harmless one.
        return original_enum_windows(harmless_enum_windows_callback, lparam);
    }

    return original_enum_windows(callback, lparam);
}
```

The full implementation of the anti-debug patches can be found here:
[anti_debug.cpp](./arxan/anti_debug.cpp)

The API logger I used during this research can be found here:
[api_logger.cpp](./arxan/api_logger.cpp)

## Integrity checks

### Some background

After T7 received an update in February 2026, I heard that the Arxan implementation had changed. This intrigued me and got me back into reverse engineering Call of Duty.

While first looking at what changed, I almost immediately noticed a small difference. As you may have read in Momo's blog post, there are two types of basic integrity blocks: intact and split. The split variant had changed slightly. Before the update, the game ran its integrity-related instructions and directly jumped to the next piece of code, as seen here:

```assembly
; Pre-update split integrity block with direct jump

8B 04 82                          mov     eax, [rdx+rax*4]
48 8D 15 69 0B 7A E6              lea     rdx, dword_14290579A
89 04 8A                          mov     [rdx+rcx*4], eax
E9 A2 70 FC 00                    jmp     loc_15D12BCDB
```

After the update, the jump no longer happened directly. Instead, the target address of the next instruction sequence was loaded onto the stack, and the game jumped to the address that had been loaded there. This is essentially a stack-obfuscated jump:

```assembly
; Post-update split integrity block with stack-obfuscated indirect jump

8B 45 68                                mov     eax, [rbp+68h]
48 63 C0                                movsxd  rax, eax
48 8B 55 40                             mov     rdx, [rbp+40h]
8B 4D 68                                mov     ecx, [rbp+68h]
48 63 C9                                movsxd  rcx, ecx
8B 04 82                                mov     eax, [rdx+rax*4]
48 8D 15 8E A8 05 00                    lea     rdx, dword_15D528ECC
89 04 8A                                mov     [rdx+rcx*4], eax
48 8D 64 24 F8                          lea     rsp, [rsp-8]           ; preparing stack for indirect jump
48 89 34 24                             mov     [rsp], rsi
48 8D 35 63 B7 E1 FF                    lea     rsi, sub_15D2E9DB4
48 87 34 24                             xchg    rsi, [rsp]
48 8D 64 24 08                          lea     rsp, [rsp+8]
FF 64 24 F8                             jmp     qword ptr [rsp-8]     ; jump to sub_15D2E9DB4
```

To fix Momo's patch for T7, I assumed we needed to resolve that address and jump directly to the next instruction sequence ourselves. That did fix the patch. However, on that same update, his fix for hardware breakpoints also started getting detected and was crashing the game for some reason.

It honestly took me a while to realize that. At first, I thought I was missing something, which got me digging deeper into Arxan. Through static analysis, I eventually noticed that there were three variants of checksum comparisons.

Patching the checks themselves seemed easier than patching the integrity handler context before chaining. [Rosie Orthinanos](https://blog.rosieorthinanos.dev/posts/bo3arxan/) wrote a blog post about this for T7, so I assumed it would work. I did not verify it for T7 myself because, before trying this method, I discovered the bad hook causing the crash and saw that my implementation got Momo's patch working again.

### First approach to patch integrity checks

So, for S2, I still had this method fresh in the back of my head. After reading Mallgrab's write-up, I did some pattern scanning to see what we were dealing with in S2. I noticed that we had to deal with the same things he described: two different types of integrity blocks, with both types appearing in intact and split forms. Since these cases need to be handled differently, I assumed patching the comparisons themselves would be easier.

You can ignore code healing for now and just assume our patches are effective and do not get changed by the game. I will explain code healing in the next chapter.

It is true that, with this approach, the checksum comparisons no longer fail. Technically, that sounds like it should satisfy Arxan, right? Apparently not. The calculated checksum still gets written to a specific location in the `.text` section. There are a couple of checks that use these checksums from the `.text` section and adjust the `RSP` register based on the result. This means that, if our calculated checksums are wrong, the game will still crash.

```assembly
8B 0D C9 38 FD EE                 mov     ecx, cs:dword_7020EA  ; move calculated checksum to ECX
03 0D A2 5C 11 EF                 add     ecx, cs:dword_8444C9  ; some constant?
33 0D 1D 76 8F EE                 xor     ecx, cs:dword_25E4A   ; some constant?
48 01 CC                          add     rsp, rcx  ; adjust stack pointer based on the result of the calculation
E9 79 32 85 00                    jmp     loc_11F81AAE
```

If I am not mistaken, the result of this computation is intended to be `0`, so if the checksums are correct, the stack pointer is left alone. If the checksums are wrong, it will most likely increase `RSP` by some unexpected value and cause the game to crash.

### Second approach to patch integrity checks

Since there is code that later reuses these calculated checksums to adjust `RSP`, patching only the comparison is not enough. If the wrong calculated checksum gets written back into the `.text` section, the later stack-pointer calculation can still corrupt `RSP` and crash the game.

Because of that, I assumed it would be safer to use Momo's approach again: replace the calculated checksum with the correct checksum on the stack before it gets written back. That way, both the immediate checksum comparison and the later `RSP`-based control-flow check see the value Arxan expects.

So, back to the drawing board. Mallgrab described that, in Cold War, there are two different types of integrity code blocks, and each of those can appear in intact or split form.

```assembly
; An example of a non-obfuscated intact integrity block using a smaller stack frame (RBP up to 0x90)

                        sub_11B8B386:
8B 45 68                          mov     eax, [rbp+68h]
48 63 C0                          movsxd  rax, eax
48 8B 55 40                       mov     rdx, [rbp+40h]
8B 4D 68                          mov     ecx, [rbp+68h]
48 63 C9                          movsxd  rcx, ecx
8B 04 82                          mov     eax, [rdx+rax*4]
48 8D 15 1A BC BA EE              lea     rdx, dword_736FBA
89 04 8A                          mov     [rdx+rcx*4], eax
83 45 68 FF                       add     dword ptr [rbp+68h], 0FFFFFFFFh
8B 45 68                          mov     eax, [rbp+68h]
85 C0                             test    eax, eax
0F 8D D4 FF FF FF                 jge     sub_11B8B386
E9 C9 D1 BF FF                    jmp     loc_11788580
```

```assembly
; An example of a non-obfuscated intact integrity block using a larger stack frame (RBP up to 0x120)

                        loc_1143BB61:
8B 85 14 01 00 00                 mov     eax, [rbp+114h]
48 63 C0                          movsxd  rax, eax
48 8B 95 E8 00 00 00              mov     rdx, [rbp+0E8h]
8B 8D 14 01 00 00                 mov     ecx, [rbp+114h]
48 63 C9                          movsxd  rcx, ecx
8B 04 82                          mov     eax, [rdx+rax*4]
48 8D 15 39 64 BB 00              lea     rdx, dword_11FF1FBD
89 04 8A                          mov     [rdx+rcx*4], eax
83 85 14 01 00 00 FF              add     dword ptr [rbp+114h], 0FFFFFFFFh
8B 85 14 01 00 00                 mov     eax, [rbp+114h]
85 C0                             test    eax, eax
0F 8D C5 FF FF FF                 jge     loc_1143BB61
E9 29 9B 30 00                    jmp     loc_117456CA
```

At first, these two examples look very similar. The main difference is that the integrity block using the larger stack frame needs more bytes for some instructions. For example, compare the `add dword ptr [rbp+?h], 0FFFFFFFFh` instruction in both snippets. In the first example, it uses 4 bytes. In the second example, it uses 7 bytes.

If we look at both situations at runtime, we can notice that in the first example the value of `RAX` is always `0`. However, in the second example, `RAX` starts at `4` and decreases by `1` each loop until it reaches `0`. Essentially, this code behaves like a loop that fetches calculated checksums from a table and writes them to checksum locations in the `.text` section. For T7, this did not really have to be treated as a checksum table loop, because `RAX` was always `0`. In S2, we have to handle both cases: the single-checksum version and the version that starts at index `4` and loops over multiple checksum entries.

<img width="1138" height="496" alt="small-integrity-block-stack-layout" src="https://github.com/user-attachments/assets/0ef047ac-a255-4c9f-9dab-dd8cdf52b1f0" />

If we look at the stack layout of the small stack frame integrity blocks, we can see a few interesting values. First, there is the table index, marked in yellow. For the small stack frame integrity blocks, this index is always `0`. Next, there are two important pointers. The first pointer, marked in blue, points to the table of calculated checksums. In this case, that table only contains one entry and lives on the stack itself. The second pointer, marked in green, points to the table of expected/correct checksums inside the `.text` section. This table also only contains one entry for the small stack frame variant. Because both tables only contain one entry, the table index can only be `0` here.

The exact stack offsets can differ between integrity blocks. For example, in the small stack frame example above, the table index is stored at `RBP + 0x68`, and the pointer to the calculated checksum table is loaded from `RBP + 0x40`.

```assembly
8B 45 68                          mov     eax, [rbp+68h]   ; table index
48 8B 55 40                       mov     rdx, [rbp+40h]   ; calculated checksum table
8B 04 82                          mov     eax, [rdx+rax*4] ; calculated checksum[index]
```

Because these offsets are not always identical, Momo's approach does not rely on hardcoding one specific `RBP + offset`. Instead, it walks the stack and looks for values that match the expected layout.

The heuristic is roughly:
1. Find a pointer that points to a nearby stack address.
2. Check whether the value at that address matches the current calculated checksum.
3. Check whether the next relevant pointer points into the `.text` section.
4. If those conditions match, we most likely found the integrity handler context.

In other words, the patch searches for the integrity context by its shape rather than by fixed stack offsets. This matters because different integrity blocks can use different `RBP` offsets while still following the same overall layout.

<img width="1125" height="895" alt="big-integrity-block-stack-layout" src="https://github.com/user-attachments/assets/66489048-fb52-4658-bc92-39e9b37dcfeb" />

The layout of the large stack frame integrity blocks is very similar, but instead of checksum tables of size 1, we have checksum tables of size 5. The table index, marked in yellow, is `4` and decreases every iteration, as mentioned before.

For some reason, there is a reversed checksum at `RBP + 0x10`, marked in purple, which is the reversed checksum of the checksum at the fourth index. It seems that we do not need to overwrite this. I am not exactly sure what its purpose is, but ignoring it does not seem to cause any problems, so I did not look further into it.

In both cases, the goal is to overwrite the calculated checksums with the expected/correct checksums if they differ. If we do that correctly, the protected regions can be patched without triggering the integrity checks.

My current implementation is based on Momo's patch. The source code can be found here:
[integrity.cpp](./arxan/integrity.cpp)

## Code healing

After my attempts to patch the integrity checks, the game kept crashing. I then noticed, after placing multiple breakpoints on different checks, that the code was somehow changing. This immediately raised suspicion, since Mallgrab had described this phenomenon.

Placing a hardware breakpoint on my patched bytes brought me to code that was overwriting bytes in the `.text` section. There are mainly two types of code healing happening. One variant overwrites byte by byte, while another overwrites 4 bytes at a time.

```assembly
; An example of a non-obfuscated code healing block overwriting 1 byte at a time inside a loop
                    loc_1B7522:
48 8B 45 18                       mov     rax, [rbp+18h]
48 8B 55 10                       mov     rdx, [rbp+10h]
0F B6 00                          movzx   eax, byte ptr [rax]
88 02                             mov     [rdx], al
83 45 20 FF                       add     dword ptr [rbp+20h], 0FFFFFFFFh
48 83 45 10 01                    add     qword ptr [rbp+10h], 1
48 83 45 18 01                    add     qword ptr [rbp+18h], 1
8B 45 20                          mov     eax, [rbp+20h]
85 C0                             test    eax, eax
0F 85 DA FF FF FF                 jnz     loc_1B7522
E9 BD 57 01 00                    jmp     loc_1CCD0A
```

The `RDX` register holds the destination address inside the `.text` section that will be overwritten. This loop writes one byte per iteration and increments `RDX` by 1 each time.

```assembly
; An example of a non-obfuscated code healing block overwriting 4 bytes at a time inside a loop
                   loc_113A8CB1:
48 8B 45 18                       mov     rax, [rbp+18h]
48 8B 55 10                       mov     rdx, [rbp+10h]
8B 00                             mov     eax, [rax]
89 02                             mov     [rdx], eax
8B 45 20                          mov     eax, [rbp+20h]
83 C0 FC                          add     eax, 0FFFFFFFCh
89 45 20                          mov     [rbp+8+arg_10], eax
48 8B 45 10                       mov     rax, [rbp+10h]
48 83 C0 04                       add     rax, 4
48 89 45 10                       mov     [rbp+10h], rax
48 8B 45 18                       mov     rax, [rbp+18h]
48 83 C0 04                       add     rax, 4
48 89 45 18                       mov     [rbp+18h], rax
8B 45 20                          mov     eax, [rbp+20h]
83 F8 04                          cmp     eax, 4
0F 83 C7 FF FF FF                 jnb     loc_113A8CB1
E9 8D B8 BF 00                    jmp     loc_11FA457C
```

The same idea applies here. The `RDX` register holds the destination address inside the `.text` section, but this version writes 4 bytes per iteration and increments `RDX` by 4 each time.

There are some other patterns present besides the ones shown here, mainly the split variants and a single outlier. However, the concept is still the same: the code behaves slightly differently, but it still overwrites our Arxan patches.

To prevent this code from overwriting our patches, I keep a list of the Arxan patches I apply, including the size of each patch.

Completely disabling this code healing logic is not safe, because it also performs legitimate runtime adjustments to existing code. Disabling it would crash the game. Instead, we check what it is trying to overwrite and whether that range overlaps with one of our patches. If it overlaps with one of our patches, we skip the overwrite. If it does not overlap with one of our patches, we allow the write to continue.

This stops it from corrupting our patches, and in my testing the game no longer crashes.

My implementation of code healing can be found here:
[code_healing.cpp](./arxan/code_healing.cpp)

## Summary

| Area | Problem | Patch idea |
| --- | --- | --- |
| Anti-debug | Arxan maps two private regions based on `ntdll` and calls code from those regions, bypassing hooks placed on the normally loaded `ntdll.dll`. | Detect those allocations, redirect them back to the original loaded `ntdll + 0x1000`, and skip the copy loops so hooks on the normal `ntdll` functions can take effect. |
| Integrity checks | Patching checksum comparisons alone is not enough, because the calculated checksums are later reused in stack-pointer calculations such as `add rsp, rcx`. A wrong checksum can still corrupt `RSP` and crash the game. | Replace the calculated checksums with the expected checksums on the stack before they are written back to the `.text` section. |
| Code healing | Arxan can rewrite bytes in the `.text` section at runtime, which can restore or corrupt our patches. | Track the Arxan patches and skip code-healing writes when the destination range overlaps one of our patched ranges. |

## Final thoughts

This took quite some time to get working reliably. I am not completely sure that every part of my implementation is perfect, and there may still be details I have missed. However, the current result seems stable in my testing, and I have not seen any recent crashes that appear to be caused by Arxan.

I still consider myself to be learning, especially compared to many others in the reverse engineering community. If you notice something incorrect or unclear, feel free to let me know.

My goal with this write-up was not to present a definitive guide, but to document the path I took, the assumptions I made, and the things that helped me understand the problem better. Hopefully, it gives some useful insight to others interested in game modding, anti-tamper systems, or reverse engineering.

I also want to thank Momo5502, Mallgrab, the Auroramod team, and everyone else who has shared research in this area. Their work made all of this much easier to understand and inspired me to document my own findings as well.
