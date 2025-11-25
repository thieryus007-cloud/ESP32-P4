#!/bin/bash

# Script de création du projet Xcode macOS pour TinyBMS

PROJECT_NAME="TinyBMS Monitor"
PROJECT_DIR="TinyBMS-macOS-App"

echo "🚀 Création de l'application macOS TinyBMS..."
echo ""

# Vérifier que Xcode est installé
if ! command -v xcodebuild &> /dev/null; then
    echo "❌ Erreur: Xcode n'est pas installé"
    echo "Installez Xcode depuis l'App Store"
    exit 1
fi

# Créer le projet Xcode
echo "📦 Création du projet Xcode..."
cd "$PROJECT_DIR"

# Créer le projet via xcodebuild
xcodebuild -project "TinyBMS Monitor.xcodeproj" 2>/dev/null || {
    echo "🔨 Génération du projet Xcode..."

    # Note: Nous allons fournir des instructions pour créer le projet manuellement
    cat << 'INSTRUCTIONS'

📋 INSTRUCTIONS DE CRÉATION DU PROJET XCODE

Étape 1: Ouvrir Xcode
   - Lancer Xcode
   - File > New > Project

Étape 2: Choisir le template
   - Sélectionner "macOS" (en haut)
   - Choisir "App"
   - Cliquer "Next"

Étape 3: Configuration du projet
   - Product Name: TinyBMS Monitor
   - Team: (votre compte développeur)
   - Organization Identifier: com.yourname
   - Interface: SwiftUI
   - Language: Swift
   - Cocher "Use Core Data": NON
   - Cliquer "Next"

Étape 4: Sauvegarder
   - Choisir le dossier: Exemple/TinyBMS-macOS-App
   - Cliquer "Create"

Étape 5: Ajouter ORSSerialPort
   - File > Add Package Dependencies
   - Coller: https://github.com/armadsen/ORSSerialPort.git
   - Dependency Rule: "Up to Next Major Version" 2.1.0
   - Cliquer "Add Package"
   - Cocher "ORSSerialPort" pour la target
   - Cliquer "Add Package"

Étape 6: Copier les fichiers sources
   - Tous les fichiers .swift sont déjà créés dans les bons dossiers
   - Dans Xcode, supprimer les fichiers par défaut (ContentView.swift, etc.)
   - Glisser-déposer tous les dossiers Sources/* dans le projet

Étape 7: Configurer les permissions
   - Cliquer sur le projet (racine) dans le navigateur
   - Sélectionner la target "TinyBMS Monitor"
   - Onglet "Signing & Capabilities"
   - Cliquer "+" et ajouter "App Sandbox"
   - Sous Hardware, cocher "USB"

Étape 8: Build & Run
   - Sélectionner "My Mac" comme destination
   - Appuyer sur Cmd+R
   - L'application se lance !

INSTRUCTIONS

}

echo ""
echo "✅ Projet prêt !"
echo ""
echo "📂 Dossier: $PROJECT_DIR"
echo "🎯 Suivez les instructions ci-dessus pour créer le projet dans Xcode"
