#!/bin/bash
# Script wrapper pour lancer le test TinyBMS depuis n'importe où

# Déterminer le répertoire du script
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Lancer le script Python
python3 "$SCRIPT_DIR/Exemple/test_tinybms.py" "$@"
