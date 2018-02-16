#!/bin/bash

set -o errexit


cat <<EOF
Requirements
---
1. R with rmarkdown package
2. pandoc
3. SciDB with Shim listening on port 8080

Two versions of help.html will be generated. One with navigation bar
for wwwroot and one without for docs. Each of the help.html files will
be copied to the right location and override existing files.

EOF
read -p "Press ENTER to continue"


echo
echo "1/2. Generate HTML for wwwroot"
read -p "Press ENTER to continue"

Rscript -e "library(rmarkdown); render('./help.Rmd', 'html_document')"
cp help.html ../wwwroot


echo
echo "2/2. Generate HTML for docs. Remporarely rename _navbar.html then undo."
read -p "Press ENTER to continue"

mv _navbar.html _navbar.html.SKIP
Rscript -e "library(rmarkdown); render('./help.Rmd', 'html_document')"
cp help.html ../docs
mv _navbar.html.SKIP _navbar.html
