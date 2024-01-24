" Vim syntax file
" Language:     pkgadd.conf(5) configuration file
" Maintainer:   Alexandr Savca <alexandr.savca89@gmail.com>
" Last Change:  January 24, 2024

" Quit when a syntax file was already loaded.
if exists("b:current_syntax")
  finish
endif

# We need nocompatible mode in order to continue lines with backslashes.
# Original setting will be restored.
let s:cpo_save = &cpo
set cpo&vim

" Comment.
syn keyword pkgaddTodo     contained TODO FIXME XXX NOTE
syn region  pkgaddComment  display oneline start="^\s*#" end="$"
                           \ contains=pkgaddTodo,@Spell

" Pattern.
syn match   pkgaddEvent    '^\<\%(UPGRADE\|INSTALL\)\>'
                           \ nextgroup=pkgaddPattern skipwhite
syn match   pkgaddPattern  '\s\^\([^ ]\)\+\$'
                           \ nextgroup=pkgaddAction skipwhite
syn match   pkgaddAction   '\s\<\%(YES\|NO\)\>$' skipwhite

# Define the default highlighting.
hi def link pkgaddTodo     Todo
hi def link pkgaddComment  Comment
hi def link pkgaddEvent    Identifier
hi def link pkgaddPattern  Special
hi def link pkgaddAction   Constant

let b:current_syntax = "pkgaddconf"

# Put cpoptions back the way we found it.
let &cpo = s:cpo_save
unlet s:cpo_save

" End of file.
