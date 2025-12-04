#!/usr/bin/env node

/**
 * Test de lecture de tous les registres de configuration spécifiés
 * Lit les registres : 300, 301, 303, 304, 305, 306, 307, 308, 310, 311,
 *                     315, 316, 317, 318, 319, 320, 321, 322, 323, 328
 */

const TinyBMS = require('./tinybms');

const PORT = process.argv[2] || '/dev/tty.usbserial-0001';

// Liste des registres à lire
const REGISTERS_TO_READ = [
    300, 301, 303, 304, 305, 306, 307, 308, 310, 311,
    315, 316, 317, 318, 319, 320, 321, 322, 323, 328
];

// Mapping des registres pour afficher les labels et unités
const REGISTER_INFO = {
    300: { label: 'Fully Charged Voltage', unit: 'V', scale: 0.001 },
    301: { label: 'Fully Discharged Voltage', unit: 'V', scale: 0.001 },
    303: { label: 'Early Balancing Threshold', unit: 'V', scale: 0.001 },
    304: { label: 'Charge Finished Current', unit: 'mA', scale: 1 },
    305: { label: 'Peak Discharge Current', unit: 'A', scale: 1 },
    306: { label: 'Battery Capacity', unit: 'Ah', scale: 0.01 },
    307: { label: 'Series Cells Count', unit: '', scale: 1 },
    308: { label: 'Allowed Disbalance', unit: 'mV', scale: 1 },
    310: { label: 'Charger Startup Delay', unit: 's', scale: 1 },
    311: { label: 'Charger Disable Delay', unit: 's', scale: 1 },
    315: { label: 'Over-Voltage Cutoff', unit: 'V', scale: 0.001 },
    316: { label: 'Under-Voltage Cutoff', unit: 'V', scale: 0.001 },
    317: { label: 'Discharge Over-Current', unit: 'A', scale: 1 },
    318: { label: 'Charge Over-Current', unit: 'A', scale: 1 },
    319: { label: 'Over-Heat Cutoff', unit: '°C', scale: 1 },
    320: { label: 'Low Temp Charge Cutoff', unit: '°C', scale: 1 },
    321: { label: 'Charge Restart Level', unit: '%', scale: 1 },
    322: { label: 'Max Cycles Count', unit: '', scale: 1 },
    323: { label: 'SOH Setting', unit: '%', scale: 0.002 },
    328: { label: 'Manual SOC Set', unit: '%', scale: 0.002 }
};

async function testReadAllSettings() {
    console.log('='.repeat(70));
    console.log('  Test de lecture de tous les registres de configuration');
    console.log('='.repeat(70));
    console.log();

    const bms = new TinyBMS(PORT);

    try {
        // Connexion
        console.log(`[1/3] Connexion au BMS sur ${PORT}...`);
        await bms.connect();
        console.log('✅ Connecté\n');

        // Configuration protocole ASCII
        console.log('[2/3] Configuration protocole ASCII...');
        await bms.setProtocol('ASCII');
        console.log('✅ Protocole configuré\n');

        // Attendre un peu que le BMS soit prêt
        await new Promise(r => setTimeout(r, 500));

        // Lecture de tous les registres
        console.log(`[3/3] Lecture de ${REGISTERS_TO_READ.length} registres...`);
        console.log('='.repeat(70));

        const results = [];
        let successCount = 0;
        let errorCount = 0;

        for (const regId of REGISTERS_TO_READ) {
            const info = REGISTER_INFO[regId];
            try {
                const rawValue = await bms.readIndividualRegister(regId);
                const displayValue = info.scale ? (rawValue * info.scale).toFixed(4) : rawValue;

                results.push({
                    regId,
                    label: info.label,
                    rawValue,
                    displayValue,
                    unit: info.unit,
                    success: true
                });

                console.log(`✅ ${regId.toString().padEnd(4)} ${info.label.padEnd(30)} = ${displayValue.toString().padStart(10)} ${info.unit} (raw: ${rawValue})`);
                successCount++;

                // Petit délai entre les lectures
                await new Promise(r => setTimeout(r, 100));
            } catch (error) {
                results.push({
                    regId,
                    label: info.label,
                    success: false,
                    error: error.message
                });

                console.log(`❌ ${regId.toString().padEnd(4)} ${info.label.padEnd(30)} = ERREUR: ${error.message}`);
                errorCount++;
            }
        }

        // Résumé
        console.log('='.repeat(70));
        console.log('\n📊 RÉSUMÉ:');
        console.log(`   Registres lus avec succès : ${successCount}/${REGISTERS_TO_READ.length}`);
        console.log(`   Erreurs                   : ${errorCount}/${REGISTERS_TO_READ.length}`);

        // Afficher les valeurs clés
        console.log('\n🔑 VALEURS CLÉS:');
        const keyRegisters = [306, 307, 300, 301];
        keyRegisters.forEach(regId => {
            const result = results.find(r => r.regId === regId);
            if (result && result.success) {
                console.log(`   ${result.label.padEnd(30)} : ${result.displayValue} ${result.unit}`);
            }
        });

        console.log('='.repeat(70));

        if (successCount === REGISTERS_TO_READ.length) {
            console.log('✅ SUCCÈS: Tous les registres ont été lus correctement !');
        } else {
            console.log(`⚠️  PARTIEL: ${successCount} registres lus, ${errorCount} erreurs`);
        }

        await bms.disconnect();
        process.exit(0);

    } catch (error) {
        console.error('\n❌ ERREUR FATALE:', error.message);
        console.error(error.stack);
        process.exit(1);
    }
}

// Lancement du test
testReadAllSettings();
