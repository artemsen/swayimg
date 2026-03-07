# swayimg(1) completion

_swayimg()
{
    local cur=${COMP_WORDS[COMP_CWORD]}
    local opts="-v --viewer \
                -g --gallery \
                -s --slideshow \
                -f --from-file \
                -P --position \
                -S --size \
                -F --fullscreen \
                -c --config \
                -e --execute \
                   --class \
                   --verbose \
                -V --version \
                -h --help"
    if [[ ${cur} == -* ]]; then
        COMPREPLY=($(compgen -W "${opts}" -- "${cur}"))
     else
        _filedir
    fi
} &&
complete -o filenames -F _swayimg swayimg

# ex: filetype=sh
