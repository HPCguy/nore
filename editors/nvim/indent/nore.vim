if exists("b:did_indent")
    finish
endif
let b:did_indent = 1

setlocal indentexpr=<SID>GetNoreIndent()
setlocal indentkeys=o,O,0{,0},!^F

function! s:GetNoreIndent() abort
    let prev_lnum = prevnonblank(v:lnum - 1)
    if prev_lnum == 0
        return 0
    endif

    let prev_line = getline(prev_lnum)
    let cur_line = getline(v:lnum)
    let ind = indent(prev_lnum)

    if cur_line =~# '^\s*}'
        let ind -= shiftwidth()
    endif

    if prev_line =~# '{\s*$'
        let ind += shiftwidth()
    endif

    if ind < 0
        return 0
    endif
    return ind
endfunction

let b:undo_indent = "setlocal indentexpr< indentkeys<"
