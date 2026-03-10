# Jig

Jig is a library designed to make adding complex branching dialogue to your C/C++ completely trivial. It combines the familiarity of common programming language constructs with syntax designed to read like a screenplay, aswell as flexible game state integration. Branching conversations, situational response options, and game state modification are first class citizens. Note - this is library is for managing dialogue logic, it does not perform any UI operations. That being said, embedding the language into a custom engine is designed to be as easy as possible (see below for instructions).

## Basic Example
```
module Default

START = if (ShimmyIntroScene.IsActive) then
    <Brian: "Oh hello there. Fancy a game of cards?"> (
        "Love to." => v01
        "If that's what counts for fun around here." => v02
        if (Player.Origin == Twinvayne) then
            "Piss off Brian ya daft git." => EXIT
        end
    )
else
    <Brian: "Lovely spot this, don't you think?"> => EXIT
end

v01 = <Brian: "Good on ye."> => game
v02 = <Brian: "You need to open yourself up to new experiences friend."> => game

game = <Brian: "Why don't you pick a card, any card."> (
    "*Pick higher up*"  => pick
    "*Pick lower down*" => pick
)

pick = match (rng3) with
    (0) <Narrator: "*You draw an ace.*">            <Brian: "Lucky lucky.">     => EXIT
    (1) <Narrator: "*You draw a 3.*">               <Brian: "Bad luck friend."> => EXIT
    (_) <Narrator: "*You draw a 9. As did Brian.*"> <Brian: "Well well.">       => EXIT
end

endmod
```

## Installation
Jig compiles and links to your project out of the box with my build system [Vango](https://github.com/EmVance1/Vango). A Makefile is also provided to build a static library, although really it is as easy as compiling everything in `src`, adding `include` as an include. For vim/nvim users, the file `qsi.vim` is provided to enable basic syntax highlighting.

## Project Integration
Integrating the library into your project is equally easy. The core API consists of just 3 functions - compile, load, execute. The compiled binary format can be saved and loaded completely as is, meaning all compilation can be done ahead of time to negate load times. Below is already a complete example of what your usual skeleton may look like (see `test/test.c` for a complete CLI example). Jig works by emitting events - upcalls - from the `exec` function. These generally occur whenever player input is expected, such as choosing dialogue options (see docs for detailed communication with the vm), but user defined events may also occur.
```c
char* src = read_to_string("examples/NPCs.qsi");
jigError errors = { 0 };
jigFile* file = jig_file_compile_from_src(src, &errors, varkey_ctx, jig_malloc_allocator());
free(src);

jigVm vm;
jig_vm_init(&vm, file, "Brian");

while (true) {
    switch (jig_vm_exec(&vm, varval_ctx)) {
    case JIG_UPCALL_LINE:  /* ... */ break;
    case JIG_UPCALL_PICK:  /* ... */ break;
    case JIG_UPCALL_EVENT: /* ... */ break;
    case JIG_UPCALL_EXIT:  /* ... */ break;
    }
}

free(file);
```
A more complete real world usage example can be found in my game engine [Shimmy](https://github.com/EmVance1/ShimmyRPG) in `game/src/game/cinematic.cpp`.

## Fun Fact
As a side note, the language is, as best I can tell, Turing-complete. In some sense it directly models a turing machine, complete with state transitions and (theoretically) unbounded memory. This means that it can be abused to make poor Brian compute the 10th fibonacci number. For example. I do not claim that you *should* ever do this, I am simply stating that you can.
```
module Fib

START = <Brian: "init"> :: ( fib.i = 0, fib.a = 0, fib.b = 1 ) => loop

loop = if (fib.i < 10) then
    <Brian: "loop"> :: ( fib.temp = fib.a + fib.b, fib.a = fib.b, fib.b = fib.temp, fib.i += 1 ) => loop
else
    <Brian: "end"> => EXIT
end

endmod
```
