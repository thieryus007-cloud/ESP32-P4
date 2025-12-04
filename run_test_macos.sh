#!/bin/bash
# Script pour macOS - Configure automatiquement l'environnement et lance le test

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
VENV_DIR="$SCRIPT_DIR/.venv"

echo "🔧 Configuration de l'environnement de test TinyBMS pour macOS"
echo "=============================================================="

# Créer l'environnement virtuel s'il n'existe pas
if [ ! -d "$VENV_DIR" ]; then
    echo "📦 Création de l'environnement virtuel..."
    python3 -m venv "$VENV_DIR"
    echo "✅ Environnement virtuel créé"
fi

# Activer l'environnement virtuel
echo "🔄 Activation de l'environnement virtuel..."
source "$VENV_DIR/bin/activate"

# Installer pyserial s'il n'est pas installé
if ! python3 -c "import serial" 2>/dev/null; then
    echo "📦 Installation de pyserial..."
    pip install pyserial
    echo "✅ pyserial installé"
else
    echo "✅ pyserial déjà installé"
fi

# Lancer le test
echo ""
echo "🚀 Lancement du test TinyBMS..."
echo "=============================================================="
python3 "$SCRIPT_DIR/Exemple/test_tinybms.py" "$@"

# Désactiver l'environnement virtuel
deactivate
