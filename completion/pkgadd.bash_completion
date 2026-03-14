# bash completion for pkgadd(8)
# See COPYING for license terms and COPYRIGHT for notices.

_pkgadd()
{
	local cur prev words cword split
	_init_completion -s || return

	case $prev in
	--help|--version|-!(-*)[hV])
		return
		;;
	--root|-r)
		_filedir -d
		return
		;;
	--config|-c)
		_filedir
		return
		;;
	esac

	$split && return

	if [[ $cur == -* ]]; then
		COMPREPLY=($(compgen -W '$(_parse_help "$1")' -- $cur))
		[[ ${COMPREPLY-} == *= ]] && compopt -o nospace
	else
		_filedir
	fi
} && complete -F _pkgadd pkgadd

# vim: ft=bash cc=72 tw=70
# End of file.
