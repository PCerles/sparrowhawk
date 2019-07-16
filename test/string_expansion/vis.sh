NAME=$1
fstdraw --isymbols=test.syms --osymbols=test.syms -portrait $1 | dot -Tjpg -Gdpi=400 >${1%.*}.jpg
mv ${1%.*}.jpg /data1/ben/tts/image/
