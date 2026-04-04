# Nore Vim and Neovim Support

This directory is a small Vim runtime package for editing Nore source files. The same package works in both Vim and Neovim.

Version 1 includes:

- filetype detection for `*.nore`
- syntax highlighting
- basic comment settings
- simple brace-based indentation

## Use With lazy.nvim

```lua
{
  dir = "/absolute/path/to/nore/editors/vim",
  name = "nore.vim",
}
```

Replace the path with your local checkout.

## Use With Vim Packages

Create a symlink into a package directory Vim already loads:

```bash
mkdir -p ~/.vim/pack/nore/start
ln -s /absolute/path/to/nore/editors/vim ~/.vim/pack/nore/start/nore.vim
```

## Use With Neovim Packages

Create a symlink into a package directory Neovim already loads:

```bash
mkdir -p ~/.local/share/nvim/site/pack/nore/start
ln -s /absolute/path/to/nore/editors/vim ~/.local/share/nvim/site/pack/nore/start/nore.vim
```

After that, opening a `*.nore` file should set `filetype=nore` and load `syntax/nore.vim`.

## Layout

- `ftdetect/nore.vim` detects `*.nore`
- `syntax/nore.vim` defines syntax groups and highlight links
- `ftplugin/nore.vim` sets local editing options
- `indent/nore.vim` provides simple indentation

## Scope

This is intentionally regex-based. It is fast, easy to maintain next to the language source, and good enough while the syntax is still evolving.

If Nore grows enough context-sensitive syntax to justify it, a later version can add Tree-sitter support without changing this basic package layout.
