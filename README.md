# mined
A minimal interactive ASCII text editor.

Inspired by TECO.

## Build
MINED is C99/POSIX compatible without no dependency.
``` shell
gcc -o mined mined.c
```

## Usage
MINED is a simple REPL-based regular file editor, it has 8 commands:

| Command | Syntax           | Explanation                       |
|---------|------------------|-----------------------------------|
| `o`     | `o[STRING]`      | Open a file for edit              |
| `c`     | `c`              | Close current file                |
| `p`     | `p[NUM][,[NUM]]` | Print a line or a range of lines  |
| `g`     | `g[NUM]`         | Goto begining of a line           |
| `i`     | `i[STRING]`      | Insert string at current point    |
| `d`     | `d[NUM][,[NUM]]` | Delete a line or a range of lines |
| `e`     | `e[NUM]`         | Edit a line                       |
| `q`     | `q`              | Quit                              |

The REPL's line editor provides a small set of GNU `readline` features:
- Use left/right arrow keys for cursor navigation.
- `C-a`/`C-e` for moving to beginning/end.
- `C-k` for kill, `C-y` for yank.
- Use up/down arrow keys to use the history ring.
- `C-g` for abort.
- Use `backspace`/`delete` keys for backward/foreward character deletion.

MINED is scriptable, however, `e` command can not be programmed in MINED,
because currently it only supports editing ASCII characters.

## Hints
- Every command will take effect immediately, there is NO undo!
  You may make a backup for safety before editing important files.
- You can use a single `$` to represent the last line.
- `e` command will load selected line into current line buffer so that you can
  edit it just like editing commands.  When you finished editing, press `RET` to
  commit or press `C-g` to abort.
- Mined does in-place insertion/deletion, it treats the edited file as an array.
  And it doesn't have a transaction layer to cache and reorder commands, so the
  performance may suck on some special cases.
- Mined heavily depends on `lseek`, so it does not work on streams like `sed`.
- Not fully tested yet.

## Quote
*All you need is `dd`.*
