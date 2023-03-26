SIZE=.50MB
# run in project root directory
curl -o data/sources.gz http://pizzachili.dcc.uchile.cl/texts/code/sources$SIZE.gz
curl -o data/english.gz http://pizzachili.dcc.uchile.cl/texts/nlang/english$SIZE.gz
curl -o data/dna.gz http://pizzachili.dcc.uchile.cl/texts/dna/dna$SIZE.gz

(
cd data
gzip -d sources.gz
gzip -d english.gz
gzip -d dna.gz
)
