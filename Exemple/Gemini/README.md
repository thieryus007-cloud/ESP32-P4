Pourquoi ce choix ?
Le TinyBMS expose une "Carte de registres" (Chapitre 3 du PDF) qui mappe toutes les données internes 
(tensions, courants, événements) et toutes les configurations sur des adresses mémoire virtuelles (de 0 à 599).

Plutôt que d'utiliser 50 commandes propriétaires différentes (une pour le voltage, une pour le courant, 
une pour la version...), nous utilisons le protocole standard MODBUS supporté par le BMS :

Lecture (0x03) : On lit des "blocs" de registres d'un coup. 
Par exemple, une seule commande demande les registres 0 à 50 
pour récupérer toutes les tensions cellules, le courant, le SOC et les températures en une seule trame.
C'est beaucoup plus rapide que de demander valeur par valeur.

Écriture (0x10) : Permet de modifier les paramètres (registres 300+) de manière fiable avec vérification CRC.

Voici l'implémentation complète avec le tableau de bord spécifique, la carte des registres exhaustive 
et la logique de "Polling" (interrogation cyclique) optimisée.

Points clés pour l'ESP32-P4 :

Performance : Le parsing se fait dans une tâche dédiée (uartTaskEntry), ce qui ne bloque absolument pas le rendu graphique LVGL.

Thread Safety : L'utilisation de xSemaphoreCreateMutex est critique. LVGL tourne dans un contexte (souvent le main loop), et l'UART dans un autre. Sans le mutex, vous pourriez afficher des valeurs corrompues (ex: Voltage High Byte du nouveau paquet + Low Byte de l'ancien paquet).

Mémoire : La structure TinyBMSData est légère. Si vous avez besoin d'historique (graphes), vous pouvez l'étendre avec des std::vector ou des buffers circulaires.

Flottants : L'ESP32-P4 a une FPU matérielle performante, donc l'utilisation de float dans la structure de données n'a pas d'impact négatif sur les performances.
