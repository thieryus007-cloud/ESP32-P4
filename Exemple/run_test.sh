#!/bin/bash
# Script de lancement automatique pour test_tinybms.py
# Usage: ./run_test.sh [port]
# Example: ./run_test.sh /dev/ttyUSB0

set -e  # Exit on error

# Couleurs pour l'affichage
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}=================================================${NC}"
echo -e "${BLUE}  Script de lancement TinyBMS Test${NC}"
echo -e "${BLUE}=================================================${NC}"
echo ""

# 1. Vérifier Python3
echo -e "${YELLOW}[1/5] Vérification de Python3...${NC}"
if ! command -v python3 &> /dev/null; then
    echo -e "${RED}❌ Python3 n'est pas installé${NC}"
    exit 1
fi
PYTHON_VERSION=$(python3 --version)
echo -e "${GREEN}✅ $PYTHON_VERSION${NC}"
echo ""

# 2. Vérifier pyserial
echo -e "${YELLOW}[2/5] Vérification de pyserial...${NC}"
if python3 -c "import serial" 2>/dev/null; then
    SERIAL_VERSION=$(python3 -c "import serial; print(serial.__version__)")
    echo -e "${GREEN}✅ pyserial version $SERIAL_VERSION${NC}"
else
    echo -e "${RED}❌ pyserial n'est pas installé${NC}"
    echo -e "${YELLOW}Installation en cours...${NC}"
    pip3 install pyserial || python3 -m pip install pyserial
    echo -e "${GREEN}✅ pyserial installé${NC}"
fi
echo ""

# 3. Identifier le port série
echo -e "${YELLOW}[3/5] Identification du port série...${NC}"
if [ -n "$1" ]; then
    PORT="$1"
    echo -e "${BLUE}ℹ️  Port spécifié: $PORT${NC}"
else
    # Auto-détection
    PORTS=$(ls /dev/ttyUSB* /dev/ttyACM* /dev/cu.usb* 2>/dev/null || true)
    if [ -z "$PORTS" ]; then
        echo -e "${RED}❌ Aucun port série détecté${NC}"
        echo -e "${YELLOW}Vérifiez que le câble USB est branché${NC}"
        exit 1
    fi
    PORT=$(echo "$PORTS" | head -1)
    echo -e "${GREEN}✅ Port auto-détecté: $PORT${NC}"
fi

# Vérifier que le port existe
if [ ! -e "$PORT" ]; then
    echo -e "${RED}❌ Le port $PORT n'existe pas${NC}"
    exit 1
fi
echo ""

# 4. Vérifier les permissions
echo -e "${YELLOW}[4/5] Vérification des permissions...${NC}"
if [ -r "$PORT" ] && [ -w "$PORT" ]; then
    echo -e "${GREEN}✅ Permissions OK${NC}"
else
    echo -e "${RED}⚠️  Permissions insuffisantes sur $PORT${NC}"
    echo ""
    echo -e "${YELLOW}Tentative de correction...${NC}"

    # Vérifier si l'utilisateur est dans le groupe dialout
    if groups | grep -q dialout; then
        echo -e "${GREEN}✅ Vous êtes dans le groupe dialout${NC}"
        echo -e "${YELLOW}Mais le port n'est pas accessible. Essai avec sudo...${NC}"
        sudo chmod 666 "$PORT"
        echo -e "${GREEN}✅ Permissions temporaires accordées${NC}"
    else
        echo -e "${YELLOW}Vous n'êtes pas dans le groupe dialout.${NC}"
        echo ""
        echo -e "${BLUE}Options:${NC}"
        echo "  1. Ajouter votre utilisateur au groupe dialout (permanent):"
        echo "     sudo usermod -a -G dialout \$USER"
        echo "     newgrp dialout"
        echo ""
        echo "  2. Utiliser sudo pour cette fois (temporaire):"
        read -p "Voulez-vous utiliser sudo chmod maintenant? (y/n) " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            sudo chmod 666 "$PORT"
            echo -e "${GREEN}✅ Permissions temporaires accordées${NC}"
        else
            echo -e "${RED}❌ Impossible de continuer sans permissions${NC}"
            exit 1
        fi
    fi
fi
echo ""

# 5. Test de connexion
echo -e "${YELLOW}[5/5] Test de connexion au port série...${NC}"
if python3 -c "import serial; s = serial.Serial('$PORT', 115200, timeout=1); s.close()" 2>/dev/null; then
    echo -e "${GREEN}✅ Port série accessible${NC}"
else
    echo -e "${RED}❌ Impossible d'ouvrir le port série${NC}"
    echo -e "${YELLOW}Vérifiez que:${NC}"
    echo "  - Le TinyBMS est alimenté"
    echo "  - Le câble USB est bien branché"
    echo "  - Aucun autre programme n'utilise le port"
    exit 1
fi
echo ""

# Lancement du script
echo -e "${BLUE}=================================================${NC}"
echo -e "${BLUE}  Lancement du script de test TinyBMS${NC}"
echo -e "${BLUE}=================================================${NC}"
echo ""

# Aller dans le bon répertoire
cd "$(dirname "$0")"

# Lancer le script Python
python3 test_tinybms.py "$PORT"

# Code de sortie
EXIT_CODE=$?
echo ""
echo -e "${BLUE}=================================================${NC}"
if [ $EXIT_CODE -eq 0 ]; then
    echo -e "${GREEN}✅ Script terminé avec succès${NC}"
else
    echo -e "${RED}❌ Script terminé avec erreur (code: $EXIT_CODE)${NC}"
fi
echo -e "${BLUE}=================================================${NC}"

exit $EXIT_CODE
