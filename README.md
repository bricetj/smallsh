# smallsh

A Unix shell implemented in C, supporting built-in commands, I/O redirection, foreground/background process execution, and custom signal handling — built for CS374 (Operating Systems I) at Oregon State University.

## Features

- Interactive `:` prompt that parses and executes user commands
- Three built-in commands: `cd`, `status`, and `exit`
- Executes arbitrary shell commands by forking a child process and calling `execvp()`
- Input (`<`) and output (`>`) redirection via `dup2()`
- Foreground and background (`&`) process execution, with background PID tracking and completion reporting
- Custom `SIGINT` handling (foreground child processes terminate normally; the shell itself and background processes are unaffected)
- Custom `SIGTSTP` handling that toggles a foreground-only mode (`&` is ignored while active)

## Tech Stack

| Component | Details |
|---|---|
| Language | C (C99) |
| APIs | POSIX process control (`fork`, `execvp`, `waitpid`), signal handling (`sigaction`), file I/O (`open`, `dup2`) |
| Platform | Linux / Unix |

## Getting Started

### Prerequisites

- A Unix-like environment (Linux, macOS, WSL) with `gcc` and POSIX headers available

### Build & Run

```bash
gcc -Wall -Wextra -std=gnu99 -o smallsh smallsh.c
./smallsh
```

### Example Session

```
: ls
smallsh  smallsh.c  README.md
: ls > out.txt
: status
exit value 0
: sleep 30 &
background pid is 4224
: status
exit value 0
: exit
```

## Acknowledgments

Completed for CS374 - Operating Systems I at Oregon State University.

## Contact

Brice Jenkins — [github.com/bricetj](https://github.com/bricetj) — [linkedin.com/in/bricetj](https://linkedin.com/in/bricetj)