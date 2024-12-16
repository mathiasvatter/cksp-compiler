#!/bin/bash

# Füge alle .md-Dateien im aktuellen Verzeichnis hinzu, falls sie existieren
# for file in ./*.md; do
#     if [ -f "$file" ]; then
#         MD_FILES+=("$file")
#     fi
# done

MD_FILES+=(
    "bachelor.md"
    "recursive_datastructures.md"
    "Struktur.md"

)

# Gibt die gesammelten Dateien zurück
./bachelor_to_pdf.sh "${MD_FILES[@]}"
