#! /bin/sh
# Compile each source file separately, as required for ccache.
# This is called by ccache-wrapper.BAT, because posix shell is better known to
# relevant people than windows batch.
set -e
set -x

# shift # 0 is bash and 1 is this sh script

echo "got: $@"

echo "$@" |grep ' /c .*\.c$' >/dev/null || {
	echo "falling through: $@"
	cl.exe "$@"
}

for a in "$@"
do
	if [ "$a" = "${a%.c}" ] || [ "$a" != "${a#[-/]}" ]
	then
		opt="$opt $a"
	else
		fn="$fn $a"
	fi
done

for f in $fn
do
	echo "running: $opt $f"
	/ProgramData/chocolatey/bin/ccache.exe cl.exe "$opt" "$f"
done
