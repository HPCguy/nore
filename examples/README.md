# Examples

Example programs that showcase what Nore can do. These are not tests, they are real programs built on the standard library.

## Running

```bash
# Build the compiler first
make

# Run an example
./nore --run examples/cat.nore -- <args>
```

## Programs

### cat.nore

A clone of the Unix `cat` utility. Reads files and writes their contents to stdout.

```bash
# Single file
./nore --run examples/cat.nore -- file.txt

# Multiple files
./nore --run examples/cat.nore -- file1.txt file2.txt

# No arguments prints usage
./nore --run examples/cat.nore
```

Uses: `std/io.nore`, `std/file.nore`, `std/sys.nore`

### wc.nore

A clone of the Unix `wc` utility. Counts lines, words, and bytes in files.

```bash
# Single file
./nore --run examples/wc.nore -- file.txt

# Multiple files (prints per-file counts and totals)
./nore --run examples/wc.nore -- file1.txt file2.txt

# No arguments prints usage
./nore --run examples/wc.nore
```

Uses: `std/io.nore`, `std/file.nore`, `std/sys.nore`, `std/string.nore`
