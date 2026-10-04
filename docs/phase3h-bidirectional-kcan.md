# Phase 3H — matériel K-CAN bidirectionnel

## Statut et périmètre

La Phase 3H crée une **nouvelle révision matérielle** destinée à remplacer la
carte Phase 3G/3G.1 RX-only pour toute fabrication future. Les fichiers et tags
des phases précédentes restent historiques et ne doivent pas être utilisés
pour fabriquer la Phase 3H.

Cette phase fournit uniquement l'infrastructure électrique CAN classique
bidirectionnelle. Elle ne contient aucun identifiant BMW, aucune commande CAS,
DDE ou KL50, aucun contournement d'antidémarrage et aucune fonction de
démarrage à distance.

## Chaîne électrique retenue

```text
ESP32-S3 GPIO5 / TWAI_TX
  -> R5 1 kΩ
  -> U2.A (domaine VCCA = 3V3)
  -> U2 SN74LXC1T45-Q1, DIR = 3V3 (A vers B)
  -> U2.B (domaine VCCB = 5V_TJA)
  -> KCAN_TXD
  -> U1.TXD TJA1055T/3/2Z
  -> CANH/CANL

CANH/CANL
  -> U1.RXD open-drain
  -> R6 pull-up 3,3 kΩ vers 3V3
  -> R7 série 1 kΩ
  -> ESP32-S3 GPIO4 / TWAI_RX
```

Il n'existe ni coupure cuivre, ni pont de soudure, ni jumper d'activation TX,
ni résistance DNP dans le trajet. `TP_TX_MCU` et `TP_TXD` sont de simples
points de mesure et ne coupent pas le signal.

## Pourquoi le SN74LVC1G125-Q1 Phase 3G n'est pas conservé

Le `TJA1055T/3` reconnaît un niveau haut à partir de 2,2 V et un niveau bas
jusqu'à 0,8 V : un GPIO ESP32-S3 3,3 V est donc compatible lorsque les deux
circuits sont alimentés. Le problème est la séquence d'alimentation : le
TJA1055 limite TXD à `VCC + 0,3 V`, tandis que le DevKit est alimenté par USB et
peut rester actif lorsque `5V_TJA` est absent.

Le `SN74LVC1G125-Q1` n'apporte pas une solution complète :

- à 3,3 V, sa sortie ne doit pas piloter un TJA1055 non alimenté ;
- à 5 V, son entrée exige au minimum `0,7 × VCC`, donc un haut ESP32 de
  3,3 V n'est pas garanti ;
- tirer sa sortie 3,3 V vers 5 V est explicitement déconseillé par TI.

U2 reste donc utile en tant qu'interface de domaines, mais devient le
`SN74LXC1T45QDCKRQ1`, AEC-Q100, double alimentation. `VCCA=3V3`, `VCCB=5V_TJA`
et `DIR=3V3` fixent définitivement le sens A vers B. Il n'a pas d'entrée
d'activation externe. Si un des deux rails est absent, sa fonction
VCC-isolation/Ioff place les ports en haute impédance ; aucun ordre de mise
sous tension n'est imposé.

Le `R2` 10 kΩ vers `5V_TJA` définit TXD récessif dans son propre domaine. Il
n'est pas un verrou TX : il ne coupe aucun signal et U2 peut normalement tirer
TXD à l'état dominant.

## Marges logiques et timing

| Liaison | Garantie utilisée | Marge |
|---|---:|---:|
| ESP32 3,3 V vers U2.A | U2 `VT+` max 1,92 V à VCCA=3,0 V | au moins 1,08 V avec un haut à 3,0 V |
| U2.B vers TJA1055 TXD haut | U2 `VOH >= VCCB-0,1 V` à faible charge ; TJA `VIH >= 2,2 V` | environ 2,45 V au pire avec VCCB=4,75 V |
| U2.B vers TJA1055 TXD bas | U2 `VOL <= 0,1 V` à 100 µA ; TJA `VIL <= 0,8 V` | au moins 0,7 V |
| TJA1055 RXD vers ESP32 | RXD open-drain, pull-up 3V3 ; `IOL >= 1,3 mA` à 0,4 V | R6 demande environ 0,88 mA |

Le traducteur est spécifié jusqu'à 420 Mbit/s pour 3,3 V vers 5 V. Son délai
est négligeable devant un bit K-CAN à 100 kbit/s. Le TJA1055 reste l'élément qui
détermine le comportement ISO 11898-3.

## Modes et état au boot

Les commandes de mode du TJA1055 sont reliées à l'ESP32, sans jumper :

- GPIO6 -> R10 -> `KCAN_STB`, avec R11 10 kΩ vers GND ;
- GPIO15 -> R12 -> `KCAN_EN`, avec R13 10 kΩ vers GND ;
- GPIO7 lit `ERR` via R8/R9 ;
- `WAKE` est relié à `BAT_LOCAL`, recommandation NXP pour une entrée WAKE non
  utilisée ; `INH` reste non connecté.

Pendant reset ou ESP32 hors tension, les pull-downs maintiennent STB=0 et
EN=0 : le TJA1055 reste dans un mode basse consommation, sans rendre le trajet
TX physiquement inutilisable. Après initialisation, le logiciel peut choisir
le mode TWAI listen-only pour une acquisition passive, ou le mode normal pour
une future fonction explicitement autorisée. Aucun chemin d'émission
applicatif n'est ajouté par cette phase.

Le profil de compilation de référence se trouve dans
`config/bench-twai.phase3h.hpp.example`. Il reste désactivé par défaut. Copié
localement sous `config/bench-twai.local.hpp`, il configure GPIO6/GPIO15 en
état bas avant de les placer à l'état haut requis par le mode normal du
TJA1055. `BMW_REMOTE_BENCH_ONLY_NORMAL_MODE=0` garde le contrôleur TWAI en
écoute seule ; la valeur `1` sélectionne le mode normal du contrôleur, mais ne
crée toujours aucune API de transmission.

## GPIO DevKitC-1

| Fonction | GPIO | Header |
|---|---:|---:|
| TWAI_RX | GPIO4 | J1.4 |
| TWAI_TX | GPIO5 | J1.5 |
| TJA1055 STB | GPIO6 | J1.6 |
| TJA1055 ERR | GPIO7 | J1.7 |
| TJA1055 EN | GPIO15 | J1.8 |

Ces GPIO sont routables par la matrice GPIO/TWAI, sont exposés sur la
DevKitC-1 et ne font pas partie des broches de strap ESP32-S3
(`GPIO0/3/45/46`). GPIO19/20 USB et GPIO26–37 flash/PSRAM restent évités.

## Alimentation et transceiver

Le power tree Phase 3G.1 est conservé : J1 10–18 V, F1, D1,
`BAT_PROTECTED`, U3 `TLS715B0EJV50XUMA1`, puis `5V_TJA`. Le DevKit reste
alimenté par USB ; son pin 5 V n'est relié à aucun rail de la carte. Seuls les
masses sont communes.

U1 conserve : BAT/WAKE sur `BAT_LOCAL`, VCC sur `5V_TJA`, C3 100 nF et C4
22 µF, C5 10 nF sur BAT, RTH/RTL avec leurs résistances 5,62 kΩ, RXD/ERR
open-drain tirés vers 3V3, et CANH/CANL vers J2. L'implantation U3 conserve le
pad exposé 2,65 × 3,00 mm sans via-in-pad et quatre vias périphériques tentés
0,60/0,30 mm.

## Sources constructeur

- NXP, TJA1055 Rev. 5 : <https://www.nxp.com/docs/en/data-sheet/TJA1055.pdf>
- TI, SN74LVC1G125-Q1 : <https://www.ti.com/lit/ds/symlink/sn74lvc1g125-q1.pdf>
- TI, SN74LXC1T45-Q1 : <https://www.ti.com/lit/ds/symlink/sn74lxc1t45-q1.pdf>
- Espressif, DevKitC-1 v1.0 : <https://documentation.espressif.com/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.0.html>

## Limites

La présence matérielle du TX ne valide aucune trame BMW. Avant toute
connexion active au véhicule, il reste nécessaire de qualifier le PCB hors
tension, puis sur banc, de vérifier le comportement boot/reset/brownout et de
définir une politique logicielle d'émission bornée. Aucun artefact de cette
phase ne constitue une autorisation d'émettre sur la BMW.
