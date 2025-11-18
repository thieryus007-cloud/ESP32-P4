# Optimisations suggérées pour le module UART TinyBMS

Cette note regroupe des pistes concrètes pour réduire la latence et la charge CPU du client UART TinyBMS (`components/tinybms_client/tinybms_client.c`). Les recommandations sont classées par impact et effort.

## 1. Réduire le polling bloquant lors de la réception
- Aujourd'hui, chaque lecture/écriture attend dans une boucle `uart_read_bytes` avec un timeout de 50 ms puis tente d'extraire une trame à chaque itération, ce qui monopolise la tâche pendant toute la durée du timeout.【F:components/tinybms_client/tinybms_client.c†L159-L195】【F:components/tinybms_client/tinybms_client.c†L227-L274】
- **Suggestion :** installer une file d'événements UART (`uart_driver_install` avec un handle de queue) et créer une tâche dédiée qui réagit aux événements RX ou au seuil FIFO. Cela permet de dormir entre les événements, d'éviter la boucle de polling et de réduire la latence de traitement des trames complètes.
- **Alternative légère :** utiliser `uart_set_rx_timeout()` pour réduire le temps bloquant par paquet et `uart_get_buffered_data_len()` pour ne boucler que lorsqu'il reste des octets en FIFO.

## 2. Ajuster les tailles et politiques de buffers
- Le driver est installé avec un buffer RX fixe de 1024 octets et aucun buffer TX, ce qui force les écritures à être bloquantes et rend le système sensible aux pics de trafic.【F:components/tinybms_client/tinybms_client.c†L112-L119】
- **Suggestion :** passer le buffer RX à la taille maximale de trame attendue + marge (ex. `2 * TINYBMS_MAX_FRAME_LEN`) et activer un buffer TX pour supprimer les `uart_wait_tx_done` bloquants dans le thread appelant. Coupler cela à `uart_set_tx_fifo_full_threshold()`/`uart_set_rx_full_threshold()` pour lisser la charge.

## 3. Limiter les flushs systématiques
- Chaque lecture ou écriture vide les buffers UART avant d'émettre, ce qui peut supprimer des octets valides en cas de trames chevauchantes et ajoute du délai inutile.【F:components/tinybms_client/tinybms_client.c†L146-L147】【F:components/tinybms_client/tinybms_client.c†L214-L215】
- **Suggestion :** ne purger que lors d'erreurs CRC ou de timeouts répétés, ou bien uniquement à l'initialisation. Sur un bus RS485 partagé, privilégier `uart_flush_input()` (RX uniquement) et laisser la FIFO TX intacte pour garder le pipeline plein.

## 4. Déporter la sérialisation dans une tâche unique
- Les fonctions publiques verrouillent un mutex et effectuent lecture/écriture + vérification dans le même contexte, ce qui bloque l'appelant jusqu'à la fin des échanges.【F:components/tinybms_client/tinybms_client.c†L352-L399】【F:components/tinybms_client/tinybms_client.c†L401-L466】
- **Suggestion :** créer une tâche UART unique consommant une queue de requêtes (lecture/écriture), ce qui :
  - réduit les temps d'attente des tâches clientes (post d'une requête puis notification via callback/événement),
  - permet de mutualiser la temporisation entre transactions (éviter plusieurs `vTaskDelay`),
  - facilite la mise en place d'un pipeline lecture/écriture séquencé et priorisé.

## 5. Durcir la vérification de trame
- La fonction `tinybms_extract_frame` est appelée à chaque ajout d'octets sans garder de contexte de parsing, ce qui multiplie les analyses sur des buffers partiellement remplis.【F:components/tinybms_client/tinybms_client.c†L172-L189】【F:components/tinybms_client/tinybms_client.c†L240-L266】
- **Suggestion :** maintenir un état de parseur (offset courant, longueur attendue après détection d'un header) pour court-circuiter l'appel à l'extracteur tant que la longueur minimale n'est pas atteinte. On peut aussi vérifier un préambule avant d'appeler la fonction complète pour limiter le coût CPU.

## 6. Instrumentation pour tuner les timeouts
- Les timeouts actuels sont fixes (`TINYBMS_TIMEOUT_MS`, attentes de 50 ms sur RX et 100 ms sur TX).【F:components/tinybms_client/tinybms_client.c†L156-L167】【F:components/tinybms_client/tinybms_client.c†L224-L235】【F:components/tinybms_client/tinybms_client.c†L423-L434】
- **Suggestion :** exposer ces délais via la configuration ou les ajuster dynamiquement en fonction du nombre de CRC/timeouts observés (`g_ctx.stats`). On peut aussi mesurer le temps réel entre requête et réponse et calculer un percentile pour auto-ajuster le budget de timeout.

En appliquant progressivement ces optimisations (commencer par la gestion d'événements RX et l'ajustement des buffers), on devrait diminuer les blocs actifs dans les tâches de communication et améliorer la robustesse face aux bursts ou aux perturbations sur le bus.
