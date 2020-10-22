set -e

# INPUT
LOG_FILE=orcas_train.txt

# OUTPUT FILES; will overwrite
OUTPUT_DIR=data/orcas/char
LOG_ENCODED=$OUTPUT_DIR/train.enc
SPM_PREFIX=$OUTPUT_DIR/subword # $SPM_PREFIX.{m, vocab}
ENCODER=$OUTPUT_DIR/encoder.fst
LANGUAGE_MODEL=$OUTPUT_DIR/ngram # $LANGUAGE_MODEL.{arpa, fst}
PRECOMPUTED=$OUTPUT_DIR/precomputed.bin

# CONFIG
SPM_MODEL=char # char, bpe, unigram
SPM_VOCAB_SIZE=4096
SPM_CHARACTER_COVERAGE=0.9995
LM_ORDER=8
LM_PRUNE="--prune 0 1 1 2 2 3 3 4"


echo "extracting subwords..."
third_party/sentencepiece/build/src/spm_train --vocab_size $SPM_VOCAB_SIZE --model_type $SPM_MODEL --model_prefix $SPM_PREFIX --input $LOG_FILE --character_coverage $SPM_CHARACTER_COVERAGE

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
