# Document de Mise en Œuvre : Tiny BMS s516 - Mode Single Port "High Power" avec Pré-charge

Ce document décrit le câblage, le principe de fonctionnement et la programmation des registres pour utiliser le Tiny BMS en tant que superviseur d'un système de forte puissance (ex: Victron) avec un seul port de charge/décharge commun.

## 1. Principe de Fonctionnement

Dans cette configuration, le Tiny BMS ne conduit pas le courant de puissance. Il agit comme un contrôleur qui pilote des relais externes.
* **Architecture "Single Port" :** Le chargeur et la charge (Load) partagent le même point de connexion.
* **Protection :** Le BMS surveille les tensions cellules et le courant global (via capteur LEM). En cas de défaut (surtension, sous-tension, surintensité), il coupe les relais externes.
* **Pré-charge :** Avant de fermer le contacteur principal, le BMS active un circuit secondaire (résistance) pour charger les condensateurs de l'onduleur en douceur, évitant les arcs électriques et la soudure des contacts.

---

## 2. Schéma de Câblage Détaillé

### A. Circuit de Puissance (Batterie <-> Victron)
Le courant fort circule directement de la batterie vers l'équipement sans traverser le PCB du BMS.

1.  **Positif (+48V) :**
    * Connecter directement la borne **Positive de la Batterie** à la borne **Positive du Victron**.
    * Connecter la borne **B+** du Tiny BMS à ce même positif (via un fil de section plus faible, ex: 1mm², pour l'alimentation du BMS).

2.  **Négatif (-) et Relais Principal :**
    * Partir de la borne **Négative de la Batterie**.
    * Passer à travers le capteur de courant **LEM DHAB S/133** (Attention au sens de la flèche : Batterie -> Charge).
    * Connecter à l'entrée du **Relais de Puissance Principal (Main Contactor)**.
    * La sortie du Relais Principal va à la borne **Négative du Victron**.
    * Connecter la borne **B-** du Tiny BMS directement au Négatif de la Batterie (avant le LEM).

3.  **Circuit de Pré-charge (Bypass) :**
    * Connecter une **Résistance de Puissance** en série avec un **Relais de Pré-charge** (plus petit que le principal).
    * Connecter cet ensemble en parallèle du Relais Principal (c'est-à-dire : une patte avant le contacteur principal, une patte après).

### B. Câblage de Commande et Détection (BMS)

1.  **Pilotage du Relais Principal (Sortie AIDO2) :**
    * Le BMS utilise des sorties "Open Drain" (mise à la masse).
    * Connecter la borne A1 (bobine +) du Relais Principal au +48V Batterie (ou +12V via convertisseur si bobine 12V).
    * Connecter la borne A2 (bobine -) à la broche **AIDO2** du BMS.
    * **IMPORTANT :** Installer une **Diode de Roue Libre** (ex: 1N4007) en parallèle de la bobine (Cathode sur A1/+, Anode sur A2/-) pour protéger le BMS.

2.  **Pilotage du Relais de Pré-charge (Sortie AIDO1) :**
    * Connecter la borne A1 (bobine +) du Relais Pré-charge au +48V Batterie.
    * Connecter la borne A2 (bobine -) à la broche **AIDO1** du BMS.
    * **IMPORTANT :** Installer également une **Diode de Roue Libre** sur la bobine.

3.  **Détection de Tension (Borne C-) :**
    * Connecter la borne **C-** du Tiny BMS au **Négatif du Victron** (côté "Charge" du relais principal) avec un fil fin.
    * *Rôle :* Cela permet au BMS de mesurer la tension côté chargeur/charge pour savoir quand un équipement est connecté, même si le contacteur est ouvert.

4.  **Capteur LEM :**
    * Connecter la fiche du capteur LEM au connecteur "External Current Sensor" du BMS.

---

## 3. Programmation des Registres (Battery Insider)

Voici la configuration logicielle stricte pour ce fonctionnement, basée sur le mapping des registres. Ces réglages se font dans l'onglet *Peripherals* ou via l'éditeur de registres.

| Registre | Nom du Paramètre | Valeur (Décimale) | Description Fonctionnelle |
| :--- | :--- | :--- | :--- |
| **340** | **BMS Operation Mode** | **1** | Définit le mode **Single Port**. Impératif pour une connexion commune charge/décharge. |
| **341** | **Single Port Switch Type** | **2** | Assigne la commande du contacteur principal à la sortie **AIDO2** (Pin 6). |
| **337** | **Precharge Pin** | **3** | Assigne la commande du relais de pré-charge à la sortie **AIDO1** (Pin 4). |
| **338** | **Precharge Duration** | **7** | Définit la durée de pré-charge à **5 secondes** (Code 0x07). Ajustable selon la capacité d'entrée du Victron. |
| **335** | **Charger Detection** | **1** | Detection **Interne**. Le BMS surveille la tension sur la borne **C-** pour activer le système. |
| **330** | **Charger Type** | **1** | Type générique CC/CV. |
| **329** | **Configuration Bits** | **Variable** | Cocher "Invert External Current Sensor Direction" (Bit 1) si le BMS affiche un courant positif lors de la décharge. |
| **331** | Load Switch Type | N/A | Ignoré car le mode Single Port est actif (Reg 340=1). |
| **333** | Charger Switch Type | N/A | Ignoré car le mode Single Port est actif (Reg 340=1). |

---

## 4. Séquence de Fonctionnement

Une fois câblé et programmé, voici comment le système réagit :

1.  **État Initial :** Tous les relais sont ouverts.
2.  **Démarrage / Réveil :** Le BMS détecte une condition d'activation (bouton ON, ou tension sur C-).
3.  **Phase de Pré-charge :**
    * Le BMS active la sortie **AIDO1** (fermeture du relais de pré-charge).
    * Le courant passe à travers la résistance, limitant l'appel de courant vers les condensateurs du Victron.
    * Cette phase dure le temps défini au Registre 338 (ex: 5 secondes).
4.  **Phase de Marche (Run) :**
    * À la fin de la temporisation, le BMS active la sortie **AIDO2** (fermeture du contacteur principal).
    * Simultanément (ou quelques millisecondes après), il désactive **AIDO1** (ouverture du circuit de pré-charge).
    * Le système est opérationnel, le courant passe par le circuit de puissance principal.
5.  **Protection :**
    * Si le capteur LEM détecte une surintensité (Over-Current), le BMS coupe l'alimentation de **AIDO2**, ouvrant le circuit instantanément.
    * Si une cellule atteint le seuil critique bas (Under-Voltage), le BMS coupe **AIDO2** pour stopper la décharge.

---

## Notes Importantes

* **Sécurité électrique :** Toujours débrancher la batterie avant toute intervention sur le câblage.
* **Diodes de roue libre :** Ne jamais omettre ces diodes sur les bobines de relais, sous peine d'endommager les sorties du BMS.
* **Sens du capteur LEM :** La flèche sur le capteur doit pointer dans le sens : Batterie → Charge.
* **Test de pré-charge :** Vérifier avec un multimètre que la tension monte progressivement côté Victron pendant la phase de pré-charge avant la fermeture du contacteur principal.
* **Résistance de pré-charge :** Calculer la valeur en fonction de la tension batterie et de la capacité d'entrée de l'onduleur (généralement entre 10Ω et 100Ω, puissance 10W minimum).

---

## Références

* Tiny BMS s516 High Power User Manual Rev D
* Tiny BMS Communication Protocols Rev D
* Documentation Battery Insider (logiciel de configuration)
