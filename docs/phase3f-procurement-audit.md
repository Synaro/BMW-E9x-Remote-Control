# Phase 3F — audit final d'approvisionnement K-CAN RX-only

## Statut et limites

Audit réalisé le 1er octobre 2026. Il porte exclusivement sur le prototype de
banc `KCAN_RX_ONLY_P3F`. Il ne valide pas une connexion à la BMW et n'ajoute ni
TX CAN, ni rejeu, ni commande CAS/DDE/KL50, ni contournement EWS, ni Phase 4.

`hardware/kcan-rxonly/BOM.csv` reste la définition d'ingénierie. Le nouveau
`hardware/kcan-rxonly/procurement.csv` est un instantané d'achat : prix et stock
doivent être revérifiés dans le panier avant paiement. Les prix ci-dessous sont
indicatifs, hors TVA et hors port, sauf mention contraire.

## Résultat exécutif

La topologie électrique n'a pas été modifiée. L'audit a cependant trouvé sept
problèmes d'approvisionnement ou de compatibilité mécanique :

1. `SN74LVC1G125-Q1` est le nom de famille TI. Le MPN DBV/SOT-23-5 de la BOM,
   `CLVC1G125QDBVRQ1`, était déjà correct. Pour l'achat Mouser France,
   `CLVC1G125QDBVRQ1G4` est accepté explicitement comme suffixe de commande
   « green » du même composant DBV ; ce n'est pas une substitution silencieuse.
2. `ESP32-S3-DEVKITC-1-N8` est désormais une référence distributeur obsolète.
   La variante officielle courante retenue est
   `ESP32-S3-DEVKITC-1-N8R8` : 8 MiB de flash et 8 MiB de PSRAM, même format
   DevKitC-1 et mêmes GPIO4/GPIO5/GPIO7 exposés.
3. Adafruit `1210` est un lot d'adaptateurs SOIC/TSSOP-14, donc incompatible
   avec le buffer DBV/SOT-23-5. Il est remplacé par Chip Quik `PA0086C`.
4. L'ancien `SBB0014A` ne donnait pas une preuve d'achat suffisante de la
   largeur et du pitch. Il est remplacé par `PA0003C`, documenté pour SOIC-14
   150/200 mil au pas de 1,27 mm.
5. Keystone `5015` est un point de test CMS, pas traversant. Il est remplacé
   par le `5001` traversant à trou de 1,02 mm.
6. Les anciennes références Murata 100 nF étaient obsolètes et `PR2H1` ne
   garantissait pas explicitement la nouvelle carte double face à trous
   métallisés. Elles sont remplacées respectivement par des MLCC actifs Würth
   et par `PR2H1-D`.
7. `TJA1055T/3/C,518`, autrefois cité comme alternative conditionnelle, est
   désormais classé EOL par NXP. Il est retiré des alternatives et passe en
   `DO_NOT_BUY`. Aucun substitut form/fit/function de `TJA1055T/3/2Z` n'est
   préapprouvé.

Les contacts JST nus restent documentés mais passent en `DO_NOT_BUY` pour ce
prototype. Des fils JST XH authentiques `ASXHSXH22K152`, déjà sertis avec des
contacts `SXH-001T-P0.6`, évitent d'acheter ou d'imiter l'outillage de sertissage
JST. Chaque fil socket-à-socket est coupé au milieu pour former deux pigtails.

## Contrôle des composants critiques

### NXP TJA1055T/3/2Z — OK

- MPN exact : `TJA1055T/3/2Z` ; statut constructeur NXP : `Active`.
- Boîtier : SO-14 étroit, corps 3,90 mm, pas 1,27 mm.
- VCC : 4,75 à 5,25 V ; BAT : 5 à 40 V ; température : -40 à +125 °C.
- La variante `/3` est obligatoire pour l'interface microcontrôleur 3,3 V et
  ses sorties RXD/ERR open-drain. Un `TJA1055T` sans `/3`, un `TJA1054A` ou un
  transceiver ISO 11898-2 n'est pas un remplacement autorisé.
- `TJA1055T/3/C,518` est EOL et n'est plus une alternative. Une nouvelle
  référence éventuelle devra faire l'objet d'une revue électrique, mécanique
  et de cycle de vie complète.
- Achat petite quantité : DigiKey `568-TJA1055T/3/2ZCT-ND`, bande coupée,
  2 115 pièces vues, 2,42 € HT pièce.

### TI SN74LVC1G125-Q1 — MPN clarifié

- Famille : `SN74LVC1G125-Q1`.
- MPN de base commandable retenu par le design : `CLVC1G125QDBVRQ1`.
- MPN disponible retenu pour l'achat : `CLVC1G125QDBVRQ1G4`, suffixe de
  commande explicitement revu, pas une autre fonction logique.
- Boîtier : DBV/SOT-23-5, cinq broches, pas 0,95 mm.
- Alimentation 1,65 à 5,5 V, sortie trois états, `Ioff`, AEC-Q100 Grade 1.
- Toute autre empreinte, toute porte open-drain, tout composant sans `Ioff` ou
  tout buffer uniquement 5 V est interdit sans nouvelle revue électrique.
- Mouser France `595-VC1G125QDBVRQ1G4`, bande coupée, 5 915 pièces vues,
  0,722 € HT pièce.

### Littelfuse MINISMDC010F-2 — OK

- MPN exact de distribution : `MINISMDC010F-2`, actif, bande coupée disponible.
- Famille miniSMDC, format 1812 ; `Ihold=0,10 A`, `Itrip=0,30 A`, `Vmax=60 V`.
- Deux sont utilisés. Un PPTC de tension inférieure ou l'absence de protection
  n'est pas autorisé.
- DigiKey `MINISMDC010F-2CT-ND`, plus de 50 000 pièces vues, 0,56 € HT pièce.

### Vishay SS16-E3/61T — OK

- MPN exact : `SS16-E3/61T`, Schottky 1 A, 60 V, SMA/DO-214AC.
- Le suffixe `/61T` correspond au conditionnement SMA sur bande. Une diode
  sous 40 V répétitifs ou une empreinte différente est interdite.
- DigiKey `SS16-E3/61TGICT-ND`, plus de 100 000 pièces vues, environ
  0,43 € HT pièce.

### Espressif ESP32-S3-DevKitC-1-N8 — référence corrigée

- `ESP32-S3-DEVKITC-1-N8` : ne pas acheter ; page distributeur obsolète.
- Référence retenue : `ESP32-S3-DEVKITC-1-N8R8`, variante officielle actuelle
  8 MiB flash + 8 MiB PSRAM.
- Mouser France `356-EP32S3DVKTC1N8R8`, 1 601 cartes vues, 12,90 € HT.
- Cette correction ne change ni la cible ESP32-S3, ni TWAI, ni le câblage
  BENCH_ONLY GPIO4/GPIO5/GPIO7. La PSRAM supplémentaire n'est pas requise par
  le firmware et ne crée aucune fonction active.

## Audit ligne par ligne de la BOM

`OK` signifie commandable ou remplaçable selon les critères inscrits dans la
BOM. `CORRIGÉ` signifie que l'ancienne ligne aurait pu conduire à un achat
erroné. `DNP` signifie qu'aucune pièce ne doit être commandée ou montée.

| Référence | Verdict | MPN exact / mécanique | Substitution autorisée | Substitution interdite ou remarque |
|---|---|---|---|---|
| U1 | OK | `TJA1055T/3/2Z`, SO-14 étroit 3,90 mm, 1,27 mm | aucune préapprouvée | sans `/3`, `TJA1055T/3/C,518` EOL, TJA1054A, ISO 11898-2 |
| U2 | OK, clarifié | `CLVC1G125QDBVRQ1`, DBV/SOT-23-5, 0,95 mm | suffixe `G4` explicitement documenté | autre boîtier, sans Ioff, open-drain |
| DEV1 | CORRIGÉ | `ESP32-S3-DEVKITC-1-N8R8`, DevKitC-1 | révision officielle avec GPIO revus | N8 obsolète, ESP32 classique, carte non documentée |
| R_LINK_TX | DNP | empreinte seulement, pas de composant | aucune | toute continuité TX, même temporaire |
| JP_RX_MODE | OK | `TSW-102-07-G-S`, 2 x 1, pas 2,54 mm | header identique mécaniquement | interrupteur piloté par firmware |
| SH_RX_MODE | OK | `SNT-100-BK-G`, shunt 2,54 mm | shunt amovible équivalent | pont permanent |
| R_TXD | OK | `RC0805FR-0710KL`, 10 kΩ, 1 %, 0805, 0,125 W | mêmes caractéristiques | pull-up 3,3 V, pull-down, >47 kΩ |
| R_OE | OK | même 10 kΩ 0805 | mêmes caractéristiques | pull-down ou firmware seul |
| R_A | OK | `RC0805FR-07100KL`, 100 kΩ, 1 %, 0805 | mêmes caractéristiques | entrée flottante ou pull-down |
| R_TX_SER | OK | `RC0805FR-071KL`, 1 kΩ, 1 %, 0805 | mêmes caractéristiques | 0 Ω avec fils longs |
| R_RX_SER | OK | même 1 kΩ 0805 | mêmes caractéristiques | >2,2 kΩ sans validation |
| R_ERR_SER | OK | même 1 kΩ 0805 | mêmes caractéristiques | >2,2 kΩ sans validation |
| R_RX_PULL | OK | `RC0805FR-073K3L`, 3,3 kΩ, 1 %, 0805 | mêmes caractéristiques | pull-up 5 V ou <2,7 kΩ |
| R_ERR_PULL | OK | même 3,3 kΩ 0805 | mêmes caractéristiques | pull-up 5 V ou <2,7 kΩ |
| R_STB_PD | OK | 10 kΩ, 1 %, 0805 | mêmes caractéristiques | pull-up ou firmware seul |
| R_EN_PD | OK | 10 kΩ, 1 %, 0805 | mêmes caractéristiques | pull-up ou firmware seul |
| R_STB_SER | OK | 1 kΩ, 1 %, 0805 | mêmes caractéristiques | liaison directe GPIO |
| R_EN_SER | OK | 1 kΩ, 1 %, 0805 | mêmes caractéristiques | liaison directe GPIO |
| R_RTH | OK | `RT0805BRD075K62L`, 5,62 kΩ, 0,1 %, 25 ppm/°C | même lot et mêmes caractéristiques | 120 Ω H-L, 560 Ω pour véhicule, valeur dépareillée |
| R_RTL | OK | même 5,62 kΩ, même lot | même lot et mêmes caractéristiques | identique à R_RTH |
| R_BAT | OK | `RC1206FR-071KL`, 1 kΩ, 1 %, 1206, 0,25 W | mêmes caractéristiques | hors plage 1–2 kΩ sans analyse |
| F_BAT | OK | `MINISMDC010F-2`, 1812, 60 V, 0,10 A hold | PPTC actif identique après empreinte | tension <40 V ou absence de protection |
| D_BAT | OK | `SS16-E3/61T`, SMA, 60 V, 1 A | AEC-Q101 équivalent avec empreinte revue | tension inverse <40 V |
| F_5V | OK | même `MINISMDC010F-2` | identique à F_BAT | absence de protection de branche |
| C_VCC_100N | CORRIGÉ | `885012207098`, 100 nF, X7R, 50 V, 0805 | actif, X7R, >=16 V, 0805 | ancien Murata obsolète, Y5V |
| C_VCC_22U | CORRIGÉ sourcing | `LMJ316BB7226MLHT`, 22 µF, X7R, 10 V, 1206 | X5R/X7R après revue de biais DC | <10 V ou capacité utile non vérifiée |
| C_BAT_10N | CORRIGÉ sourcing | `885012207092`, 10 nF, X7R, 50 V, 0805 | actif et mêmes caractéristiques | <50 V ou changement de valeur |
| C_U2_100N | CORRIGÉ | même `885012207098` | identique à C_VCC_100N | condensateur omis |
| J_BAT | OK | `B2B-XH-A(LF)(SN)`, JST XH 2 voies, 2,50 mm | famille complète équivalente revue | Dupont non détrompé pour 12 V |
| J_KCAN | OK | `B3B-XH-A(LF)(SN)`, JST XH 3 voies, 2,50 mm | famille complète équivalente revue | prise OBD ou dérivation BMW non qualifiée |
| H_BAT | OK | `XHP-2`, boîtier XH 2 voies | seulement avec contacts XH compatibles | fils nus forcés dans le header |
| H_KCAN | OK | `XHP-3`, boîtier XH 3 voies | seulement avec contacts XH compatibles | prise véhicule non qualifiée |
| CRIMP | DNP achat | `SXH-001T-P0.6`, AWG 26–22 | seulement avec l'applicateur qualifié | pince générique non validée |
| LEAD_XH | AJOUT procurement | `ASXHSXH22K152`, 22 AWG, 152 mm, deux contacts sertis | fil JST XH authentique AWG 26–22 | pigtail marketplace non traçable |
| PCB1 | CORRIGÉ | `PR2H1-D`, PTH double face 80 x 50 mm, grille 2,54 mm | FR4 PTH équivalent après implantation | ancienne référence ambiguë, breadboard sans soudure |
| ADP_U1 | CORRIGÉ | `PA0003C`, SOIC-14 150/200 mil, 1,27 mm vers DIP | `PA0003` si encombrement revu | wide-body seul, mauvais pitch, fils volants |
| ADP_U2 | CORRIGÉ | `PA0086C`, SOT-23-5 0,95 mm vers DIP-6 | PCB avec DBV exact | Adafruit 1210, SOT-23-3, module actif |
| TP_SET | CORRIGÉ | Keystone `5001`, traversant, trou 1,02 mm | couleurs `5000`–`5004` mêmes dimensions | `5015` CMS, fils non étiquetés |
| D_CAN | DNP | `PESD2CAN,215` seulement comme hypothèse | aucune avant qualification LS/FT | TVS ISO 11898-2 choisi arbitrairement |
| L_CAN | DNP | aucun MPN sélectionné | aucune avant mesure CEM | choke non revu |
| PSU12 | À posséder | 0–15 V, >=0,5 A, limitation de courant | instrument existant conforme | batterie ou bloc non régulé |
| DMM | À posséder | continuité, résistance, tension et courant DC | instrument existant conforme | lampe témoin seule |
| SCOPE | PLUS TARD | deux voies, >=20 MHz, deux sondes x10 | 100 MHz recommandé | analyseur logique seul pour preuve analogique |
| GEN_LSFT | PLUS TARD | `IPEH-002021` + `IPEH-002039` | nœud actif de banc séparé, qualifié | interface HS-CAN présentée comme LS/FT |

## Compatibilité mécanique et assemblage

- Le TJA1055 SO-14 a un corps étroit de 3,90 mm et un pas de 1,27 mm. Le
  `PA0003C` accepte SOIC-14 150/200 mil et ressort sur une grille DIP 2,54 mm.
- Le buffer TI DBV est un SOT-23-5 au pas de 0,95 mm. Le `PA0086C` accepte
  précisément SOT-23-5/SC59-5/SC-74A et ressort en DIP-6 2,54 mm.
- `PR2H1-D` fournit 31 x 19 trous PTH de 0,94 mm sur grille 2,54 mm. Les pins
  DIP des deux adaptateurs et le header Samtec correspondent à cette grille.
- JST XH emploie un pas de 2,50 mm, et non 2,54 mm. Les headers vont directement
  dans leur propre empreinte PTH ; il ne faut pas les forcer sur plusieurs
  positions d'une grille 2,54 mm. Pour 2 ou 3 voies, l'écart cumulé reste faible,
  mais les trous doivent être percés/positionnés sans contrainte mécanique.
- `B2B-XH-A` ↔ `XHP-2` et `B3B-XH-A` ↔ `XHP-3` sont des paires cohérentes. Le
  contact `SXH-001T-P0.6` accepte AWG 26–22 ; `ASXHSXH22K152` l'intègre déjà.
- Les points de test `5001` sont traversants et le fabricant recommande un
  trou de montage de 1,02 mm. Les trous de `PR2H1-D` font 0,94 mm : la
  compatibilité de la retenue isolante doit être vérifiée par un montage à
  blanc. Si nécessaire, seuls les emplacements concernés seront élargis
  proprement à environ 1,05 mm, sans arracher le cuivre. Une substitution ne
  peut être choisie qu'après contrôle de son diamètre de montage.

Cette dernière contrainte est volontairement exposée : `5001` est le bon type
électrique/mécanique, mais son montage « drop-in » dans les trous 0,94 mm de la
carte n'est pas affirmé sans essai physique.

## Liste d'achat en trois étapes

### ACHETER MAINTENANT

Le détail exact, les SKU, les liens et les quantités de rechange sont dans
`procurement.csv`. Les 25 lignes `BUY_NOW` représentent :

- carte `ESP32-S3-DEVKITC-1-N8R8` ;
- deux `TJA1055T/3/2Z` ;
- trois `CLVC1G125QDBVRQ1G4` ;
- composants passifs, protections et condensateurs actifs ;
- header/cavalier RX uniquement ;
- connectique JST XH et fils présertis ;
- `PR2H1-D`, `PA0003C`, `PA0086C` et points de test `5001`.

L'alimentation limitée et le DMM sont `BUY_NOW_IF_NOT_OWNED`. Ils sont
indispensables avant toute mise sous tension, mais ne doivent pas être rachetés
si l'utilisateur possède déjà des instruments conformes.

### ACHETER PLUS TARD

- oscilloscope deux voies et sondes x10, avant tout verdict dynamique PASS ;
- source active ISO 11898-3 à 100 kbit/s, par exemple le couple PEAK documenté ;
- aucun de ces outils ne constitue une autorisation de connexion BMW.

### NE PAS ACHETER

- toute pièce pour `R_LINK_TX` ;
- ancien substitut EOL `TJA1055T/3/C,518` ;
- contacts JST nus sans outillage qualifié ;
- TVS et choke encore DNP ;
- ESP32-S3-DevKitC-1-N8 obsolète ;
- Adafruit `1210`, Proto Advantage `SBB0014A`, Keystone `5015` ;
- anciens MLCC Murata obsolètes ;
- connecteur OBD, faisceau BMW, actionneur ou matériel de rejeu.

## Budget

| Périmètre | Montant indicatif HT, hors port |
|---|---:|
| minimum absolu, une pièce de chaque article réellement utilisé | 39,17 € |
| commande recommandée avec rechanges et marge de soudure | 64,49 € |
| alimentation + DMM seulement s'ils ne sont pas déjà possédés | +105,00 € |
| qualification dynamique future : scope estimatif + PEAK | +611,00 € |

À titre indicatif, 64,49 € HT correspondent à environ 77,39 € TTC à 20 %.
DigiKey annonce 25 € de port sous 75 € HT et le port gratuit à partir de 75 € ;
le sous-panier DigiKey actuel reste sous ce seuil. Mouser annonce généralement
la gratuité au-dessus de 75 €, mais le coût sous le seuil dépend du panier et
de l'adresse : il n'est pas inventé ici. Regrouper les articles chez un seul
distributeur peut être moins cher, mais uniquement si tous les MPN et suffixes
restent strictement ceux de cette revue.

## Sources principales

- [NXP TJA1055](https://www.nxp.com/docs/en/data-sheet/TJA1055.pdf)
- [TI SN74LVC1G125-Q1](https://www.ti.com/lit/ds/symlink/sn74lvc1g125-q1.pdf)
- [Espressif ESP32-S3-DevKitC-1](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/index.html)
- [Littelfuse miniSMDC](https://www.littelfuse.com/assetdocs/littelfuse-ptc-minismdc-datasheet?assetguid=3ed735aa-64ed-43a6-bc20-610590bc99c6)
- [Vishay SS12–SS16](https://www.vishay.com/docs/88746/ss12.pdf)
- [Chip Quik PA0003C](https://www.chipquik.com/datasheets/PA0003C.pdf)
- [Chip Quik PA0086C](https://www.chipquik.com/datasheets/PA0086C.pdf)
- [JST XH](https://www.jst-mfg.com/product/pdf/eng/eXH.pdf)
- [BusBoard PR2H1-D](https://www.busboard.com/documents/datasheets/BPS-DAT-%28PR2H1-D%29-Datasheet.pdf)
- [DigiKey France, livraison](https://www.digikey.fr/fr/help-support/delivery-information/delivery-time-and-cost)

## Verrou de sécurité après achat

Acheter et assembler les pièces ne vaut pas qualification. Le montage reste
interdit de connexion véhicule jusqu'à inspection, tests hors tension, contrôle
des rails, boot/reset/brownout au scope, trafic LS/FT de banc, test no-ACK et
checklist PASS complète. `R_LINK_TX` doit rester physiquement absent.
