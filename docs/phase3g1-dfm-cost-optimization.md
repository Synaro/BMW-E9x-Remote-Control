# Phase 3G.1 — optimisation DFM et coût du PCB

## Statut et périmètre

Cette révision traite exclusivement le DFM thermique de `U3`. Le premier
chargement réel du ZIP Phase 3G dans le configurateur JLCPCB a accepté les
Gerbers, mais a associé les neuf vias 0,50/0,20 mm situés dans le pad exposé à
des options spéciales coûteuses. Aucun composant, net, valeur, connecteur,
GPIO, rail, mécanisme RX-only ou fichier firmware n'est modifié.

Statut : **`READY FOR JLCPCB RECHECK`**, pas `RELEASED FOR ORDER`.

## Cause observée du surcoût

Le configurateur a présenté environ :

| Option liée au premier DFM | Montant observé |
|---|---:|
| remplissage époxy / bouchage | 46,15 € |
| foret minimum 0,20 mm | 15,73 € |
| test Kelvin 4 fils | 15,25 € |
| métallisation spéciale des vias | 3,04 € |
| **total options** | **80,17 €** |

Le total PCB observé était environ 89 € pour cinq cartes, avant assemblage et
transport. Ces montants décrivent un écran du configurateur, pas un devis
contractuel. La présence du groupe de vias dans la zone imprimée du pad U3 est
la géométrie qui déclenche le traitement via-in-pad ; le lien exact entre
chaque ligne tarifaire, notamment Kelvin, et la règle interne du fabricant
n'est pas revendiqué comme une spécification publique.

## Sources primaires

- [Infineon TLS715B0EJ-V50, datasheet](https://www.infineon.com/assets/row/public/documents/10/49/infineon-tls715b0ej-v50-datasheet-en.pdf)
- [Infineon PG-DSO-8-52](https://www.infineon.com/package/PG-DSO-8-52)
- [Infineon, board assembly recommendations for dual-row gullwing packages](https://www.infineon.com/assets/row/public/documents/packages/infineon-board-assembly-recommendations-gullwing-package-v05-00-en.pdf)
- [JLCPCB, PCB capabilities](https://jlcpcb.com/capabilities/pcb-capabilities/)
- [PCBWay, PCB capabilities](https://www.pcbway.com/capabilities.html)

Infineon demande un pad thermique au moins congruent au pad exposé et indique
que nombre, diamètre et placement des vias dépendent de la puissance et du
PCB. Sa recommandation générique donne 0,2 à 0,5 mm comme plage typique, sans
imposer neuf forets 0,20 mm. Lorsque le transfert direct sous le pad n'est pas
nécessaire, des vias proches du boîtier et couverts de masque sont permis. Le
dessin propre au `PG-DSO-8-52` fixe le pad à 2,65 x 3,00 mm et montre quatre
fenêtres de stencil.

Les capacités publiées JLCPCB montrent que 0,60/0,30 mm se situe dans le
process de via mécanique ordinaire et que via-in-pad rempli/cappé est un
process distinct. PCBWay accepte également 0,30 mm comme géométrie de
prototype conventionnelle. La conformité et le prix réels restent à confirmer
par rechargement des fichiers.

## Options comparées

| Option | Thermique | DFM / pâte | Décision |
|---|---|---|---|
| A. pad exposé et F.Cu sans via | calcul conservateur encore PASS | coût minimal, aucun pont vers B.Cu | viable mais ne profite pas du plan inférieur |
| B. vias 0,30 mm autour du pad | ajoute un chemin vers B.Cu sans trou dans la pâte | process standard, tenting possible | **principe retenu** |
| C. nombre réduit dans le pad | transfert direct | conserve risque de mèche à brasure et process via-in-pad | rejeté |
| D. quatre vias périphériques 0,60/0,30 | symétrique, sous le corps, bénéfice non crédité | hors cuivre/pâte EP, tentés deux faces | **géométrie retenue** |
| E. vias hors corps dans la grande zone | facile à fabriquer | chemin thermique plus long | inutile ici |

## Géométrie retenue

- boîtier exact : `PG-DSO-8-52`, corps 3,90 x 4,90 mm, pas 1,27 mm ;
- pad exposé NSMD : 2,65 x 3,00 mm, GND, F.Cu/F.Mask, aucun trou ;
- vias : quatre, GND, pad 0,60 mm, foret 0,30 mm, tentés F.Cu et B.Cu ;
- offsets depuis le centre U3 : `(-0,80;-2,00)`, `(0,80;-2,00)`,
  `(-0,80;2,00)`, `(0,80;2,00)` mm ;
- distance cuivre EP–via la plus faible : 0,20 mm ;
- les vias restent sous le corps 3,90 x 4,90 mm mais hors pad et hors pâte ;
- F.Cu thermique rempli mesuré par l'API KiCad : **459,429 mm²** ;
- plan GND B.Cu : conservé, sans modification de topologie.

Le stencil comporte quatre rectangles 1,125 x 1,300 mm centrés à
`x=±0,665 mm`, `y=±0,750 mm`. L'aire imprimée vaut 5,850 mm² sur 7,950 mm²,
soit **73,58 %**. Aucun orifice n'est présent sous une fenêtre de pâte.

## Enveloppe thermique

Cas de calcul conservé :

```text
VIN = 18,00 V
VOUT = 4,90 V
IOUT = 20,00 mA
Iq max = 80 µA
IEN max = 22 µA
Pmax = (18 - 4,90) x 0,020 + 18 x (0,000080 + 0,000022)
Pmax = 0,263836 W
Ta max = 85 °C
Tj limite = 150 °C
```

La résistance thermique limite du design vaut :

```text
RthJA_requise <= (150 - 85) / 0,263836 = 246,37 K/W
```

Infineon publie 153 K/W pour une carte 1s0p sans dissipateur étendu :

```text
deltaT = 0,263836 x 153 = 40,37 °C
Tj = 85 + 40,37 = 125,37 °C
marge = 150 - 125,37 = 24,63 °C
```

Ce calcul ne crédite ni les 459,429 mm² de F.Cu, ni le plan B.Cu, ni les quatre
vias : il démontre donc que la suppression des neuf anciens vias ne dépend pas
d'une estimation optimiste de leur efficacité. Les données 71 K/W à 300 mm²
et 59 K/W à 600 mm² sont publiées sur la carte 1s0p Infineon à cuivre 70 µm ;
elles ne sont pas transposées directement au prototype 1 oz (~35 µm).

`RthJA` est une donnée typique de conception, pas une garantie de chaque carte.
Le bring-up doit mesurer la température U3 au pire cas et arrêter l'essai si la
marge réelle est insuffisante.

## Impact DFM et estimation non contractuelle

Le nouveau package demande : foret minimum 0,30 mm, vias ordinaires tentés,
aucun via-in-pad, aucun remplissage époxy, aucun cap cuivre et aucune
métallisation spéciale de via. Il ne demande pas non plus un test Kelvin.

Arithmétiquement, retirer 80,17 € d'options du total observé de 89 € laisserait
environ **8,83 € pour cinq PCB**. C'est uniquement un ordre de grandeur
théorique : paramètres du configurateur, taxes, transport, finition et règles
DFM peuvent changer le résultat. Seul un nouveau chargement du ZIP Phase 3G.1
permettra d'obtenir le prix réel.

## Barrière RX-only et limites

La zone U3 est éloignée de `TP_GATE_Y`, `TX_PHYSICAL_GAP_KEEP_OUT` et
`TP_TXD`. Leurs nets, pistes, rule areas et absence de lien restent inchangés.
La carte demeure interdite de connexion BMW jusqu'au bring-up complet : aucune
absence d'ACK, de dominant ou de pulse n'est encore mesurée. Aucune commande
fabricant n'est passée, aucun TX CAN n'est ajouté et la Phase 4 ne commence pas.
