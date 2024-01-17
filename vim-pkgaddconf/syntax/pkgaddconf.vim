" Vim syntax file
" Language:     pkgadd.conf(5) configuration file
" Maintainer:   Alexandr Savca <alexandr.savca89@gmail.com>
" Last Change:  Jan 17 2024

" quit when a syntax file was already loaded.
if exists("b:current_syntax")
  finish
endif

let s:cpo_save = &cpo
set cpo&vim

syn keyword pkgaddTodo     contained TODO FIXME XXX NOTE
syn region  pkgaddComment  display oneline start="^\s*#" end="$"
                           \ contains=pkgaddTodo,@Spell

syn match   pkgaddEvent    '^\<\%(UPGRADE\|INSTALL\)\>'
                           \ nextgroup=pkgaddPattern skipwhite
syn match   pkgaddPattern  '\s\^\([^ ]\)\+\$'
                           \ nextgroup=pkgaddAction skipwhite
syn match   pkgaddAction   '\s\<\%(YES\|NO\)\>' skipwhite

hi def link pkgaddTodo     Todo
hi def link pkgaddComment  Comment
hi def link pkgaddEvent    Identifier
hi def link pkgaddPattern  Special
hi def link pkgaddAction   Constant

let b:current_syntax = "pkgaddconf"

let &cpo = s:cpo_save
unlet s:cpo_save

" End of file.
