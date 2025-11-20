# Interface locale TinyBMS pour Mac mini

Cette application Node.js fournit une interface web locale exécutée sur le Mac mini. Elle communique directement avec le TinyBMS via un câble USB ↔ UART pour lire et écrire la configuration des registres.

**✅ Conforme au protocole TinyBMS Communication Protocols Revision D (2025-07-04)**

## ✨ Fonctionnalités

- **Découverte et sélection du port série TinyBMS**
- **Lecture complète des registres de configuration TinyBMS** (via `/api/registers`)
- **Écriture des registres individuels** (`POST /api/registers`) avec vérification
- **Redémarrage du TinyBMS** (`POST /api/system/restart`)
- **Interface web autonome** (HTML/CSS/JS) fournie intégralement dans `mac-local/public`
- **Tableau interactif des registres** avec filtrage par groupe et édition inline
- **Protocole UART conforme Rev D:**
  - Commande Read Individual (0x09)
  - Commande Write Individual (0x0D)
  - ACK/NACK avec codes d'erreur
  - CRC-16 MODBUS validé

## 🔌 Pré-requis

- macOS avec Node.js ≥ 18 installé (`brew install node`)
- Câble USB-UART relié au TinyBMS (3.3V TTL)
- Droits d'accès au périphérique série (généralement `/dev/tty.usbserial-*` ou `/dev/cu.usbserial-*`)

## 🚀 Installation

Dans le répertoire mac-local
cd /home/user/TinyBMS-GW/mac-local

# Installer les dépendances
npm install

# Lister les registres (sans connexion série nécessaire)
npm run list-registers

# Démarrer le serveur
npm start

Par défaut, le serveur écoute sur `http://localhost:5173`

## 🖥️ Utilisation

1. Brancher le TinyBMS au Mac via le câble USB-UART.
2. Ouvrir `http://localhost:5173` dans le navigateur du Mac mini.
3. Sélectionner le port série détecté puis cliquer sur **Se connecter**.
4. La page charge automatiquement le catalogue des registres TinyBMS et affiche un tableau interactif pour lire ou modifier les valeurs autorisées.

## 📋 Liste des registres lus/écrits

Le catalogue complet des registres TinyBMS exposés par l’interface est généré automatiquement à partir du firmware. Pour le consulter sans lancer le serveur, exécutez :

```bash
npm run list-registers
```

La commande affiche un tableau Markdown comprenant l’adresse, la clé, le libellé, les droits d’accès et le type de chaque registre.

## ⚙️ Configuration UART

**Paramètres par défaut (conformes au protocole TinyBMS Rev D):**
- **Baud rate:** 115200
- **Data bits:** 8
- **Parity:** None
- **Stop bits:** 1
- **Flow control:** None
- **Timeout:** 750ms

Ces paramètres sont optimaux pour le TinyBMS et ne devraient pas nécessiter de modification.

## 📁 Structure du module (10 fichiers, 98K)

mac-local/
├── README.md                              (Documentation)
├── package.json                           (Dépendances npm)
├── public/                                (Interface web)
│   ├── css/mac-app.css                   (Styles)
│   ├── index.html                        (Page principale)
│   └── js/mac-app.js                     (Logique client)
├── scripts/
│   └── list-registers.js                 (Utilitaire)
└── src/                                   (Backend)
    ├── generated_tiny_rw_registers.inc   (✅ Embarqué - 26K, 34 registres)
    ├── registers.js                      (Parser de registres)
    ├── serial.js                         (Communication USB-UART)
    └── server.js                         (Serveur Express)


- `data/registers.json` : catalogue précompilé des registres TinyBMS embarqué avec l'application.
- `src/registers.js` : charge le catalogue JSON embarqué (ou retombe sur le fichier généré du firmware si présent).
- `src/serial.js` : gère la communication USB-UART (construction/parsing des trames TinyBMS).
- `src/server.js` : serveur Express + API REST.
- `public/` : interface web (HTML/CSS/JS) hébergée par Express.

## 🔒 Remarques

- L'upload OTA n'est pas supporté dans cette version (renvoie HTTP 501).
- Assurez-vous qu'aucun autre service n'utilise le port série pendant la configuration.
- Le serveur doit être relancé si le périphérique USB est débranché/rebranché.
- Le dossier `mac-local/` est autonome : copiez-le tel quel sur un Mac disposant de Node.js pour utiliser l'outil sans dépendre du code ESP32.

## 🧪 Tests

Les tests automatisés ne sont pas fournis pour ce module. Vérifiez la communication en suivant les logs dans le terminal (`npm start`).

### 🔁 Mise à jour du catalogue lors du développement

Si vous travaillez depuis le dépôt complet et que le firmware évolue, régénérez le fichier `data/registers.json` avec :

```bash
npm run refresh-registers
```

La commande lit `main/config_manager/generated_tiny_rw_registers.inc` et écrase le JSON embarqué. Copiez ensuite `mac-local/` sur le Mac mini pour profiter du nouveau catalogue hors-ligne.

## 📚 Documentation

- **Référence complète des commandes:** Voir `/docs/tinybms_commands_reference.md`
- **Corrections du protocole:** Voir `/UART_PROTOCOL_FIXES.md`
- **Protocole officiel:** TinyBMS Communication Protocols Revision D (2025-07-04)

## 🔧 API REST

### Endpoints disponibles

```
GET  /api/ports                 - Lister les ports série disponibles
POST /api/connection/open       - Ouvrir connexion (body: {path, baudRate})
POST /api/connection/close      - Fermer connexion
GET  /api/connection/status     - État de la connexion

GET  /api/registers             - Lire tous les registres
GET  /api/registers?group=xxx   - Lire registres d'un groupe
POST /api/registers             - Écrire un registre (body: {key, value})

POST /api/system/restart        - Redémarrer le TinyBMS
```

### Exemple d'utilisation

```bash
# Lister les ports
curl http://localhost:5173/api/ports

# Ouvrir connexion
curl -X POST http://localhost:5173/api/connection/open \
  -H "Content-Type: application/json" \
  -d '{"path": "/dev/tty.usbserial-1234", "baudRate": 115200}'

# Lire tous les registres
curl http://localhost:5173/api/registers

# Écrire un registre
curl -X POST http://localhost:5173/api/registers \
  -H "Content-Type: application/json" \
  -d '{"key": "fully_charged_voltage_mv", "value": 4200}'

# Redémarrer TinyBMS
curl -X POST http://localhost:5173/api/system/restart
```
