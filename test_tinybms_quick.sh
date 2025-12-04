#!/bin/bash
# Script de lancement ultra-rapide pour le test TinyBMS
# Peut être lancé depuis n'importe où dans le projet

# Déterminer le répertoire du script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EXEMPLE_DIR="$SCRIPT_DIR/Exemple"

# Vérifier que le répertoire Exemple existe
if [ ! -d "$EXEMPLE_DIR" ]; then
    echo "❌ Erreur: Répertoire Exemple/ non trouvé"
    exit 1
fi

# Lancer le script de test depuis le bon répertoire
cd "$EXEMPLE_DIR"

echo "🚀 Lancement du test TinyBMS..."
echo ""

# Lancer le script automatique
if [ -x "run_test.sh" ]; then
    ./run_test.sh "$@"
else
    echo "⚠️  run_test.sh n'est pas exécutable, correction..."
    chmod +x run_test.sh
    ./run_test.sh "$@"
fi
