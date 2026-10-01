# Câblage Phase 3F — récepteur K-CAN RX-only

> **Banc uniquement. Ne pas raccorder à la BMW.** Ce document décrit la
> variante `KCAN_RX_ONLY_P3F`. `R_LINK_TX` doit être physiquement absent. Le
> cavalier `JP_RX_MODE` doit être retiré à chaque mise sous tension.

La nomenclature normative est [BOM.csv](BOM.csv), la liste d'achat vérifiée est
[procurement.csv](procurement.csv) et le netlist lisible par les tests est
[netlist.csv](netlist.csv). Les numéros U1 ci-dessous correspondent
au boîtier SO-14 NXP SOT108-1. Vérifier l'encoche et la broche 1 avant soudure.

## Connecteurs

| Connecteur | Broche | Signal | Règle |
|---|---:|---|---|
| `J_BAT` JST-XH 2 voies | 1 | `BAT_INPUT` +12 V banc | alimentation de laboratoire limitée en courant uniquement |
| `J_BAT` | 2 | `GND` | brancher en premier, débrancher en dernier |
| `J_KCAN` JST-XH 3 voies | 1 | `KCAN_H` | LS/FT ISO 11898-3 uniquement |
| `J_KCAN` | 2 | `KCAN_L` | LS/FT ISO 11898-3 uniquement |
| `J_KCAN` | 3 | `GND` | référence du banc ; brancher avant H/L |
| `JP_RX_MODE` | 1 | `+5V_TJA` | cavalier amovible |
| `JP_RX_MODE` | 2 | `RX_MODE_FEED` | ouvert au boot ; fermé seulement après contrôles |

Il n'existe aucun connecteur véhicule, OBD ou BMW dans ce gel. Un futur
faisceau devra être réversible, détrompé et étudié à partir du schéma exact du
véhicule ; il ne peut pas être déduit de ce connecteur de banc.

## Interface ESP32-S3-DevKitC-1-N8R8

| Broche DevKit | Connexion | Motif |
|---|---|---|
| USB | alimentation et programmation | unique source du DevKit pendant ce prototype |
| `5V` | vers `F_5V`, puis rail `+5V_TJA` | alimente uniquement la branche transceiver ; ne jamais ajouter une seconde source 5 V |
| `3V3` | U2 VCC, pulls RXD/ERR, pulls U2 | domaine logique |
| `GND` | plan `GND` commun | référence unique du banc |
| `GPIO4` | depuis U1 RXD via `R_RX_SER=1 kΩ` | `TWAI_RX`, configuration `BENCH_ONLY` |
| `GPIO5` | vers U2 A via `R_TX_SER=1 kΩ` | `TWAI_TX` s'arrête sur la chaîne isolée |
| `GPIO7` | depuis U1 ERR via `R_ERR_SER=1 kΩ` | entrée diagnostic, pas une commande |

GPIO4, GPIO5 et GPIO7 ne sont ni les straps GPIO0/3/45/46, ni USB
GPIO19/20, ni SPI0/1 GPIO26–37. Ces affectations restent `BENCH_ONLY` et ne
préjugent pas d'un calculateur automobile final.

## Câblage FROM → composant → TO

| FROM | Composant / valeur | TO | Montage |
|---|---|---|---|
| DevKit `GND` | fil court / plan | U1 pin 13 `GND` | monté |
| DevKit `GND` | fil court / plan | U2 pin 3 `GND` | monté |
| `J_BAT.2` | fil court / plan | `GND` | monté |
| `J_KCAN.3` | fil court / plan | `GND` | monté |
| DevKit `5V` | `F_5V`, PTC 100 mA | `+5V_TJA` | monté |
| `+5V_TJA` | direct | U1 pin 10 `VCC` | monté |
| U1 pin 10 | `C_VCC_100N=100 nF` | `GND` | au plus près de U1 |
| U1 pin 10 | `C_VCC_22U=22 µF` | `GND` | au plus près de U1 |
| DevKit `3V3` | direct | U2 pin 5 `VCC` | monté |
| U2 pin 5 | `C_U2_100N=100 nF` | `GND` | au plus près de U2 |
| `J_BAT.1` | `F_BAT`, PTC 100 mA | anode `D_BAT` | monté |
| anode `D_BAT` | `SS16`, cathode côté protégé | `BAT_PROTECTED` | monté |
| `BAT_PROTECTED` | `R_BAT=1 kΩ`, 0,25 W | `BAT_LOCAL` | monté |
| `BAT_LOCAL` | direct | U1 pin 14 `BAT` | monté |
| U1 pin 14 | direct et piste courte | U1 pin 7 `WAKE` | monté |
| U1 pin 14 | `C_BAT_10N=10 nF/50 V` | `GND` | au plus près de U1 |
| U1 pin 1 `INH` | aucune liaison | nulle part | laissé ouvert |
| `+5V_TJA` | `R_TXD=10 kΩ` | U1 pin 2 `TXD` | monté |
| U1 pin 2 `TXD` | direct | `TP_TXD` | test uniquement |
| DevKit `GPIO5` | `R_TX_SER=1 kΩ` | U2 pin 2 `A` | monté |
| DevKit `GPIO5` | direct | `TP_TWAI_TX` | test uniquement |
| DevKit `3V3` | `R_A=100 kΩ` | U2 pin 2 `A` | monté |
| DevKit `3V3` | `R_OE=10 kΩ` | U2 pin 1 `/OE` | monté ; aucun GPIO sur `/OE` |
| U2 pin 1 `/OE` | direct | `TP_OE` | test uniquement |
| U2 pin 4 `Y` | direct | `TP_GATE_Y` et pad A `R_LINK_TX` | monté jusqu'au pad A |
| pad A `R_LINK_TX` | **aucune pièce, écart 2,54 mm** | pad B / U1 pin 2 | **DNP** |
| U1 pin 3 `RXD` | `R_RX_SER=1 kΩ` | DevKit `GPIO4` | monté |
| DevKit `3V3` | `R_RX_PULL=3,3 kΩ` | U1 pin 3 `RXD` | monté |
| U1 pin 4 `ERR` | `R_ERR_SER=1 kΩ` | DevKit `GPIO7` | monté |
| DevKit `3V3` | `R_ERR_PULL=3,3 kΩ` | U1 pin 4 `ERR` | monté |
| `+5V_TJA` | `JP_RX_MODE` amovible | `RX_MODE_FEED` | cavalier retiré au boot |
| `RX_MODE_FEED` | `R_STB_SER=1 kΩ` | U1 pin 5 `STB` | monté |
| U1 pin 5 `STB` | `R_STB_PD=10 kΩ` | `GND` | monté |
| `RX_MODE_FEED` | `R_EN_SER=1 kΩ` | U1 pin 6 `EN` | monté |
| U1 pin 6 `EN` | `R_EN_PD=10 kΩ` | `GND` | monté |
| `J_KCAN.1` `KCAN_H` | direct | U1 pin 11 `CANH` | monté |
| `J_KCAN.1` `KCAN_H` | `R_RTH=5,62 kΩ`, 0,1 % | U1 pin 8 `RTH` | monté |
| `J_KCAN.2` `KCAN_L` | direct | U1 pin 12 `CANL` | monté |
| `J_KCAN.2` `KCAN_L` | `R_RTL=5,62 kΩ`, 0,1 % | U1 pin 9 `RTL` | monté |
| DevKit `EN` | direct | `TP_RESET` | test uniquement |

Il ne faut monter **aucune résistance de 120 Ω entre `KCAN_H` et `KCAN_L`**.
Les empreintes `D_CAN` et `L_CAN` sont réservées et DNP : choisir au hasard un
TVS ou une self prévue pour ISO 11898-2 pourrait modifier la capacité et les
niveaux du réseau LS/FT. Les protections effectivement montées dans le
prototype sont les limites internes du TJA1055, le PTC 12 V, la diode
anti-inversion, `R_BAT`, le PTC 5 V et les découplages.

## Implantation et routage du prototype

- placer U1, `C_VCC_100N`, `C_VCC_22U`, `C_BAT_10N`, `R_RTH` et `R_RTL` dans
  une zone compacte ;
- router `KCAN_H` et `KCAN_L` ensemble, de longueur similaire, sans boucle ;
- maintenir la dérivation entre `J_KCAN` et U1 sous 10 cm sur le banc ;
- ne pas utiliser de breadboard sans soudure pour la qualification alimentée ;
- séparer physiquement `TP_GATE_Y` et `TP_TXD` d'au moins 2,54 mm, sans pistes
  parallèles ni cuivre entre les deux pads `R_LINK_TX` ;
- ne monter ni broches ni header sur `R_LINK_TX` : ses deux trous restent nus ;
- sérigraphier ou étiqueter `RX ONLY — TX LINK DNP — BENCH ONLY` ;
- placer les points `TP_CANH`, `TP_CANL` et `TP_GND` côte à côte pour les
  sondes ; utiliser des ressorts de masse courts, pas une longue pince de masse ;
- torsader le câble H/L ; transporter GND séparément dans le même faisceau.
- monter U1 sur `PA0003C` et U2 sur `PA0086C` ; Adafruit `1210` n'accepte pas
  le boîtier SOT-23-5 de U2 et ne doit pas être utilisé ;
- effectuer un montage à blanc des points de test `5001` dans les trous de
  0,94 mm du `PR2H1-D` ; si la retenue isolante exige le trou constructeur de
  1,02 mm, agrandir uniquement les emplacements concernés avec un foret adapté,
  sans arracher les pastilles PTH ;
- couper les fils `ASXHSXH22K152` en deux, étiqueter chaque pigtail puis insérer
  les contacts présertis dans `XHP-2` et `XHP-3` ; ne pas sertir les contacts
  nus avec une pince non qualifiée.

## Points de test

| Point | Attendu, cavalier RX absent | Attendu, cavalier RX présent |
|---|---|---|
| `TP_BAT` | environ 11,3–11,8 V pour 12 V en entrée | identique |
| `TP_5V` | 4,75–5,25 V | 4,75–5,25 V |
| `TP_3V3` | rail DevKit 3,3 V | rail DevKit 3,3 V |
| `TP_OE` | HIGH, proche de 3,3 V | HIGH, proche de 3,3 V |
| `TP_GATE_Y` | haute impédance ; sa tension n'est pas un critère TXD | identique |
| `TP_TXD` | HIGH, proche de `+5V_TJA` | HIGH, proche de `+5V_TJA` |
| `TP_TWAI_TX` | activité firmware possible, isolée de TXD | activité firmware possible, isolée de TXD |
| `TP_RESET` | HIGH hors reset, LOW pendant reset | identique |
| `TP_STB` | LOW, <0,8 V | HIGH, >2,2 V, typiquement ~4,5 V |
| `TP_EN` | LOW, <0,8 V | HIGH, >2,2 V, typiquement ~4,5 V |
| `TP_RXD` | réveil/flag possible, pas les données complètes | données CAN reçues |
| `TP_ERR` | flag power-on/réveil possible | HIGH sans défaut, LOW/toggle selon défaut |
| `TP_CANH/L` | aucun dominant produit par ce nœud | aucun dominant produit par ce nœud |

## Checklist avant première mise sous tension

- [ ] U1 est bien la variante **`TJA1055T/3`**, pas `TJA1055T`.
- [ ] L'orientation des pins 1 de U1 et U2 est contrôlée sous loupe.
- [ ] `R_LINK_TX` est vide, sans header, sans étain et photographié.
- [ ] DMM : résistance/continuité infinie entre U2.Y et U1.TXD.
- [ ] DMM : absence de court-circuit 12 V–GND, 5 V–GND et 3,3 V–GND.
- [ ] `R_RTH` et `R_RTL` mesurent chacun 5,62 kΩ et sont appariés à 1 % ou mieux.
- [ ] Aucune résistance 120 Ω ne relie H et L.
- [ ] `JP_RX_MODE` est retiré.
- [ ] `J_KCAN` n'est relié ni à la BMW ni à un nœud inconnu.
- [ ] Le DevKit est alimenté par USB ; aucune autre source n'est reliée à son 5 V.
- [ ] L'alimentation 12 V est réglée à 12,0 V avec une limite initiale de 20 mA.
- [ ] GND est branché avant le +12 V.
- [ ] Après alimentation, `TP_5V`, `TP_3V3`, `TP_BAT`, `TP_STB`, `TP_EN`,
  `TP_OE` et `TP_TXD` sont dans les plages ci-dessus.
- [ ] Le courant BAT anormal, une chauffe, une odeur ou une tension hors plage
  provoque une coupure immédiate et un statut FAIL.

## Séquence d'armement du récepteur

1. démarrer sans bus et sans cavalier RX ;
2. mesurer `/OE=HIGH`, `TXD=HIGH`, `STB=LOW`, `EN=LOW` ;
3. confirmer l'ouverture de `R_LINK_TX` hors tension si elle n'a pas été
   contrôlée pendant cette session ;
4. connecter d'abord GND du **banc LS/FT**, puis CANH/CANL ;
5. alimenter le générateur et contrôler les niveaux au scope ;
6. installer `SH_RX_MODE` : STB et EN passent HIGH, U1 entre en mode normal ;
7. vérifier que RXD suit les données et que ni ce nœud ni l'ESP32 ne produisent
   d'ACK ;
8. retirer le cavalier avant tout changement d'alimentation ou de câblage.

La mesure ACK/no-ACK et la recherche de pulses boot/reset nécessitent un
oscilloscope. Une réception logicielle correcte ne prouve pas le silence
électrique.
