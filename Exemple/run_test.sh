#!/bin/bash
# Script de lancement automatique pour test_tinybms.py
# Compatible macOS avec Homebrew Python (environnement virtuel automatique)
# Usage: ./run_test.sh [port]
# Example: ./run_test.sh /dev/tty.usbserial-0001

set -e  # Exit on error

# Couleurs pour l'affichage
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
VENV_DIR="$SCRIPT_DIR/.venv"

echo -e "${BLUE}=================================================${NC}"
echo -e "${BLUE}  Script de lancement TinyBMS Test${NC}"
echo -e "${BLUE}  Compatible macOS + Homebrew Python${NC}"
echo -e "${BLUE}=================================================${NC}"
echo ""

# 1. Vérifier Python3
echo -e "${YELLOW}[1/6] Vérification de Python3...${NC}"
if ! command -v python3 &> /dev/null; then
    echo -e "${RED}❌ Python3 n'est pas installé${NC}"
    exit 1
fi
PYTHON_VERSION=$(python3 --version)
echo -e "${GREEN}✅ $PYTHON_VERSION${NC}"
echo ""

# 2. Créer/Activer l'environnement virtuel (pour macOS/Homebrew)
echo -e "${YELLOW}[2/6] Configuration de l'environnement virtuel...${NC}"
if [ ! -d "$VENV_DIR" ]; then
    echo -e "${YELLOW}📦 Création de l'environnement virtuel...${NC}"
    python3 -m venv "$VENV_DIR"
    echo -e "${GREEN}✅ Environnement virtuel créé${NC}"
else
    echo -e "${GREEN}✅ Environnement virtuel existant${NC}"
fi

# Activer l'environnement virtuel
source "$VENV_DIR/bin/activate"
echo -e "${GREEN}✅ Environnement virtuel activé${NC}"
echo ""

# 3. Vérifier/Installer pyserial
echo -e "${YELLOW}[3/6] Vérification de pyserial...${NC}"
if python3 -c "import serial" 2>/dev/null; then
    SERIAL_VERSION=$(python3 -c "import serial; print(serial.__version__)")
    echo -e "${GREEN}✅ pyserial version $SERIAL_VERSION${NC}"
else
    echo -e "${YELLOW}📦 Installation de pyserial...${NC}"
    pip install pyserial
    echo -e "${GREEN}✅ pyserial installé${NC}"
fi
echo ""

# 4. Identifier le port série
echo -e "${YELLOW}[4/6] Identification du port série...${NC}"
if [ -n "$1" ]; then
    PORT="$1"
    echo -e "${BLUE}ℹ️  Port spécifié: $PORT${NC}"
else
    # Port par défaut pour macOS
    PORT="/dev/tty.usbserial-0001"
    echo -e "${BLUE}ℹ️  Port par défaut: $PORT${NC}"

    # Vérifier si le port existe, sinon essayer de détecter
    if [ ! -e "$PORT" ]; then
        echo -e "${YELLOW}⚠️  Port par défaut non trouvé, tentative de détection...${NC}"
        PORTS=$(ls /dev/tty.usb* /dev/cu.usb* 2>/dev/null || true)
        if [ -n "$PORTS" ]; then
            PORT=$(echo "$PORTS" | head -1)
            echo -e "${GREEN}✅ Port auto-détecté: $PORT${NC}"
        else
            echo -e "${RED}❌ Aucun port série détecté${NC}"
            echo -e "${YELLOW}Ports disponibles:${NC}"
            ls /dev/tty.* /dev/cu.* 2>/dev/null | grep -i "usb\|serial" || echo "  Aucun"
            echo ""
            echo -e "${YELLOW}Spécifiez le port manuellement:${NC}"
            echo "  ./run_test.sh /dev/tty.usbserial-XXXX"
            deactivate
            exit 1
        fi
    else
        echo -e "${GREEN}✅ Port trouvé: $PORT${NC}"
    fi
fi
echo ""

# 5. Vérifier que le port existe
echo -e "${YELLOW}[5/6] Vérification du port...${NC}"
if [ ! -e "$PORT" ]; then
    echo -e "${RED}❌ Le port $PORT n'existe pas${NC}"
    echo ""
    echo -e "${YELLOW}Ports série disponibles:${NC}"
    ls /dev/tty.* /dev/cu.* 2>/dev/null | grep -i "usb\|serial" || echo "  Aucun port USB/série trouvé"
    echo ""
    echo -e "${YELLOW}Vérifiez que:${NC}"
    echo "  - Le câble USB est bien branché"
    echo "  - L'appareil est reconnu par le système"
    deactivate
    exit 1
fi

# Test de connexion
echo -e "${YELLOW}Test de connexion au port série...${NC}"
if python3 -c "import serial; s = serial.Serial('$PORT', 115200, timeout=1); s.close()" 2>/dev/null; then
    echo -e "${GREEN}✅ Port série accessible${NC}"
else
    echo -e "${RED}❌ Impossible d'ouvrir le port série${NC}"
    echo -e "${YELLOW}Vérifiez que:${NC}"
    echo "  - Le TinyBMS est alimenté"
    echo "  - Le câble USB est bien branché"
    echo "  - Aucun autre programme n'utilise le port"
    deactivate
    exit 1
fi
echo ""

# 6. Lancement du script
echo -e "${BLUE}=================================================${NC}"
echo -e "${BLUE}  Lancement du script de test TinyBMS${NC}"
echo -e "${BLUE}  Port: $PORT${NC}"
echo -e "${BLUE}=================================================${NC}"
echo ""

# Lancer le script Python
python3 "$SCRIPT_DIR/test_tinybms.py" "$PORT"

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

# Désactiver l'environnement virtuel
deactivate

exit $EXIT_CODE
