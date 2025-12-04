#!/usr/bin/env node

/**
 * Test simple : lecture seule des registres 306 et 343
 * Pour vérifier que la commande 0x09 fonctionne correctement
 */

const TinyBMS = require('./tinybms');

const PORT = process.argv[2] || '/dev/tty.usbserial-0001';

async function testSimpleRead() {
    console.log('='.repeat(60));
    console.log('  Test simple: Lecture registres 306 et 343');
    console.log('='.repeat(60));
    console.log();

    const bms = new TinyBMS(PORT);

    try {
        // Connexion
        console.log(`[1/4] Connexion au BMS sur ${PORT}...`);
        await bms.connect();
        console.log('✅ Connecté\n');

        // Configuration protocole ASCII
        console.log('[2/4] Configuration protocole ASCII...');
        await bms.setProtocol('ASCII');
        console.log('✅ Protocole configuré\n');

        // Attendre un peu que le BMS soit prêt
        await new Promise(r => setTimeout(r, 500));

        // Test lecture registre 343 (Protocol)
        console.log('[3/4] Lecture registre 343 (Protocol)...');
        const protocol = await bms.readIndividualRegister(343);
        console.log(`✅ Registre 343 = ${protocol} (${protocol === 1 ? 'ASCII' : 'Unknown'})\n`);

        // Test lecture registre 306 (Battery Capacity)
        console.log('[4/4] Lecture registre 306 (Battery Capacity)...');
        const capacity = await bms.readIndividualRegister(306);
        const capacityAh = (capacity * 0.01).toFixed(2);
        console.log(`✅ Registre 306 = ${capacity} (valeur brute)`);
        console.log(`   → Capacité affichée: ${capacityAh} Ah\n`);

        // Vérification
        console.log('='.repeat(60));
        if (capacity === 320) {
            console.log('✅ SUCCÈS: Le registre 306 lit bien 320 (3.20 Ah)');
        } else if (capacity === 1792) {
            console.log('❌ ERREUR: Le registre 306 lit 1792 au lieu de 320');
            console.log('   Problème de byte order ou de commande');
        } else {
            console.log(`ℹ️  INFO: Le registre 306 = ${capacity} (${capacityAh} Ah)`);
            console.log('   Valeur différente, vérifiez avec le script Python');
        }
        console.log('='.repeat(60));

        await bms.disconnect();
        process.exit(0);

    } catch (error) {
        console.error('\n❌ ERREUR:', error.message);
        console.error(error.stack);
        process.exit(1);
    }
}

// Lancement du test
testSimpleRead();
