" Vim syntax file
" Language:     pkgadd.conf(5) configuration file
" Maintainer:   Alexandr Savca <alexandr.savca89@gmail.com>
" Last Change:  December 31, 2024

" Quit when a syntax file was already loaded.
if exists("b:current_syntax")
  finish
endif

" We need nocompatible mode in order to continue lines with backslashes.
" Original setting will be restored.
let s:cpo_save = &cpo
set cpo&vim

" Comment.
syn keyword pkgadd_todo    TODO FIXME XXX NOTE
                           \ contained

syn region  pkgadd_comment start="^\s*#" end="$"
                           \ display
                           \ oneline
                           \ contains=pkgadd_todo,@Spell

" Event.
syn match   pkgadd_event   '^\<\%(UPGRADE\|INSTALL\)\>'
                           \ nextgroup=pkgadd_pattern
                           \ skipwhite
" Pattern.
syn match   pkgadd_pattern '\s\^\([^ ]\)\+\$'
                           \ nextgroup=pkgadd_action
                           \ skipwhite
" Action.
syn match   pkgadd_action  '\s\<\%(YES\|NO\)\>$'
                           \ skipwhite

" Define the default highlighting.
hi def link pkgadd_todo    Todo
hi def link pkgadd_comment Comment
hi def link pkgadd_event   Identifier
hi def link pkgadd_pattern Special
hi def link pkgadd_action  Constant

let b:current_syntax = "pkgaddconf"

" Put cpoptions back the way we found it.
let &cpo = s:cpo_save
unlet s:cpo_save

" vim: cc=72
" End of file.
