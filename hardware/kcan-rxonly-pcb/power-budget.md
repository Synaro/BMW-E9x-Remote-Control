# Phase 3G — budget d'alimentation `5V_TJA`

## Décision

Le régulateur retenu est l'Infineon **`TLS715B0EJV50XUMA1`**, PG-DSO-8-52 EP,
150 mA, qualifié automobile et annoncé `Active and preferred` avec une
disponibilité planifiée au moins jusqu'en 2038.

Sources primaires :

- [Infineon, page produit](https://www.infineon.com/part/TLS715B0EJ-V50) ;
- [Infineon, datasheet Rev. 1.01](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tls715b0ej-v50-datasheet-en.pdf) ;
- [NXP, TJA1055 Rev. 5](https://www.nxp.com/docs/en/data-sheet/TJA1055.pdf).

## Comparaison des candidats

| Candidat | Limites garanties pertinentes | Résultat |
|---|---|---|
| Infineon `TLS715B0EJV50XUMA1` | 4–40 V, 150 mA, 4,90–5,10 V sur 6–28 V et 0,05–150 mA, -40..150 °C, EP thermique | **RETENU** : garantie totale explicite, composant actif, empreinte et thermique documentés |
| TI `TPS7E8150QDGNRQ1` | 3–40 V, 150 mA, ±1,2 % ligne/charge/température, 2,2–100 µF, AEC-Q100 | PASS électrique et alternative de revue ; génération 2026 plus récente et preuve thermique optimale fondée sur 2s2p |
| Microchip `MCP1799T-5002E/DB` | 6,2–45 V, 80 mA, 5 V ±2 %, Grade 0 | PASS de tension, mais marge de courant inférieure et SOT-223 plus encombrant |
| ST `L5150CJTR` | 5,6–40 V, 150 mA, 5 V ±2 %, AEC-Q100 | PASS de tension, mais fonctions reset/early-warning inutiles et complexité supérieure |

Un buck n'apporte pas d'avantage à environ 12 mA de charge réelle. Il ajoute
inductance, boucle commutée, EMI et modes de panne sans réduire un échauffement
qui reste inférieur à 0,27 W dans l'enveloppe de calcul.

## Arbre retenu

`F_BAT` reste la protection de courant commune en amont. `D_BAT` protège
contre l'inversion. La branche régulateur part de `BAT_PROTECTED` :

```text
BAT_PROTECTED -> U3.IN
BAT_PROTECTED -> U3.EN
BAT_PROTECTED -> C_REG_IN 100 nF -> GND
U3.OUT -> 5V_TJA
5V_TJA -> C_REG_OUT 2,2 uF -> GND
5V_TJA -> C_VCC_100N + C_VCC_22U -> GND, près de U1
```

La diode `OUT -> IN` envisagée pendant l'étude n'est **pas montée**. La
datasheet Infineon exige une protection externe contre l'inversion à l'entrée,
assurée ici par `D_BAT`, mais ne spécifie ni limite de courant inverse Q→I ni
diode de décharge Q→I pour cette application. Le schéma d'application montre
une diode de protection contre les surtensions sur l'entrée; il ne justifie
pas une diode reliant la sortie à l'entrée. Les 24,3 uF nominaux de sortie ne
sont reliés à aucune autre source et se déchargent dans la charge locale
TJA1055/réseaux lors du retrait du 12 V. Ajouter un composant non spécifié
créerait un chemin inverse inutile vers `BAT_PROTECTED`.

Les broches U3 4–7 restent ouvertes. L'exposed pad emploie l'empreinte locale
Phase 3G.1 `PG-DSO-8-52`, conforme au dessin Infineon courant : corps
3,90 x 4,90 mm, pas 1,27 mm et pad NSMD 2,65 x 3,00 mm. Il ne contient plus
aucun trou. Quatre vias GND périphériques tentés, pads 0,60 mm / forets
0,30 mm, sont placés à `x=±0,80 mm`, `y=±2,00 mm` par rapport au centre U3.
Ils restent sous le corps, mais hors du cuivre et des ouvertures de pâte du pad
exposé. Le remplissage cuivre supérieur mesuré par l'API KiCad après routage
vaut **459,429 mm²**, auquel s'ajoute le plan GND de la face inférieure.

La pâte du pad exposé est divisée en quatre fenêtres de 1,125 x 1,300 mm,
soit 5,850 mm² imprimés sur 7,950 mm² de pad (73,58 %). Cette segmentation
reprend le dessin d'empreinte Infineon et évite à la fois une masse unique de
pâte et toute aspiration de brasure dans un trou.

KiCad 10 ne fournit pas encore de modèle 3D portant le nom de boîtier `-52`.
Le STEP utilise uniquement le modèle d'enveloppe mécanique SO-8 exact
3,90 x 4,90 mm / pas 1,27 mm. L'exposed pad, le masque, la pâte et les vias
restent définis par l'empreinte locale et non par ce modèle visuel.

## Worst case de tension

Enveloppe de banc : `J_BAT = 10,0..18,0 V`, alimentation limitée à 100 mA.

À 20 mA de calcul :

```text
chute F_BAT max initiale = 0,020 A x 12,7 ohm = 0,254 V
chute D_BAT conservatrice = 0,750 V
VIN_U3_min = 10,000 - 0,254 - 0,750 = 8,996 V
VIN_U3_max <= 18,000 V
```

Ces deux valeurs sont dans la plage 6–28 V associée à la garantie complète
de sortie. La limite Infineon de 4,90–5,10 V inclut ligne, charge et
température; les 25 mV séparés de line/load regulation ne sont donc pas
additionnés une seconde fois.

Il n'existe aucun composant série après U3 :

```text
VCC_TJA_min garanti = 4,90 V >= 4,75 V  -> marge 150 mV
VCC_TJA_max garanti = 5,10 V <= 5,25 V  -> marge 150 mV
```

**Résultat : PASS.**

## Traitement de `F_5V`

Le `MINISMDC010F-2` aval de Phase 3F n'est pas repris. Sa résistance initiale
maximale de 12,7 ohms pourrait retirer 254 mV à 20 mA et annuler la marge de
150 mV. La protection contre le court-circuit est assurée par `F_BAT` en amont
et par la limitation de courant U3 (minimum 151 mA). Ce choix ne modifie pas
la BOM Phase 3F historique.

## Courant du domaine 5 V

| Charge | Maximum/enveloppe |
|---|---:|
| TJA1055, normal récessif | 10,00 mA |
| réseau STB 1 k/10 k + courant entrée | 0,483 mA |
| réseau EN 1 k/10 k + courant entrée | 0,483 mA |
| entrée TXD | 0,021 mA |
| contribution RTH/RTL au rail, enveloppe | 0,900 mA |
| fuites et marge locale | 0,100 mA |
| **total borné** | **11,987 mA** |
| **enveloppe de conception** | **20,000 mA** |

Le courant de démarrage des 24,3 µF nominaux reste borné par U3. À la limite
de courant minimale de 151 mA, la charge idéale 0→5 V vaut moins de 0,81 ms;
la valeur réelle peut être plus longue à cause de la régulation et doit être
mesurée, sans effet sur le budget continu.

## Thermique

Cas conservateur : `VIN=18 V`, `VOUT=4,90 V`, `IOUT=20 mA`, courant propre U3
`80 µA max`, EN `22 µA max` :

```text
P = (18 - 4,90) x 0,020 + 18 x (0,000080 + 0,000022)
P = 0,263836 W
```

La résistance thermique maximale compatible avec cette enveloppe est :

```text
RthJA_requise <= (150 - 85) / 0,263836 = 246,37 K/W
```

La donnée constructeur la plus conservatrice publiée est `RthJA=153 K/W`
pour une carte 1s0p « footprint only ». En ne créditant **aucun** bénéfice aux
quatre vias périphériques, aux 459,429 mm² de F.Cu ni au plan B.Cu :

```text
deltaT = 0,263836 x 153 = 40,37 °C
Tj = 85 + 40,37 = 125,37 °C
marge à 150 °C = 24,63 °C
```

Le résultat reste donc **PASS** sans dépendre des neuf anciens vias 0,20 mm.
Infineon publie aussi 71 K/W avec 300 mm² et 59 K/W avec 600 mm² sur sa carte
1s0p de référence à cuivre 70 µm. La carte Phase 3G.1 utilise 1 oz (environ
35 µm) : ces chiffres sont conservés comme contexte, mais ne sont pas utilisés
pour prétendre une température réelle. Les 459,429 mm² et les quatre vias
constituent une marge non créditée ; la température devra tout de même être
mesurée au bring-up.

## Séquencement et backfeed

| Cas | État sûr démontré par l'architecture |
|---|---|
| USB présent, 12 V absent | ESP32/U2 alimentés; U2 `/OE` tiré HIGH; U1/U3 éteints; aucun lien entre U2.Y et U1.TXD |
| 12 V présent, USB absent | U1/U3 alimentés; `JP_RX_MODE` absent force STB/EN LOW; TXD tiré HIGH par son propre rail; RXD/ERR sont open-drain sans source vers le DevKit |
| USB d'abord ou 12 V d'abord | mêmes états indépendants; seule GND est commune |
| retrait USB | U2 s'éteint avec Ioff; U1.TXD reste HIGH; barrière physique inchangée |
| retrait 12 V | aucune source ne maintient `5V_TJA`; les condensateurs se déchargent dans la charge locale; U1 force ses commandes internes au mode basse consommation; `D_BAT` bloque tout retour au connecteur |
| reset ESP32 | `/OE` matériel reste HIGH; GPIO5 ne dépasse pas U2.A |
| brownout 5 V | U1 force STB/EN LOW selon NXP; aucune commande ne peut atteindre TXD |
| BAT présent, VCC absent | état prévu par NXP; le nœud non alimenté ne perturbe pas le bus |
| VCC présent, ESP32 hors tension | RXD/ERR `/3` sont open-drain et ne peuvent pas sourcer le MCU; U2 est hors tension et physiquement séparé |

NXP indique explicitement que l'interface microcontrôleur ne possède pas de
chemin de courant inverse lorsque le TJA1055 est hors tension. Pour la variante
`/3`, RXD et ERR sont des sorties open-drain : lorsque le domaine ESP32 est
éteint, aucun pull-up alimenté ne se trouve côté transceiver; lorsqu'il est
allumé et que le TJA est éteint, ses pull-ups 3,3 V ne réalimentent pas VCC du
TJA. STB et EN n'ont aucune connexion au DevKit, et TXD reste un îlot 5 V
local séparé physiquement de U2.Y.

Le header DevKit n'affecte aucun net à sa broche 5 V. Un test de résistance
entre `TP_5V` et la broche 5 V/VBUS du DevKit doit rester ouvert avant toute
mise sous tension.
