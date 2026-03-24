# bash completion for pkgchk(1)
# See COPYING for license terms and COPYRIGHT for notices.

_pkgchk()
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
	esac

	$split && return

	if [[ $cur == -* ]]; then
		COMPREPLY=($(compgen -W '$(_parse_help "$1")' -- $cur))
		[[ ${COMPREPLY-} == *= ]] && compopt -o nospace
	else
		# Complete with all installed packages
		COMPREPLY=($(compgen -W '$(pkginfo -i | cut -d" " -f1)' -- $cur))
	fi
} && complete -F _pkgchk pkgchk

# vim: ft=bash cc=72 tw=70
# End of file.
