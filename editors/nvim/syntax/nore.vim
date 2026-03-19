if exists("b:current_syntax")
    finish
endif

syn case match

syn keyword noreTodo TODO FIXME NOTE XXX contained

syn match noreEscape /\\[ntr0"\\']/ contained
syn region noreString start=+"+ skip=+\\\\\|\\"+ end=+"+ contains=noreEscape
syn region noreChar start=+'+ skip=+\\\\\|\\'+ end=+'+ contains=noreEscape

syn match noreFloat /\v<\d+\.\d+>/
syn match noreNumber /\v<\d+>/
syn keyword noreBoolean true false

syn keyword noreDeclaration pub native
syn keyword noreTypeKeyword value struct enum table nextgroup=noreTypeDecl skipwhite
syn keyword noreFuncKeyword func nextgroup=noreFuncDecl skipwhite
syn keyword noreImportKeyword import nextgroup=noreImportAlias skipwhite
syn keyword noreStatement val mut ref return assert break continue
syn keyword noreConditional if else match
syn keyword noreRepeat while for in
syn keyword noreType i64 i32 u8 u32 f64 bool void str Arena OS

syn keyword noreBuiltin arena arena_alloc arena_reset
syn keyword noreBuiltin table_alloc table_len table_get table_insert

syn keyword noreConstant TARGET_OS
syn match noreConstant /\<[A-Z][A-Z0-9_]*\>/

syn match noreOperator /==\|!=\|<=\|>=/
syn match noreOperator /&&/
syn match noreOperator /||/
syn match noreOperator /<</
syn match noreOperator />>/
syn match noreOperator /\.\./
syn match noreOperator /[=+\-*\/%&|^~!<>]/
syn match noreDelimiter /[(){}\[\],:.;]/

syn match noreTypeName /\<[A-Z][A-Za-z0-9_]*[a-z][A-Za-z0-9_]*\>/
syn match noreQualifiedType /\<[A-Z][A-Za-z0-9_]*\ze\./
syn match noreConstructor /\.[A-Z][A-Za-z0-9_]*\>/hs=s+1
syn match noreField /\.[a-z_][A-Za-z0-9_]*\>/hs=s+1
syn match noreSpecialField /\.len\>/hs=s+1

syn match noreTypeDecl /[A-Z][A-Za-z0-9_]*/ contained
syn match noreFuncDecl /[a-z_][A-Za-z0-9_]*/ contained
syn match noreImportAlias /[a-z_][A-Za-z0-9_]*/ contained
syn match noreMatchArm /^\s*\zs[A-Z][A-Za-z0-9_]*\ze\(\s*(\|\s*=\)/

syn match noreLineComment "//.*$" contains=noreTodo,@Spell containedin=ALLBUT,noreString,noreChar
syn region noreBlockComment start="/\*" end="\*/" contains=noreTodo,@Spell containedin=ALLBUT,noreString,noreChar

hi def link noreTodo Todo

hi def link noreLineComment Comment
hi def link noreBlockComment Comment

hi def link noreEscape SpecialChar
hi def link noreString String
hi def link noreChar Character

hi def link noreFloat Float
hi def link noreNumber Number
hi def link noreBoolean Boolean

hi def link noreDeclaration Keyword
hi def link noreTypeKeyword Keyword
hi def link noreFuncKeyword Keyword
hi def link noreImportKeyword Keyword
hi def link noreStatement Statement
hi def link noreConditional Conditional
hi def link noreRepeat Repeat
hi def link noreType Type

hi def link noreBuiltin Function
hi def link noreConstant Constant

hi def link noreOperator Operator
hi def link noreDelimiter Delimiter

hi def link noreTypeName Type
hi def link noreQualifiedType Type
hi def link noreConstructor Type
hi def link noreField Identifier
hi def link noreSpecialField Special

hi def link noreTypeDecl Type
hi def link noreFuncDecl Function
hi def link noreImportAlias Include
hi def link noreMatchArm Constant

let b:current_syntax = "nore"
