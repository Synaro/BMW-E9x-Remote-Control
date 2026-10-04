# Phase 3G — procédure de réception et bring-up du PCB RX-only

## Règle d'arrêt

Cette procédure s'applique uniquement après fabrication et réception des
cartes. Elle commence hors tension et sur banc isolé. Une seule case `FAIL`
interdit toute progression. Elle ne contient aucune étape de raccordement BMW.

Matériel minimal : alimentation de laboratoire isolée et limitée en courant,
multimètre, oscilloscope deux voies minimum, analyseur logique, générateur ou
réseau ISO 11898-3 de banc qualifié, charges factices, bracelet ESD et loupe.

## 1. Incoming inspection

- [ ] numéro/révision `3G-A` et dimensions 120 x 80 mm ;
- [ ] deux couches, 1,6 mm, 1 oz conformément au devis ;
- [ ] aucun court-circuit, pont de masque ou cuivre parasite visible ;
- [ ] U3 correspond au boîtier PG-DSO-8-52 et son pad exposé est soudé ;
- [ ] aucun trou dans le pad exposé U3 ; quatre vias GND périphériques
  0,60/0,30 mm présents et tentés des deux côtés ;
- [ ] polarité et marquage D1 vérifiés ;
- [ ] références U1/U2/U3 exactes, sans substitution ;
- [ ] JST, sockets et JP1 orientés comme le plan d'assemblage ;
- [ ] aucune empreinte ou liaison entre TP5 `GATE_Y` et TP6 `TXD` ;
- [ ] sérigraphie RX-only et avertissements lisibles.

Photographier recto, verso, marquages U1/U2/U3 et zone TX avant retouche.

## 2. Continuité hors tension — DevKit et JP1 absents

| Mesure | PASS |
|---|---|
| J1.2 ↔ J2.3 ↔ H2.1 ↔ TP15 | continuité GND |
| J1.1 ↔ BAT_INPUT | continuité via connecteur |
| TP5 `TX_GATE_OUT` ↔ TP6 `TXD_SAFE` | circuit ouvert, jamais un bip |
| H1.21 DevKit 5 V ↔ TP2 `5V_TJA` | circuit ouvert |
| H1.21 ↔ J1.1/J1.2 | circuit ouvert |
| 5V_TJA ↔ GND | pas de court-circuit, valeur archivée |
| BAT_PROTECTED ↔ GND | pas de court-circuit, valeur archivée |
| KCAN_H ↔ KCAN_L | aucune terminaison 120 Ω, valeur archivée |
| KCAN_H/L ↔ GND | réseau faible conforme, valeur archivée |

Vérifier ensuite chaque pin de `pin-review.csv` au microscope et au multimètre.

## 3. 12 V seul — DevKit absent, JP1 absent, bus absent

1. régler J1 à 10,0 V, limite 30 mA, sortie coupée ;
2. connecter J1, observer courant/tension dès l'activation ;
3. arrêter immédiatement en cas de dépassement, échauffement ou oscillation ;
4. mesurer BAT_INPUT, BAT_FUSED, BAT_PROTECTED, BAT_LOCAL et 5V_TJA ;
5. exiger `4,90 V <= 5V_TJA <= 5,10 V` à l'état établi ;
6. vérifier STB=LOW, EN=LOW, TXD_SAFE=HIGH ;
7. répéter à 12,0 V puis 18,0 V en surveillant U3 ;
8. capturer démarrage et extinction de 5V_TJA à l'oscilloscope.

PASS : aucune surtension hors spécification, aucun courant inattendu, aucune
oscillation soutenue et aucun rail sur H1.21.

## 4. USB seul — 12 V et bus absents, JP1 absent

1. installer le DevKit hors tension puis alimenter par USB ;
2. vérifier 3V3, OE_SAFE=HIGH et U2.Y en haute impédance ;
3. exiger 0 V sur 5V_TJA, BAT_PROTECTED et BAT_LOCAL ;
4. vérifier qu'aucun courant ne revient vers J1 ;
5. provoquer reset et watchdog logiciel de banc sans changer ces états.

## 5. Séquences croisées

Archiver formes d'onde et valeurs pour chaque cas :

- USB présent / 12 V absent ;
- 12 V présent / USB absent ;
- USB puis 12 V ;
- 12 V puis USB ;
- retrait USB ;
- retrait 12 V ;
- reset ESP32 ;
- brownout progressif 5V_TJA.

Sur chaque cas, vérifier : aucune tension anormale sur GPIO4/5/7, aucun backfeed
VBUS/5 V DevKit, OE_SAFE toujours HIGH, TP5 isolé de TP6 et aucune excursion
dominante sur un réseau LS/FT factice.

## 6. Activation RX manuelle sur réseau de banc

Cette étape requiert un réseau ISO 11898-3 de banc déjà qualifié, jamais la
BMW. Le générateur doit produire des trames connues à 100 kbit/s.

1. mettre toutes les alimentations hors tension ;
2. connecter J2 au réseau de banc, vérifier H/L/GND ;
3. remettre sous tension avec JP1 absent ;
4. contrôler CANH/CANL et confirmer absence de dominant/ACK ;
5. poser JP1 manuellement pour STB=HIGH et EN=HIGH ;
6. vérifier la réception sur RXD/GPIO4 ;
7. saturer la réception puis provoquer reset/watchdog/brownout ;
8. vérifier en permanence TP5, TP6, CANH et CANL.

PASS absolu : aucune trame émise, aucun ACK, aucun dominant indésirable, aucun
pulse TX au boot/reset/retrait d'alimentation. La réception et les compteurs de
perte peuvent fonctionner, mais ne priment jamais sur ce critère électrique.

## 7. Rapport de qualification

Pour chaque carte, conserver numéro de série, photos, multimètre utilisé,
oscilloscope, date, température, version firmware de banc, captures et résultat
PASS/FAIL. Un PASS de continuité n'est pas un PASS dynamique.

Même avec tous les tests de banc verts, une connexion BMW demande une nouvelle
autorisation explicite. Cette procédure ne valide aucun ID CAN, aucune commande,
aucun replay et aucun remote-start.
