# bash completion for pkginfo(1)
# See COPYING for license terms and COPYRIGHT for notices.

_pkginfo()
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
	--footprint|-f|--owner|-o)
		_filedir
		return
		;;
	--list|-l)
		if [[ $cur == ./* || $cur == /* ]]; then
			_filedir
		else
			# Complete with all installed packages
			COMPREPLY=($(compgen -W '$(pkginfo -i | cut -d" " -f1)' -- $cur))
		fi
		return
		;;
	esac

	$split && return

	if [[ $cur == -* ]]; then
		COMPREPLY=($(compgen -W '$(_parse_help "$1")' -- $cur))
		[[ ${COMPREPLY-} == *= ]] && compopt -o nospace
		return
	fi
} && complete -F _pkginfo pkginfo

# vim: ft=bash cc=72 tw=70
# End of file.
