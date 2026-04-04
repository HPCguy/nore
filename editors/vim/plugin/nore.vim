" Start-package installs need to load the Nore runtime pieces directly on first open.
augroup nore_filetype
    autocmd!
    autocmd BufRead,BufNewFile *.nore call s:load_nore_runtime()
augroup END

function! s:load_nore_runtime() abort
    setfiletype nore
    runtime! ftplugin/nore.vim
    runtime! indent/nore.vim
    if exists("syntax_on")
        runtime! syntax/nore.vim
    endif
endfunction
