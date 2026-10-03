# Phase 3G — carte K-CAN RX-only

Cette arborescence décrit la **révision PCB Phase 3G**. Elle ne remplace pas
le prototype perfboard gelé dans `hardware/kcan-rxonly/`.

Le prototype Phase 3G est exclusivement destiné au banc. Il emploie un
TJA1055T/3 pour observer un bus ISO 11898-3 à 100 kbit/s et conserve une
coupure TX physique : la sortie `U2.Y` se termine sur `TP_GATE_Y`; la broche
`U1.TXD` est uniquement tirée au niveau récessif par `R_TXD`. Il n'existe sur
le PCB ni composant `R_LINK_TX`, ni piste, ni via, ni zone reliant ces deux
nets.

## Alimentation

```text
J_BAT 10..18 V banc limite en courant
  -> F_BAT -> D_BAT -> BAT_PROTECTED
       |-> R_BAT -> BAT_LOCAL -> U1 BAT et WAKE
       `-> U3 TLS715B0EJV50 -> 5V_TJA -> U1 VCC

USB -> ESP32-S3-DevKitC-1-N8R8
ESP32 3V3 -> U2 et pull-ups RXD/ERR
GND commun uniquement
```

La broche 5 V du DevKit est `NC`. Elle ne possède ni piste, ni pad fonctionnel
sur le domaine `5V_TJA`. Voir [power-budget.md](power-budget.md).

## Livrables

- `BOM_PHASE3G.csv` : nomenclature électrique de la PCBA ;
- `procurement_PHASE3G.csv` : sourcing sans substitution silencieuse ;
- `netlist_PHASE3G.csv` : connectivité normative lisible et testable ;
- `pin-review.csv` : revue broche à broche ;
- `kicad/` : projet, schéma et PCB KiCad 10 ;
- `manufacturing/` : exports de revue fabricant, pas un bon de commande.

Le projet KiCad est généré de manière reproductible par
`tools/generate_phase3g_kicad.py`. Les exports se régénèrent avec
`tools/generate_phase3g_manufacturing.ps1`, qui refuse de continuer si ERC ou
DRC échoue. Les règles et résultats de revue sont détaillés dans
`docs/phase3g-pcb-manufacturing.md`.

Les fichiers de fabrication sont des **artefacts de revue**. Ils n'autorisent
ni commande, ni connexion BMW. Les essais électriques de
`docs/phase3g-pcb-bringup.md` restent obligatoires.
