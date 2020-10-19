set -e

# INPUT
LOG_FILE=train.txt

# OUTPUT FILES; will overwrite
LOG_ENCODED=train.enc
SPM_PREFIX=subword # $SPM_PREFIX.{m, vocab}
ENCODER=encoder.fst
LANGUAGE_MODEL=ngram # $LANGUAGE_MODEL.{arpa, fst}
PRECOMPUTED=precomputed.bin

# CONFIG
SPM_MODEL=bpe # char, bpe, unigram
SPM_VOCAB_SIZE=4096
SPM_CHARACTER_COVERAGE=0.9995
LM_ORDER=5
LM_PRUNE="--prune 1 11 21 31 41"


echo "extracting subwords..."
third_party/sentencepiece/build/src/spm_train --model_type $SPM_MODEL --model_prefix $SPM_PREFIX --input $LOG_FILE --vocab_size $SPM_VOCAB_SIZE --character_coverage $SPM_CHARACTER_COVERAGE

echo "building encoder..."
cut -f 1 $SPM_PREFIX.vocab | build/qbz_build_encoder /dev/stdin $ENCODER

echo "encoding log file..."
build/qbz_encode $ENCODER $LOG_FILE > $LOG_ENCODED

echo "building language model..."
third_party/kenlm/build/bin/lmplz --order $LM_ORDER --discount_fallback $LM_PRUNE -T . < $LOG_ENCODED > $LANGUAGE_MODEL.arpa

echo "converting FST..."
sh script/build_fst_model.sh $ENCODER $LANGUAGE_MODEL.arpa $LANGUAGE_MODEL.fst

echo "precomputing beam search..."
build/qbz_build_queryblazer $ENCODER $LANGUAGE_MODEL.fst $PRECOMPUTED