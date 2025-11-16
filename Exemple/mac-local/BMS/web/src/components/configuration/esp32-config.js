/**
 * @file esp32-config.js
 * @brief Gestion de la configuration ESP32-CAN-X2 (UART, Wi-Fi, CAN, Identité)
 *
 * Cette logique s'appuie sur :
 *   - index.html (section "Configuration ESP32-CAN-X2")
 *   - L'API backend : 
 *       GET  /api/config  -> retourne l'objet de configuration courant
 *       POST /api/config  -> accepte l'objet de configuration à sauvegarder
 *
 * Hypothèse : l'objet JSON a des clés identiques aux attributs "name"
 * des champs HTML (device_name, uart_baudrate, wifi_sta_ssid, ...).
 */

(function () {
  const API_CONFIG = '/api/config';

  const state = {
    currentConfig: null,   // Dernier JSON reçu de /api/config
    isLoading: false,
    isSaving: false,
  };

  document.addEventListener('DOMContentLoaded', initEsp32Config);

  async function initEsp32Config() {
    // Si la section n’est pas présente, on sort (autre page)
    const configContent = document.getElementById('esp32-config-content');
    if (!configContent) {
      console.warn('[ESP32 Config] Section #esp32-config-content non trouvée, init ignorée.');
      return;
    }

    console.log('[ESP32 Config] Initialisation…');

    // Brancher boutons / formulaires
    wireFormsAndButtons();

    // Charger la configuration initiale
    await reloadConfig();
  }

  /**
   * Attache tous les écouteurs sur les formulaires et les boutons
   */
  function wireFormsAndButtons() {
    // Bouton global "Rafraîchir"
    const refreshBtn = document.getElementById('config-refresh');
    if (refreshBtn) {
      refreshBtn.addEventListener('click', (e) => {
        e.preventDefault();
        reloadConfig();
      });
    }

    // Formulaires de chaque onglet
    wireForm('device-uart-form', 'device-uart-reset');
    wireForm('wifi-sta-form', 'wifi-sta-reset');
    wireForm('wifi-ap-form', 'wifi-ap-reset');
    wireForm('can-bus-form', 'can-bus-reset');
    wireForm('identity-form', 'identity-reset');
  }

  /**
   * Attache submit + reset sur un formulaire donné
   */
  function wireForm(formId, resetButtonId) {
    const form = document.getElementById(formId);
    if (!form) {
      console.warn(`[ESP32 Config] Formulaire #${formId} introuvable.`);
      return;
    }

    // Submit = Enregistrer
    form.addEventListener('submit', async (e) => {
      e.preventDefault();
      await handleSaveConfig(formId);
    });

    // Reset = réappliquer les valeurs connues du backend
    const resetBtn = document.getElementById(resetButtonId);
    if (resetBtn) {
      resetBtn.addEventListener('click', (e) => {
        e.preventDefault();
        handleResetForm(formId);
      });
    }
  }

  /**
   * Met à jour le badge de statut global (#config-status)
   */
  function setStatusBadge(text, type) {
    const badge = document.getElementById('config-status');
    if (!badge) return;

    badge.textContent = text;

    // On enlève les classes de couleur connues
    badge.classList.remove(
      'bg-secondary-lt',
      'text-secondary',
      'bg-success-lt',
      'text-success',
      'bg-danger-lt',
      'text-danger',
      'bg-warning-lt',
      'text-warning'
    );

    switch (type) {
      case 'ok':
        badge.classList.add('bg-success-lt', 'text-success');
        break;
      case 'error':
        badge.classList.add('bg-danger-lt', 'text-danger');
        break;
      case 'loading':
        badge.classList.add('bg-warning-lt', 'text-warning');
        break;
      default:
        badge.classList.add('bg-secondary-lt', 'text-secondary');
        break;
    }
  }

  /**
   * Recharge la configuration complète depuis l’API
   */
  async function reloadConfig() {
    if (state.isLoading) return;
    state.isLoading = true;
    setStatusBadge('Chargement…', 'loading');

    try {
      const config = await fetchConfig();
      state.currentConfig = config || {};
      applyConfigToAllForms();
      setStatusBadge('Synchronisé', 'ok');
      console.log('[ESP32 Config] Configuration rechargée depuis /api/config');
    } catch (err) {
      console.error('[ESP32 Config] Erreur lors du chargement:', err);
      setStatusBadge('Erreur de chargement', 'error');
    } finally {
      state.isLoading = false;
    }
  }

  /**
   * Appelle GET /api/config et retourne le JSON
   */
  async function fetchConfig() {
    const res = await fetch(API_CONFIG, { cache: 'no-store' });
    if (!res.ok) {
      throw new Error(`GET ${API_CONFIG} -> ${res.status} ${res.statusText}`);
    }
    return res.json();
  }

  /**
   * Applique la config courante à tous les formulaires
   */
  function applyConfigToAllForms() {
    if (!state.currentConfig) return;

    applyConfigToForm('device-uart-form');
    applyConfigToForm('wifi-sta-form');
    applyConfigToForm('wifi-ap-form');
    applyConfigToForm('can-bus-form');
    applyConfigToForm('identity-form');
  }

  /**
   * Applique la config courante à un formulaire spécifique
   */
  function applyConfigToForm(formId) {
    const form = document.getElementById(formId);
    if (!form || !state.currentConfig) return;

    const cfg = state.currentConfig;

    Array.from(form.elements).forEach((el) => {
      if (!el.name) return;
      if (!(el.name in cfg)) return;

      const value = cfg[el.name];

      if (el.type === 'number') {
        el.value = (value !== null && value !== undefined) ? value : '';
      } else if (el.type === 'password' || el.type === 'text') {
        el.value = (value !== null && value !== undefined) ? value : '';
      } else if (el.tagName === 'TEXTAREA') {
        el.value = (value !== null && value !== undefined) ? value : '';
      } else if (el.tagName === 'SELECT') {
        el.value = (value !== null && value !== undefined) ? value : '';
      } else {
        // Autres types non utilisés ici (checkbox, radio…)
      }
    });
  }

  /**
   * Reconstruit un objet de configuration complet à partir de TOUS les formulaires
   * → toutes les clefs = attributs "name" des champs.
   */
  function collectConfigFromForms() {
    const data = {};

    const formIds = [
      'device-uart-form',
      'wifi-sta-form',
      'wifi-ap-form',
      'can-bus-form',
      'identity-form',
    ];

    formIds.forEach((formId) => {
      const form = document.getElementById(formId);
      if (!form) return;

      Array.from(form.elements).forEach((el) => {
        if (!el.name) return;

        let value = el.value;

        if (el.type === 'number') {
          const num = Number(value);
          if (!isNaN(num)) {
            value = num;
          } else if (value === '') {
            value = null;
          }
        }

        // Pour l’instant aucun checkbox/radio dans ce HTML, donc pas de gestion spéciale

        data[el.name] = value;
      });
    });

    return data;
  }

  /**
   * Sauvegarde la config (tous formulaires) via POST /api/config
   * Appelée lorsqu’on fait "Enregistrer" sur un des formulaires.
   */
  async function handleSaveConfig(originFormId) {
    if (state.isSaving) return;

    const configToSave = collectConfigFromForms();
    state.isSaving = true;
    setStatusBadge('Enregistrement…', 'loading');

    try {
      const res = await fetch(API_CONFIG, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(configToSave),
      });

      if (!res.ok) {
        throw new Error(`POST ${API_CONFIG} -> ${res.status} ${res.statusText}`);
      }

      // Si le backend renvoie la config appliquée, on la reprend, sinon on garde ce qu’on a envoyé
      let saved = null;
      try {
        saved = await res.json();
      } catch (_) {
        saved = null;
      }
      state.currentConfig = (saved && typeof saved === 'object') ? saved : configToSave;

      // On réapplique dans les formulaires pour être sûr d’être aligné
      applyConfigToAllForms();
      setStatusBadge('Configuration enregistrée', 'ok');
      console.log(`[ESP32 Config] Configuration sauvegardée (origine: ${originFormId}).`);
    } catch (err) {
      console.error('[ESP32 Config] Erreur lors de la sauvegarde:', err);
      setStatusBadge('Erreur d’enregistrement', 'error');
    } finally {
      state.isSaving = false;
    }
  }

  /**
   * Réinitialise un formulaire avec les valeurs connues du backend
   * (sans toucher aux autres onglets)
   */
  function handleResetForm(formId) {
    if (!state.currentConfig) {
      console.warn('[ESP32 Config] Impossible de réinitialiser, aucune config chargée.');
      return;
    }

    applyConfigToForm(formId);
    setStatusBadge('Formulaire réinitialisé', 'loading'); // léger feedback
    setTimeout(() => {
      if (!state.isLoading && !state.isSaving) {
        setStatusBadge('Synchronisé', 'ok');
      }
    }, 800);
  }
})();
