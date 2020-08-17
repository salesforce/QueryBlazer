set -e

ENCODER=$1
LM=$2
OUTPUT=$3

export PATH=third_party/openfst-1.7.7/build/src/bin:third_party/ngram-1.3.10/build/src/bin:$PATH

# extract output symbols from the encoder
fstprint --save_osymbols=osyms.txt $ENCODER /dev/null

ngramread --ARPA --symbols=osyms.txt $LM | fstrelabel --relabel_ipairs=script/relabel.txt | fstarcsort | fstconvert --fst_type=const - $OUTPUT
