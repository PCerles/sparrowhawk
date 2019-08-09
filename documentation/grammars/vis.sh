NAME=$1
fstdraw --isymbols=/tts/sparrowhawk/test/string_expansion/ascii.syms --osymbols=/tts/sparrowhawk/test/string_expansion/ascii.syms -portrait $1 | dot -Tjpg -Gdpi=3500 >${1%.*}.jpg
mv ${1%.*}.jpg /data1/ben/tts/image/
